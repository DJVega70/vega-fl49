#pragma once
#include "DawUI.h"
// Included from MixerView.cpp after the xl:: hardware parts (uses xl::Fader for the master fader).

/** Master channel strip panel (classic console style): EQ column, centre meters / trims / master fader,
    dynamics column. Drawn on a fixed 460 x 880 canvas and scaled to the mixer height. */
namespace console
{
const juce::Colour panelTop { 0xff303030 }, panelBot { 0xff2a2a2a }, ink { 0xffd8d8d8 }, inkDim { 0xff8e8e8e }, line { 0xff3e3e3e };
const juce::Colour red { 0xffff6a5a }, green { 0xff5ee08a }, blue { 0xff6fa8ff }, black { 0xffffffff }, grey { 0xffffffff };   // pointer colours

/** Console knob: grey skirt with a coloured cap, ring of scale dots and a white pointer */
class Knob : public juce::Slider
{
public:
    explicit Knob (juce::Colour cap = black) : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox), capColour (cap)
    {
        setPopupDisplayEnabled (true, true, nullptr);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
    juce::Colour capColour;
    void paint (juce::Graphics& g) override
    {
        // the plug-ins' knob and ticks; the band colour lives in the pointer
        const auto rp = getRotaryParameters();
        auto b = getLocalBounds().toFloat();
        const float d = std::min (b.getWidth(), b.getHeight()) - 20.0f;
        auto k = juce::Rectangle<float> (d, d).withCentre (b.getCentre());
        juce::ignoreUnused (rp);
        const float v = (float) valueToProportionOfLength (getValue());
        const float a = vegacontrols::wheelStart + v * (vegacontrols::wheelEnd - vegacontrols::wheelStart);
        vegacontrols::paintWheel (g, k.expanded (9.0f), a, v, isMouseOverOrDragging(), isMouseButtonDown(), capColour);
    }
};

/** Square console switch: grey when off, warm lamp glow when on */
class Lamp : public juce::Button
{
public:
    Lamp() : juce::Button ({}) { setClickingTogglesState (true); }
    bool red = false;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        vegapaint::squareButton (g, r, getToggleState(), over, red ? juce::Colour (0xffff5a5a) : VegaColours::accent);
    }
};

/** Big momentary-style latch button (CUT) */
class BigButton : public juce::Button
{
public:
    explicit BigButton (const juce::String& t) : juce::Button (t) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto face = vegapaint::key (g, getLocalBounds().toFloat().reduced (3.0f), getToggleState(), over, down, juce::Colour (0xffff5a5a));
        g.setColour (getToggleState() ? vegapaint::keyInk (true) : juce::Colour (0xffff5a5a));
        g.setFont (VegaFonts::ui (14.0f, true)); g.drawText (getButtonText(), face, juce::Justification::centred);
    }
};
} // namespace console

class MasterSection : public juce::Component, private juce::Timer
{
public:
    static constexpr int CW = 460, CH = 880;
    using M = VegaMasterStripPlugin;

    explicit MasterSection (ViewState& v) : vs (v)
    {
        plugin = vs.session.masterStrip();
        addAndMakeVisible (canvas);
        canvas.setSize (CW, CH);
        canvas.paintFn = [this] (juce::Graphics& g) { paintCanvas (g); };

        // ---- EQ column
        knob (M::lpf, console::grey, 26, 22);       knob (M::hpf, console::grey, 82, 66);
        lamp (M::hfBell, 106, 146);
        knob (M::hfGain, console::red, 26, 168);     knob (M::hfFreq, console::red, 82, 214);
        knob (M::hmfGain, console::green, 26, 290);  knob (M::hmfFreq, console::green, 82, 324);  knob (M::hmfQ, console::green, 26, 366);
        lamp (M::eqOn, 106, 438);
        knob (M::lmfGain, console::blue, 26, 486);   knob (M::lmfFreq, console::blue, 82, 520);   knob (M::lmfQ, console::blue, 26, 562);
        knob (M::lfFreq, console::black, 82, 664);   knob (M::lfGain, console::black, 26, 712);   lamp (M::lfBell, 106, 778);
        // ---- centre
        knob (M::inGain, console::red, 168, 346, 58);  knob (M::outGain, console::grey, 236, 346, 58);
        // ---- dynamics column
        lamp (M::dynOn, 420, 18); lamp (M::compFast, 420, 52); lamp (M::compPeak, 420, 82);
        knob (M::compRatio, console::black, 322, 108); knob (M::compThresh, console::black, 384, 150); knob (M::compRelease, console::black, 322, 196);
        knob (M::gateRange, console::black, 322, 400); knob (M::gateThresh, console::black, 384, 442); knob (M::gateRelease, console::black, 322, 488); knob (M::gateHold, console::black, 384, 530);
        lamp (M::gateExpand, 322, 594); lamp (M::gateFast, 322, 626);
        lamp (M::polarity, 322, 760);
        cutBtn.onClick = [this] { if (auto* p = strip()) p->set (M::cut, cutBtn.getToggleState() ? 1.0f : 0.0f); };
        cutBtn.setTooltip ("Cut (mute) the master output");
        canvas.addAndMakeVisible (cutBtn); cutBtn.setBounds (370, 804, 70, 52);
        power.setTooltip ("Master strip on / off (bypass)");
        power.onClick = [this] { if (auto* p = strip()) p->setEnabled (power.getToggleState()); };
        canvas.addAndMakeVisible (power); power.setBounds (160, 8, 26, 26);

        // ---- master fader (master volume)
        fader.setRange (-60.0, 6.0, 0.1); fader.setSkewFactorFromMidPoint (-10.0); fader.setDoubleClickReturnValue (true, 0.0);
        fader.setTextValueSuffix (" dB");
        fader.onValueChange = [this] { if (auto vp = vs.edit().getMasterVolumePlugin()) vp->setVolumeDb ((float) fader.getValue()); };
        canvas.addAndMakeVisible (fader); fader.setBounds (196, 470, 70, 330);
        fader.capLine = VegaColours::accent;
        startTimerHz (30);
        refresh();
    }

    void resized() override
    {
        const float s = (float) getHeight() / CH;
        canvas.setTransform (juce::AffineTransform::scale (s));
    }
    int widthForHeight (int h) const { return juce::roundToInt (CW * (float) h / CH); }
    void refresh()
    {
        auto* p = strip();
        if (p == nullptr) return;
        for (auto& b : binds)
        {
            const float v = p->get (b.param);
            if (b.knob != nullptr && ! b.knob->isMouseButtonDown() && std::abs (b.knob->getValue() - v) > 1.0e-4) b.knob->setValue (v, juce::dontSendNotification);
            if (b.lamp != nullptr && b.lamp->getToggleState() != (v > 0.5f)) b.lamp->setToggleState (v > 0.5f, juce::dontSendNotification);
        }
        if (cutBtn.getToggleState() != (p->get (M::cut) > 0.5f)) cutBtn.setToggleState (p->get (M::cut) > 0.5f, juce::dontSendNotification);
        if (power.getToggleState() != p->isEnabled()) power.setToggleState (p->isEnabled(), juce::dontSendNotification);
        if (auto vp = vs.edit().getMasterVolumePlugin()) if (! fader.isMouseButtonDown()) fader.setValue (vp->getVolumeDb(), juce::dontSendNotification);
    }

private:
    struct Canvas : juce::Component
    {
        std::function<void (juce::Graphics&)> paintFn;
        void paint (juce::Graphics& g) override { if (paintFn) paintFn (g); }
    };
    struct Bind { int param; console::Knob* knob = nullptr; console::Lamp* lamp = nullptr; };

    VegaMasterStripPlugin* strip() const { return dynamic_cast<VegaMasterStripPlugin*> (plugin.get()); }

    void knob (int param, juce::Colour cap, int x, int y, int size = 52)
    {
        auto* k = knobs.add (new console::Knob (cap));
        const auto& d = M::def (param);
        k->setRange (d.lo, d.hi, 0.0);
        if (d.mid != 0 && d.mid > d.lo && d.mid < d.hi) k->setSkewFactorFromMidPoint (d.mid);
        k->setDoubleClickReturnValue (true, d.def);
        k->textFromValueFunction = [param] (double v) { return M::text (param, (float) v); };
        k->onValueChange = [this, param, k] { if (auto* p = strip()) p->set (param, (float) k->getValue()); };
        k->setTooltip (juce::String (d.name) + " (double-click to reset)");
        canvas.addAndMakeVisible (k); k->setBounds (x, y, size, size);
        binds.push_back ({ param, k, nullptr });
    }
    void lamp (int param, int x, int y)
    {
        auto* l = lamps.add (new console::Lamp());
        l->onClick = [this, param, l] { if (auto* p = strip()) p->set (param, l->getToggleState() ? 1.0f : 0.0f); };
        l->setTooltip (M::def (param).name);
        canvas.addAndMakeVisible (l); l->setBounds (x, y, 26, 26);
        binds.push_back ({ param, nullptr, l });
    }

    void timerCallback() override
    {
        refresh();
        if (auto* p = strip())
        {
            for (int c = 0; c < 2; ++c)
            {
                const float in = juce::Decibels::gainToDecibels (p->meterIn[c].exchange (0.0f), -100.0f), out = juce::Decibels::gainToDecibels (p->meterOut[c].exchange (0.0f), -100.0f);
                lvlIn[c] = std::max (in, lvlIn[c] - 1.2f); lvlOut[c] = std::max (out, lvlOut[c] - 1.2f);
                peakHoldOut[c] = lvlOut[c] >= peakHoldOut[c] ? lvlOut[c] : peakHoldOut[c] - 0.25f;
            }
            const float gr = p->grComp.exchange (0.0f), gg = p->grGate.exchange (0.0f);
            grC = std::max (gr, grC - 1.0f); grG = std::max (gg, grG - 1.5f);
        }
        canvas.repaint();
    }

    // ---- drawing
    static void label (juce::Graphics& g, const juce::String& t, int x, int y, int w, float size = 13.0f, bool bold = true, juce::Justification j = juce::Justification::centredLeft)
    {
        g.setFont (VegaFonts::ui (size, bold)); g.setColour (console::ink); g.drawText (t, x, y, w, 18, j);
    }
    static void ends (juce::Graphics& g, juce::Rectangle<int> k, const juce::String& lo, const juce::String& hi, const juce::String& unit = {})
    {
        g.setFont (VegaFonts::ui (9.5f, true)); g.setColour (console::inkDim);
        g.drawText (lo, k.getX() - 14, k.getBottom() - 10, 26, 11, juce::Justification::centred);
        g.drawText (hi, k.getRight() - 12, k.getBottom() - 10, 26, 11, juce::Justification::centred);
        if (unit.isNotEmpty()) { g.setColour (console::ink); g.setFont (VegaFonts::ui (10.5f, true)); g.drawText (unit, k.getX(), k.getBottom() - 2, k.getWidth(), 13, juce::Justification::centred); }
    }
    juce::Rectangle<int> kb (int param) const { for (auto& b : binds) if (b.param == param && b.knob) return b.knob->getBounds(); return {}; }

    void paintCanvas (juce::Graphics& g)
    {
        using namespace console;
        // side panels
        for (auto x : { 0, 310 })
        {
            g.setGradientFill (juce::ColourGradient (panelTop, (float) x, 0, panelBot, (float) x, (float) CH, false));
            g.fillRect (x, 0, 150, CH);
        }
        g.setColour (VegaColours::line); g.fillRect (149, 0, 1, CH); g.fillRect (310, 0, 1, CH);
        // centre
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262626), 150, 0, juce::Colour (0xff1e1e1e), 150, (float) CH, false));
        g.fillRect (150, 0, 160, CH);
        g.setColour (VegaColours::bar); g.fillRect (151, 0, 158, 42);
        g.setColour (VegaColours::line); g.fillRect (151, 42, 158, 1);
        g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (15.0f, true));
        g.drawText ("Master Strip", 190, 10, 116, 22, juce::Justification::centredLeft);

        // ---- EQ column labels
        g.setColour (line);
        for (int y : { 136, 280, 474, 652 }) g.fillRect (12, y, 126, 2);
        label (g, "LP", 86, 26, 40); label (g, "HP", 32, 92, 40);
        ends (g, kb (M::lpf), "OUT", "3.0", "kHz"); ends (g, kb (M::hpf), "OUT", "500", "Hz");
        label (g, "HF", 106, 176, 40); ends (g, kb (M::hfGain), "-", "+", "dB"); ends (g, kb (M::hfFreq), "1.5", "22", "kHz");
        g.setFont (VegaFonts::ui (9.0f, true)); g.setColour (inkDim); g.drawText ("BELL", 78, 152, 28, 14, juce::Justification::centredRight);
        label (g, "HMF", 92, 390, 50); ends (g, kb (M::hmfGain), "-", "+", "dB"); ends (g, kb (M::hmfFreq), "0.6", "7.0", "kHz"); ends (g, kb (M::hmfQ), "^", "^", "Q");
        g.setColour (line); g.drawRect (90, 430, 52, 40, 1);
        label (g, "EQ", 64, 442, 40, 15.0f);
        label (g, "LMF", 92, 590, 50); ends (g, kb (M::lmfGain), "-", "+", "dB"); ends (g, kb (M::lmfFreq), "0.2", "2.0", "kHz"); ends (g, kb (M::lmfQ), "^", "^", "Q");
        label (g, "LF", 106, 742, 40); ends (g, kb (M::lfFreq), "40", "600", "Hz"); ends (g, kb (M::lfGain), "-", "+", "dB");
        g.setFont (VegaFonts::ui (9.0f, true)); g.setColour (inkDim); g.drawText ("BELL", 76, 784, 28, 14, juce::Justification::centredRight);

        // ---- meters
        auto meter = [&] (juce::Rectangle<int> r, float lvl, float hold)
        {
            g.setColour (juce::Colour (0xff141414)); g.fillRoundedRectangle (r.toFloat(), 3.0f);
            vegapaint::ledColumn (g, r.toFloat().reduced (6.0f, 4.0f), lvl, hold);
        };
        const int mTop = 54, mH = 268;
        meter ({ 182, mTop, 40, mH }, lvlOut[0], peakHoldOut[0]); meter ({ 238, mTop, 40, mH }, lvlOut[1], peakHoldOut[1]);
        // thin input bars beside the output meters
        for (int c = 0; c < 2; ++c)
        {
            juce::Rectangle<int> r (c == 0 ? 225 : 281, mTop, 5, mH);
            g.setColour (juce::Colour (0xff141414)); g.fillRect (r);
            const float f = juce::jlimit (0.0f, 1.0f, (lvlIn[c] + 60.0f) / 66.0f);
            g.setColour (VegaColours::accentDim); g.fillRect (r.withTop (r.getBottom() - (int) (f * r.getHeight())));
        }
        g.setFont (VegaFonts::ui (9.5f, true)); g.setColour (VegaColours::textDim);
        for (int db : { 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60 })
        {
            const float y = mTop + mH - 2 - (60.0f - db) / 66.0f * (mH - 4);
            g.drawText (juce::String (db), 154, (int) y - 6, 24, 12, juce::Justification::centredRight);
        }
        g.setColour (ink); g.setFont (VegaFonts::ui (11.0f, true));
        g.drawText ("INPUT", 160, 404, 74, 14, juce::Justification::centred); g.drawText ("OUTPUT", 228, 404, 74, 14, juce::Justification::centred);
        // 7-segment-style display
        vegapaint::screen (g, { 164.0f, 428.0f, 132.0f, 32.0f });
        g.setFont (VegaFonts::mono (22.0f)); g.setColour (VegaColours::accent); g.drawText ("MST", 170, 430, 52, 28, juce::Justification::centredLeft);
        g.setColour (ink); g.setFont (VegaFonts::ui (15.0f, true)); g.drawText ("Master", 222, 430, 74, 28, juce::Justification::centredLeft);
        // fader scale + value
        auto fb = fader.getBounds();
        g.setFont (VegaFonts::ui (9.0f, true)); g.setColour (inkDim);
        for (double db : { 6.0, 0.0, -5.0, -10.0, -20.0, -30.0, -40.0, -60.0 })
        {
            const int y = fb.getY() + (int) fader.getPositionOfValue (db);
            g.fillRect (fb.getX() - 14, y, 10, 1); g.drawText (db > 0 ? "+" + juce::String ((int) db) : juce::String ((int) db), fb.getX() - 44, y - 6, 28, 12, juce::Justification::centredRight);
        }
        vegapaint::screen (g, { 180.0f, 804.0f, 100.0f, 20.0f });
        g.setFont (VegaFonts::mono (12.0f)); g.setColour (VegaColours::text);
        g.drawText (juce::String (fader.getValue(), 1) + " dB", 170, 806, 120, 16, juce::Justification::centred);
        g.setColour (inkDim); g.setFont (VegaFonts::ui (11.0f, true));
        g.drawText ("Vega Productions", 150, 836, 160, 14, juce::Justification::centred);
        g.setFont (VegaFonts::ui (9.5f)); g.drawText ("MASTER SECTION", 150, 852, 160, 12, juce::Justification::centred);

        // ---- dynamics column
        label (g, "DYNAMICS", 322, 22, 96, 15.0f);
        g.setFont (VegaFonts::ui (10.0f, true)); g.setColour (inkDim);
        g.drawText ("FAST ATTACK", 322, 56, 94, 16, juce::Justification::centredRight); g.drawText ("PEAK", 322, 86, 94, 16, juce::Justification::centredRight);
        label (g, "RATIO", 318, 158, 60, 11.0f); ends (g, kb (M::compRatio), "1", juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")));
        label (g, "THRESHOLD", 374, 200, 80, 11.0f); ends (g, kb (M::compThresh), "-30", "+10");
        label (g, "RELEASE", 314, 246, 70, 11.0f); ends (g, kb (M::compRelease), ".05", "4");
        g.setColour (line); g.fillRect (322, 364, 126, 2);
        label (g, "COMP", 322, 300, 60, 13.0f); label (g, "GATE", 322, 372, 60, 13.0f);
        // gain-reduction ladders
        static const int steps[] { 20, 14, 10, 6, 3 };
        for (int i = 0; i < 5; ++i)
        {
            const float y = 266.0f + i * 18.0f;
            const bool cOn = grC >= steps[i], gOn = grG >= steps[i] * 2;
            const auto cc = i < 2 ? juce::Colour (0xffff5a5a) : i < 4 ? juce::Colours::white : VegaColours::accent;
            for (auto [x, on, col] : { std::tuple<float, bool, juce::Colour> { 392.0f, cOn, cc }, { 412.0f, gOn, VegaColours::accent } })
            {
                g.setColour (on ? col : juce::Colour (0xff3a3a3a)); g.fillRect (x, y + 2, 12.0f, 6.0f);
            }
            g.setFont (VegaFonts::ui (9.0f, true)); g.setColour (inkDim); g.drawText (juce::String (steps[i]), 426, (int) y - 1, 20, 12, juce::Justification::centredLeft);
        }
        label (g, "RANGE", 318, 450, 60, 11.0f); ends (g, kb (M::gateRange), "0", "40");
        label (g, "THRESHOLD", 374, 492, 80, 11.0f); ends (g, kb (M::gateThresh), "-80", "0");
        label (g, "RELEASE", 314, 538, 70, 11.0f); ends (g, kb (M::gateRelease), ".05", "4");
        label (g, "HOLD", 384, 580, 52, 11.0f); ends (g, kb (M::gateHold), "0", "4");
        g.setFont (VegaFonts::ui (10.0f, true)); g.setColour (inkDim);
        g.drawText ("EXPAND", 352, 598, 80, 16, juce::Justification::centredLeft); g.drawText ("FAST ATTACK", 352, 630, 90, 16, juce::Justification::centredLeft);
        g.setColour (line); g.fillRect (322, 668, 126, 2);
        // badge
        g.setColour (ink); g.drawEllipse (330.0f, 690.0f, 26.0f, 26.0f, 2.5f); g.fillEllipse (338.0f, 698.0f, 10.0f, 10.0f);
        g.setFont (VegaFonts::ui (14.0f, true)); g.drawText ("VEGA", 364, 686, 80, 16, juce::Justification::centredLeft);
        g.setFont (VegaFonts::ui (11.0f)); g.drawText ("MASTER", 364, 702, 80, 14, juce::Justification::centredLeft);
        g.setFont (VegaFonts::ui (12.0f, true)); g.setColour (inkDim);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x98")), 352, 764, 40, 18, juce::Justification::centredLeft);
        g.setFont (VegaFonts::ui (9.5f, true)); g.drawText ("POLARITY", 322, 786, 60, 12, juce::Justification::centredLeft);
    }

    ViewState& vs;
    te::Plugin::Ptr plugin;
    Canvas canvas;
    juce::OwnedArray<console::Knob> knobs; juce::OwnedArray<console::Lamp> lamps;
    std::vector<Bind> binds;
    console::BigButton cutBtn { "CUT" };
    console::Lamp power;
    xl::Fader fader;
    float lvlIn[2] { -100, -100 }, lvlOut[2] { -100, -100 }, peakHoldOut[2] { -100, -100 }, grC = 0, grG = 0;
};
