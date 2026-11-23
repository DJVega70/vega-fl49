#include "MixerView.h"

//==============================================================================
// Hardware-style parts
namespace xl
{
const juce::Colour face { 0xff1d1d1f }, faceLight { 0xff242427 }, keyFace { 0xff202023 }, keyEdge { 0xff38383c }, slot { 0xff0a0a0b };
const juce::Colour rowLed[4] { juce::Colour (0xffe35ce8), juce::Colour (0xff1ea9ee), juce::Colour (0xff17dca2), juce::Colour (0xffffb23a) };
// pad LED colours run pink -> violet -> blue -> cyan -> green across the channels, like the hardware
inline juce::Colour columnColour (int i)
{
    static const juce::uint32 c[] { 0xffe35ce8, 0xffb183f2, 0xff7fa2f7, 0xff67cdf7, 0xff52dcef, 0xff31e3e6, 0xff2fe2d2, 0xff22dfa8 };
    return juce::Colour (c[((i % 8) + 8) % 8]);
}

inline void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float lit)
{
    if (lit > 0.5f)
    {
        g.setColour (c.withAlpha (0.18f)); g.fillRoundedRectangle (r.expanded (5.0f, 4.0f), 4.0f);
        g.setColour (c.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (2.0f, 1.5f), 2.5f);
    }
    g.setColour (lit > 0.5f ? c : c.withMultipliedBrightness (0.35f).withAlpha (0.55f));
    g.fillRoundedRectangle (r, 1.0f);
    if (lit > 0.5f) { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.fillRect (r.withHeight (1.0f).reduced (r.getWidth() * 0.15f, 0)); }
}

inline void drawKeyBox (juce::Graphics& g, juce::Rectangle<float> r, bool down = false)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.translated (0, 2.0f), 5.0f);
    g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xff1a1a1c) : juce::Colour (0xff27272a), 0, r.getY(), juce::Colour (0xff1b1b1d), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (keyEdge); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
}

/** Rotary knob: soft-touch dark cap with a white pointer */
class Knob : public juce::Slider
{
public:
    // drawn by VegaLookAndFeel::drawRotarySlider - the same machined knob and tick ring as the plug-ins
    Knob() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
    {
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (200);
        setPopupDisplayEnabled (true, true, nullptr);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
};

/** Long-throw fader: black slot, chunky cap with a white line */
class Fader : public juce::Slider
{
public:
    Fader() : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox)
    {
        setSliderSnapsToMousePosition (false);
        setPopupDisplayEnabled (true, true, nullptr);
    }
    static constexpr float capW = 34.0f, capH = 44.0f;

    juce::Colour capLine = juce::Colours::white;
    bool lit = false;
    void paint (juce::Graphics& g) override
    {
        // the plug-ins' fader (ticks both sides, thin slot, dark cap with a white line)
        // our own value -> position map, so the tall cap stays inside the slot at both ends
        auto area = getLocalBounds().toFloat();
        const float y = juce::jmap ((float) valueToProportionOfLength (getValue()), area.getBottom() - capH * 0.5f, area.getY() + capH * 0.5f);
        vegapaint::hardwareFader (g, area, y, isMouseOverOrDragging(), lit || capLine != juce::Colours::white, capW, capH);
    }
};

/** Compact horizontal pan control: slot with a centre notch and a small cap */
class PanBar : public juce::Slider
{
public:
    PanBar() : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
    {
        setRange (-1.0, 1.0, 0.01); setDoubleClickReturnValue (true, 0.0);
        setSliderSnapsToMousePosition (false); setPopupDisplayEnabled (true, true, nullptr);
        textFromValueFunction = [] (double v) { return std::abs (v) < 0.01 ? juce::String ("Pan C") : "Pan " + juce::String (juce::roundToInt (std::abs (v) * 100)) + (v < 0 ? " L" : " R"); };
    }
    void paint (juce::Graphics& g) override
    {
        const float cy = getHeight() * 0.5f;
        auto slotR = juce::Rectangle<float> (8.0f, cy - 2.0f, getWidth() - 16.0f, 4.0f);
        g.setColour (slot); g.fillRoundedRectangle (slotR, 2.0f);
        g.setColour (juce::Colour (0xff8a8a8a)); g.fillRect (getWidth() * 0.5f - 0.5f, cy - 6.0f, 1.0f, 12.0f);
        const float x = (float) getPositionOfValue (getValue());
        const float cx = getWidth() * 0.5f;
        g.setColour (rowLed[2].withAlpha (0.7f)); g.fillRect (juce::Rectangle<float> (std::min (x, cx), cy - 1.0f, std::abs (x - cx), 2.0f));
        auto cap = juce::Rectangle<float> (12.0f, 14.0f).withCentre ({ x, cy });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4e), 0, cap.getY(), juce::Colour (0xff252528), 0, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (juce::Colour (0xffe6e6e6)); g.fillRect (x - 0.75f, cap.getY() + 3, 1.5f, cap.getHeight() - 6);
    }
};

/** Pad with an LED strip: dim when off, glowing when on */
class Pad : public juce::Button
{
public:
    Pad() : juce::Button ({}) {}
    juce::Colour led { 0xff48e1e8 };
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (down ? juce::Colour (0xff141416) : over ? juce::Colour (0xff1d1d20) : juce::Colour (0xff18181a));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xff2c2c30)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        drawLed (g, juce::Rectangle<float> (r.getWidth() * 0.42f, 4.0f).withCentre (r.getCentre()), led, getToggleState() ? 1.0f : 0.0f);
    }
};

/** Small hardware key with a drawn symbol */
class Key : public juce::Button
{
public:
    enum Shape { up, down, left, right, rec, play, text };
    Key (Shape s, juce::String t = {}) : juce::Button (t), shape (s) {}
    Shape shape; juce::Colour on { 0xffffffff };
    void paintButton (juce::Graphics& g, bool isOver, bool isDown) override
    {
        auto r = getLocalBounds().toFloat();
        if (isDown) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (r.reduced (2.0f), 3.0f); }
        const auto c = r.getCentre(); const float s = std::min (r.getWidth(), r.getHeight()) * 0.22f;
        const auto ink = juce::Colour (0xffe8e8e8).withAlpha (isOver ? 1.0f : 0.85f);
        juce::Path p;
        switch (shape)
        {
            case up:    p.startNewSubPath (c.x - s, c.y + s * 0.5f); p.lineTo (c.x, c.y - s * 0.5f); p.lineTo (c.x + s, c.y + s * 0.5f); break;
            case down:  p.startNewSubPath (c.x - s, c.y - s * 0.5f); p.lineTo (c.x, c.y + s * 0.5f); p.lineTo (c.x + s, c.y - s * 0.5f); break;
            case left:  p.startNewSubPath (c.x + s * 0.5f, c.y - s); p.lineTo (c.x - s * 0.5f, c.y); p.lineTo (c.x + s * 0.5f, c.y + s); break;
            case right: p.startNewSubPath (c.x - s * 0.5f, c.y - s); p.lineTo (c.x + s * 0.5f, c.y); p.lineTo (c.x - s * 0.5f, c.y + s); break;
            case rec:
            {
                const auto col = juce::Colour (0xffff3355);
                if (getToggleState()) { g.setColour (col.withAlpha (0.3f)); g.fillEllipse (juce::Rectangle<float> (s * 3.4f, s * 3.4f).withCentre (c)); }
                g.setColour (getToggleState() ? col : col.withMultipliedBrightness (0.75f)); g.fillEllipse (juce::Rectangle<float> (s * 2.3f, s * 2.3f).withCentre (c));
                return;
            }
            case play:
            {
                const auto col = juce::Colour (0xff22dd66);
                juce::Path t; t.addTriangle (c.x - s * 0.9f, c.y - s * 1.1f, c.x - s * 0.9f, c.y + s * 1.1f, c.x + s * 1.1f, c.y);
                if (getToggleState()) { g.setColour (col.withAlpha (0.3f)); g.strokePath (t, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved)); }
                g.setColour (getToggleState() ? col : col.withMultipliedBrightness (0.8f)); g.fillPath (t);
                return;
            }
            case text:
                g.setColour (getToggleState() ? on : ink); g.setFont (VegaFonts::ui (12.5f, getToggleState()));
                g.drawText (getButtonText(), r, juce::Justification::centred);
                return;
        }
        g.setColour (ink); g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};
} // namespace xl

#include "MasterSection.h"

//==============================================================================
// Channel-strip parts, drawn with the plug-ins' painters (vegapaint)
namespace cs
{
/** Combo-box looking button: shows a value, opens a menu (onClick) */
class ComboButton : public juce::Button
{
public:
    ComboButton() : juce::Button ({}) {}
    juce::Colour ink = VegaColours::textDim;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat();
        vegapaint::comboField (g, r, over);
        g.setColour (ink); g.setFont (VegaFonts::ui (10.5f));
        g.drawFittedText (getButtonText(), r.withTrimmedRight (14).withTrimmedLeft (4).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
};

/** Insert slot drawn as the plug-ins' black display: cyan name, dim when bypassed, "+" when empty */
class ScreenButton : public juce::Button
{
public:
    explicit ScreenButton (const juce::String& t = {}) : juce::Button (t) {}
    juce::Colour ink = VegaColours::accent; bool empty = false;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        vegapaint::screen (g, r);
        if (over) { g.setColour (juce::Colours::white.withAlpha (down ? 0.08f : 0.04f)); g.fillRoundedRectangle (r, 3.0f); }
        if (empty) { g.setColour (juce::Colour (over ? 0xff8e8e8e : 0xff4a4a4a)); g.setFont (VegaFonts::ui (15.0f, true)); g.drawText ("+", r, juce::Justification::centred); return; }
        g.setColour (ink); g.setFont (VegaFonts::mono (10.5f));
        g.drawFittedText (getButtonText(), r.reduced (4, 0).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    }
};

/** The plug-ins' square push-button with its label underneath */
class SqButton : public juce::Button
{
public:
    SqButton (const juce::String& label, juce::Colour litColour) : juce::Button (label), lit (litColour) {}
    juce::Colour lit;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto area = getLocalBounds().toFloat();
        const float sz = std::min (area.getWidth() - 4.0f, area.getHeight() - 15.0f);
        auto sq = juce::Rectangle<float> (sz, sz).withCentre ({ area.getCentreX(), area.getY() + sz * 0.5f + 2.0f });
        vegapaint::squareButton (g, sq, getToggleState(), over, lit);
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.5f));
        g.drawText (getButtonText(), area.withTop (sq.getBottom() + 2.0f).toNearestInt(), juce::Justification::centredTop);
    }
};
} // namespace cs

namespace mixlayout
{
constexpr int title = 12, combo = 20, gap = 5, eqRow = 46, panRow = 54, sqRow = 38, nameH = 30, dbH = 18, vuH = 40;
}

//==============================================================================
class MixerView::Strip : public juce::Component
{
public:
    static constexpr int W = 120;
    te::AudioTrack* getTrack() const { return track.get(); }
    Strip (MixerView& o, ViewState& v, te::AudioTrack* t, int column) : owner (o), vs (v), track (t), col (column)
    {
        fader.setRange (-60.0, 6.0, 0.1); fader.setSkewFactorFromMidPoint (-10.0);
        fader.setTextValueSuffix (" dB"); fader.setDoubleClickReturnValue (true, 0.0);
        fader.onValueChange = [this] { if (auto vp = volume()) vp->setVolumeDb ((float) fader.getValue()); touch ("Volume", juce::String (fader.getValue(), 1) + " dB"); repaint (dbArea); };
        // gestures let automation WRITE mode record these moves
        fader.onDragStart = [this] { if (track != nullptr && vs.selectedTrack != track->itemID) vs.selectTrack (track.get()); if (auto vp = volume()) dawui::gesture (vp->volParam.get(), true); };
        fader.onDragEnd   = [this] { if (auto vp = volume()) dawui::gesture (vp->volParam.get(), false); };
        pan.setRange (-1.0, 1.0, 0.01); pan.setDoubleClickReturnValue (true, 0.0);
        pan.textFromValueFunction = [] (double v) { return std::abs (v) < 0.01 ? juce::String ("C") : juce::String (juce::roundToInt (std::abs (v) * 100)) + (v < 0 ? " L" : " R"); };
        pan.onDragStart = [this] { if (auto vp = volume()) dawui::gesture (vp->panParam.get(), true); };
        pan.onDragEnd   = [this] { if (auto vp = volume()) dawui::gesture (vp->panParam.get(), false); };
        pan.onValueChange = [this] { if (auto vp = volume()) vp->setPan ((float) pan.getValue()); touch ("Pan", pan.getTextFromValue (pan.getValue())); repaint(); };
        pan.setTooltip ("Pan (double-click to centre)");
        for (int i = 0; i < 4; ++i)
        {
            knobs[i].onDragStart = [this, i] { if (auto* d = dj()) dawui::gesture (i < 3 ? d->band[i].get() : d->filter.get(), true); };
            knobs[i].onDragEnd   = [this, i] { if (auto* d = dj()) dawui::gesture (i < 3 ? d->band[i].get() : d->filter.get(), false); };
        }
        // HI / MID / LOW isolator knobs: 12 o'clock = 0 dB, fully left = kill
        static const char* bandName[] { "Hi", "Mid", "Low" };
        for (int i = 0; i < 3; ++i)
        {
            knobs[i].setRange (VegaDJEQPlugin::killDb, VegaDJEQPlugin::maxDb, 0.1); knobs[i].setSkewFactorFromMidPoint (0.0);
            knobs[i].setDoubleClickReturnValue (true, 0.0);
            knobs[i].textFromValueFunction = [] (double v) { return v <= VegaDJEQPlugin::killDb + 0.05 ? juce::String ("KILL") : (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
            knobs[i].onValueChange = [this, i]
            {
                if (auto* d = dj()) d->band[i]->setParameter ((float) knobs[i].getValue(), juce::sendNotification);
                touch (bandName[i], knobs[i].getTextFromValue (knobs[i].getValue()));
                repaint();
            };
        }
        // DJ filter: left = low-pass, right = high-pass, centre = off
        knobs[3].setRange (-1.0, 1.0, 0.001); knobs[3].setDoubleClickReturnValue (true, 0.0);
        knobs[3].textFromValueFunction = [] (double v) { return VegaDJEQPlugin::describeFilter ((float) v); };
        knobs[3].onValueChange = [this]
        {
            if (auto* d = dj()) d->filter->setParameter ((float) knobs[3].getValue(), juce::sendNotification);
            touch ("Filter", knobs[3].getTextFromValue (knobs[3].getValue()));
            repaint();
        };
        knobs[0].setTooltip ("HI - highs above 2.5 kHz (turn fully left to kill)");
        knobs[1].setTooltip ("MID - 250 Hz to 2.5 kHz (turn fully left to kill)");
        knobs[2].setTooltip ("LOW - lows below 250 Hz (turn fully left to kill)");
        knobs[3].setTooltip ("FILTER - left: low-pass, right: high-pass, centre: off (double-click to reset)");
        addAndMakeVisible (pan);
        addAndMakeVisible (fader);
        for (auto& k : knobs) addAndMakeVisible (k);
        if (track != nullptr)
        {
            rec.setTooltip ("Record arm"); solo.setTooltip ("Solo"); mute.setTooltip ("Mute");
            rec.onClick = [this] { dawui::toggleArm (*track); owner.touched (track->getName(), dawui::isArmed (*track) ? "Armed" : "Disarmed"); };
            solo.onClick = [this] { track->setSolo (! track->isSolo (false)); owner.touched (track->getName(), track->isSolo (false) ? "Solo On" : "Solo Off"); };
            mute.onClick = [this] { track->setMute (! track->isMuted (false)); owner.touched (track->getName(), track->isMuted (false) ? "Muted" : "Unmuted"); };
            for (auto* b : { &rec, &solo, &mute }) addAndMakeVisible (b);
            output.onChange = [this] { const int i = output.getSelectedItemIndex(); vs.session.routeToBus (*track, i <= 0 ? nullptr : outBuses[i - 1]); };
            output.setTooltip ("Output");
            addAndMakeVisible (output);
            input.onClick = [this] { inputMenu(); }; input.setTooltip ("Input: what this track records and plays live");
            tape.onClick = [this] { tapeMenu(); }; tape.setTooltip ("Tape: Off, 15 ips (warm, softer top) or 30 ips (clean, a little glue)");
            autoMode.onClick = [this] { automationMenu(); }; autoMode.setTooltip ("Automation: Read plays it back, Write records your moves while playing");
            for (auto* b : { &input, &tape, &autoMode }) addAndMakeVisible (b);
            if (DawSession::kindOf (*track) == DawSession::TrackKind::midi)
            {
                instrument.onClick = [this]
                {
                    auto* first = track->pluginList[0];
                    if (first != nullptr && first->isSynth() && ! juce::ModifierKeys::currentModifiers.isPopupMenu()) dawui::openPlugin (first);
                    else chooseInstrument();
                };
                instrument.setTooltip ("Instrument: click to open it, right-click to change it");
                instrument.ink = VegaColours::text;
                addAndMakeVisible (instrument);
            }
            addFx.empty = true; addFx.setTooltip ("Add an insert"); addFx.onClick = [this] { chooseFx(); }; addAndMakeVisible (addFx);
            addSend.setButtonText ("+ Send"); addSend.setTooltip ("Add a send to a bus"); addSend.onClick = [this] { chooseSend(); }; addAndMakeVisible (addSend);
            if (auto* lm = track->getLevelMeterPlugin()) { lm->measurer.addClient (client); measurer = &lm->measurer; }
        }
        rebuildSlots();
    }
    ~Strip() override { if (measurer) measurer->removeClient (client); }

    te::VolumeAndPanPlugin* volume() { return track != nullptr ? track->getVolumePlugin() : vs.edit().getMasterVolumePlugin().get(); }
    juce::String name() const { return track != nullptr ? track->getName() : juce::String ("Master"); }

    /** Height the inserts + sends sections need */
    int editNeeded() const
    {
        using namespace mixlayout;
        if (track == nullptr) return 0;
        int h = title + 2;                                           // INSERTS
        if (instrument.isVisible() || DawSession::kindOf (*track) == DawSession::TrackKind::midi) h += 24;
        h += slots.size() * 24 + 24 + gap;
        h += title + 2 + sends.size() * 26 + 22 + gap;               // SENDS
        return h;
    }
    int editH = 0;

    void rebuildSlots()
    {
        slots.clear(); sends.clear();
        if (track == nullptr) { updateKnobs(); resized(); return; }
        for (auto* p : vs.session.getInsertPlugins (*track))
        {
            if (p->isSynth() && DawSession::kindOf (*track) == DawSession::TrackKind::midi && track->pluginList.indexOf (p) == 0) continue;
            auto* b = slots.add (new cs::ScreenButton (p->getName()));
            b->ink = p->isEnabled() ? VegaColours::accent : VegaColours::textDim.withAlpha (0.6f);
            b->setTooltip (p->getName() + (p->isEnabled() ? "" : " (bypassed)") + " - click to open, right-click for options");
            b->onClick = [this, pp = te::Plugin::Ptr (p), b]
            {
                if (juce::ModifierKeys::currentModifiers.isPopupMenu()) slotMenu (pp, b); else dawui::openPlugin (pp.get());
            };
            addAndMakeVisible (b);
        }
        for (auto* bus : vs.session.getBuses())
            if (auto* s = vs.session.findSend (*track, *bus))
            {
                auto* row = sends.add (new SendRow (*s, bus->getName()));
                row->name.onClick = [this, b = te::AudioTrack::Ptr (bus)]
                {
                    juce::PopupMenu m; m.addItem (1, "Remove send to " + b->getName());
                    m.showMenuAsync ({}, [this, b] (int r) { if (r == 1) if (auto* sp = vs.session.findSend (*track, *b)) { sp->deleteFromParent(); rebuildSlots(); } });
                };
                addAndMakeVisible (row->knob); addAndMakeVisible (row->name);
            }
        djPlugin = vs.session.djEq (*track);
        if (DawSession::kindOf (*track) == DawSession::TrackKind::midi)
        {
            auto* first = track->pluginList[0];
            instrument.setButtonText (first != nullptr && first->isSynth() ? first->getName() : "Choose instrument");
        }
        outBuses.clear(); output.clear (juce::dontSendNotification);
        output.addItem ("Master", 1);
        for (auto* b : vs.session.getBuses()) if (b != track) { outBuses.add (b); output.addItem (b->getName(), outBuses.size() + 1); }
        auto* cur = vs.session.getOutputBus (*track);
        output.setSelectedItemIndex (cur == nullptr ? 0 : outBuses.indexOf (cur) + 1, juce::dontSendNotification);
        updateKnobs();
        resized();
    }
    juce::String layoutKey()
    {
        juce::String k;
        if (track == nullptr) return "master";
        k << track->getName() << "|" << track->pluginList.size();
        for (auto* p : track->pluginList.getPlugins()) k << p->getName() << (int) p->isEnabled();
        if (auto* o = vs.session.getOutputBus (*track)) k << ">" << o->getName();
        return k;
    }
    void refresh()
    {
        if (auto vp = volume())
        {
            if (! fader.isMouseButtonDown() && std::abs (fader.getValue() - vp->getVolumeDb()) > 0.01) fader.setValue (vp->getVolumeDb(), juce::dontSendNotification);
            if (! pan.isMouseButtonDown() && std::abs (pan.getValue() - vp->getPan()) > 0.001) pan.setValue (vp->getPan(), juce::dontSendNotification);
        }
        if (auto* d = dj())
        {
            for (int i = 0; i < 3; ++i)
                if (! knobs[i].isMouseButtonDown() && std::abs (knobs[i].getValue() - d->band[i]->getCurrentValue()) > 0.05) knobs[i].setValue (d->band[i]->getCurrentValue(), juce::dontSendNotification);
            if (! knobs[3].isMouseButtonDown() && std::abs (knobs[3].getValue() - d->filter->getCurrentValue()) > 0.001) knobs[3].setValue (d->filter->getCurrentValue(), juce::dontSendNotification);
            const auto tn = VegaDJEQPlugin::tapeName (d->tapeValue.get());
            if (tape.getButtonText() != tn) { tape.setButtonText (tn); tape.ink = d->tapeValue.get() > 0 ? VegaColours::accent : VegaColours::textDim; tape.repaint(); }
        }
        for (auto* s : sends) if (! s->knob.isMouseButtonDown()) s->knob.setValue (s->send.getGainDb(), juce::dontSendNotification);
        if (track != nullptr)
        {
            auto setT = [] (juce::Button& b, bool on) { if (b.getToggleState() != on) b.setToggleState (on, juce::dontSendNotification); };
            setT (rec, dawui::isArmed (*track)); setT (solo, track->isSolo (false)); setT (mute, track->isMuted (false));
            const auto inName = currentInput();
            if (input.getButtonText() != inName) input.setButtonText (inName);
            auto& arm = vs.edit().getAutomationRecordManager();
            const juce::String am = arm.isWritingAutomation() ? "WRITE" : arm.isReadingAutomation() ? "READ" : "OFF";
            if (autoMode.getButtonText() != am)
            {
                autoMode.setButtonText (am);
                autoMode.ink = am == "WRITE" ? juce::Colour (0xffff5a5a) : am == "READ" ? VegaColours::accent : VegaColours::textDim;
                autoMode.repaint();
            }
        }
        auto* m = measurer;
        if (m != nullptr)
            for (int c = 0; c < 2; ++c)
            {
                const float db = client.getAndClearAudioLevel (c).dB;
                level[c] = db > level[c] ? level[c] + (db - level[c]) * 0.6f : std::max (db, level[c] - 0.9f);
                hold[c] = level[c] >= hold[c] ? level[c] : hold[c] - 0.4f;
            }
        // VU: slow (300 ms-ish) ballistics on the louder side
        const float target = juce::jlimit (-20.0f, 3.0f, std::max (level[0], level[1]) + 6.0f);
        vu += (target - vu) * 0.18f;
        const bool sel = track != nullptr && vs.selectedTrack == track->itemID;
        if (sel != wasSelected) { wasSelected = sel; fader.lit = sel; fader.repaint(); repaint(); }
        repaint (meterArea.expanded (2)); repaint (vuArea);
    }
    void paint (juce::Graphics& g) override
    {
        using namespace mixlayout;
        auto r = getLocalBounds();
        const bool sel = track != nullptr && vs.selectedTrack == track->itemID;
        // strip body: the plug-ins' charcoal panel with a hairline divider
        g.setGradientFill (juce::ColourGradient (VegaColours::panelLight, 0, 0, VegaColours::panel, 0, (float) getHeight(), false));
        g.fillRect (r);
        g.setColour (VegaColours::line); g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
        if (sel) { g.setColour (VegaColours::accent.withAlpha (0.04f)); g.fillRect (r); }
        // section titles
        for (auto& t : titles) vegapaint::sectionTitle (g, t.r, t.text);
        // VU (black screen, cyan needle)
        if (! vuArea.isEmpty())
        {
            auto v = vuArea.toFloat();
            vegapaint::vuFace (g, v);
            const auto pivot = juce::Point<float> (v.getCentreX(), v.getBottom() + 10.0f);
            const float rad = v.getHeight() + 2.0f;
            auto angleFor = [] (float db) { return juce::jmap (db, -20.0f, 3.0f, -0.82f, 0.82f); };
            juce::Path arc; arc.addCentredArc (pivot.x, pivot.y, rad - 6, rad - 6, 0, angleFor (-20), angleFor (0), true);
            g.setColour (vegapaint::vuInk.withAlpha (0.85f)); g.strokePath (arc, juce::PathStrokeType (1.2f));
            juce::Path hot; hot.addCentredArc (pivot.x, pivot.y, rad - 6, rad - 6, 0, angleFor (0), angleFor (3), true);
            g.setColour (juce::Colour (0xffff6a5a)); g.strokePath (hot, juce::PathStrokeType (2.2f));
            g.setFont (VegaFonts::ui (8.5f)); g.setColour (vegapaint::vuInk);
            g.drawText ("-20", (int) v.getX() + 4, (int) v.getY() + 2, 20, 10, juce::Justification::left);
            g.setColour (juce::Colour (0xffff6a5a)); g.drawText ("+3", (int) v.getRight() - 18, (int) v.getY() + 2, 14, 10, juce::Justification::right);
            g.setColour (vegapaint::vuInk.withAlpha (0.8f)); g.drawText ("VU", vuArea.withTrimmedTop (vuArea.getHeight() - 11), juce::Justification::centred);
            const float a = angleFor (vu);
            g.saveState(); g.reduceClipRegion (vuArea);
            g.setColour (vegapaint::vuNeedle);
            g.drawLine (pivot.x + std::sin (a) * 12.0f, pivot.y - std::cos (a) * 12.0f, pivot.x + std::sin (a) * (rad - 3), pivot.y - std::cos (a) * (rad - 3), 1.5f);
            g.restoreState();
        }
        // knob captions + values
        static const char* labels[] { "HI", "MID", "LO", "FILTER" };
        for (int i = 0; i < 4; ++i)
        {
            if (! knobs[i].isVisible()) continue;
            auto kb = knobs[i].getBounds();
            const double v = knobs[i].getValue();
            const bool killed = i < 3 && v <= VegaDJEQPlugin::killDb + 0.05;
            const bool active = i < 3 ? std::abs (v) >= 0.05 : std::abs (v) >= 0.03;
            g.setFont (VegaFonts::ui (9.5f, true));
            g.setColour (killed ? juce::Colour (0xffff5a5a) : active ? VegaColours::accent : VegaColours::textDim);
            g.drawText (killed ? juce::String ("KILL") : juce::String (labels[i]), kb.getX() - 6, kb.getBottom() - 2, kb.getWidth() + 12, 11, juce::Justification::centred);
        }
        if (pan.isVisible())
        {
            auto pb = pan.getBounds();
            g.setFont (VegaFonts::ui (9.5f, true));
            g.setColour (std::abs (pan.getValue()) >= 0.01 ? VegaColours::accent : VegaColours::textDim);
            g.drawText ("PAN " + pan.getTextFromValue (pan.getValue()), pb.getX() - 10, pb.getBottom() - 2, pb.getWidth() + 20, 11, juce::Justification::centred);
        }
        // divider above the channel controls
        if (dividerY > 0) { g.setColour (VegaColours::line); g.drawHorizontalLine (dividerY, 0, (float) getWidth() - 1); }
        // selected channel, like FL Studio: a lit frame around its fader + meters
        if (sel && ! meterArea.isEmpty())
        {
            auto box = fader.getBounds().getUnion (meterArea).expanded (3, 0).toFloat();
            g.setColour (VegaColours::accent.withAlpha (0.10f)); g.fillRoundedRectangle (box.expanded (3.0f), 4.0f);
            g.setColour (VegaColours::accent.withAlpha (0.25f)); g.drawRoundedRectangle (box.expanded (1.5f), 3.5f, 3.0f);
            g.setColour (VegaColours::accent); g.drawRoundedRectangle (box, 3.0f, 1.4f);
        }
        // meters (the plug-ins' LED columns) + scale
        if (! meterArea.isEmpty())
        {
            auto mr = meterArea.toFloat();
            const float cw = (mr.getWidth() - 2.0f) * 0.5f;
            vegapaint::ledColumn (g, mr.withWidth (cw), level[0], hold[0]);
            vegapaint::ledColumn (g, mr.withWidth (cw).translated (cw + 2.0f, 0), level[1], hold[1]);
        }
        // dB readout
        if (! dbArea.isEmpty())
        {
            vegapaint::screen (g, dbArea.toFloat());
            g.setFont (VegaFonts::mono (11.0f)); g.setColour (VegaColours::text);
            g.drawText (juce::String (fader.getValue(), 1) + " dB", dbArea, juce::Justification::centred);
        }
        // name plate with the track colour across the top
        auto np = nameArea;
        g.setColour (VegaColours::bar); g.fillRect (np);
        const auto tcol = track != nullptr ? DawSession::trackColour (*track) : VegaColours::accent;
        g.setColour (tcol); g.fillRect (np.removeFromTop (3));
        g.setColour (sel ? VegaColours::accent : juce::Colour (0xffe6e6e6)); g.setFont (VegaFonts::ui (12.5f, true));
        g.drawFittedText (name().toUpperCase(), np.reduced (4, 0), juce::Justification::centred, 1, 0.75f);
    }
    void resized() override
    {
        using namespace mixlayout;
        titles.clear();
        const int h = getHeight(), x0 = 8, w = W - 16;
        int y = 6;
        auto section = [&] (const juce::String& t) { titles.add ({ { 0, y, W, title }, t }); y += title + 2; };
        const bool hasTrack = track != nullptr;
        // INPUT
        section ("Input"); input.setBounds (x0, y, w, combo); y += combo + gap;
        // CONSOLE: VU + HI / MID / LO
        section ("Console");
        vuArea = { x0, y, w, vuH }; y += vuH + 2;
        for (int i = 0; i < 3; ++i) knobs[i].setBounds (x0 - 4 + i * (w + 8) / 3, y, (w + 8) / 3, eqRow - 10);
        y += eqRow + 2;
        // TAPE
        section ("Tape"); tape.setBounds (x0, y, w, combo); y += combo + gap;
        // INSERTS + SENDS (hidden with the panel's EDIT key)
        const bool ed = owner.showEdit && hasTrack;
        instrument.setVisible (ed && hasTrack && DawSession::kindOf (*track) == DawSession::TrackKind::midi);
        addFx.setVisible (ed); addSend.setVisible (ed);
        for (auto* s : slots) s->setVisible (ed);
        for (auto* s : sends) { s->knob.setVisible (ed); s->name.setVisible (ed); }
        if (ed)
        {
            const int top = y;
            section ("Inserts");
            if (instrument.isVisible()) { instrument.setBounds (x0, y, w, 22); y += 24; }
            for (auto* s : slots) { s->setBounds (x0, y, w, 22); y += 24; }
            addFx.setBounds (x0, y, w, 22); y += 24 + gap;
            section ("Sends");
            for (auto* s : sends) { s->name.setBounds (x0, y + 1, w - 28, combo); s->knob.setBounds (x0 + w - 26, y - 2, 28, 26); y += 26; }
            addSend.setBounds (x0, y, w, combo); y += 22 + gap;
            y = std::max (y, top + editH);
        }
        // OUTPUT
        section ("Output"); output.setBounds (x0, y, w, combo); y += combo + gap + 2;
        dividerY = y - 3;
        autoMode.setBounds (x0, y + 2, w, combo); y += combo + 6;
        // PAN + FILTER
        pan.setBounds (x0 + 2, y, w / 2 - 4, panRow - 12);
        knobs[3].setBounds (x0 + w / 2 + 2, y, w / 2 - 4, panRow - 12);
        y += panRow;
        // REC / S / M
        const int bw = (w + 4) / 3;
        rec.setBounds (x0 - 2, y, bw, sqRow); solo.setBounds (x0 - 2 + bw, y, bw, sqRow); mute.setBounds (x0 - 2 + 2 * bw, y, bw, sqRow);
        y += sqRow + 4;
        // fader + meters, dB readout, name plate
        nameArea = { 0, h - nameH, W - 1, nameH };
        dbArea = { x0, nameArea.getY() - dbH - 6, w, dbH };
        const int fTop = y, fBottom = dbArea.getY() - 6;
        fader.setBounds (x0 + 2, fTop, 64, std::max (60, fBottom - fTop));
        meterArea = { fader.getRight() + 8, fTop + 8, 18, std::max (40, fader.getHeight() - 16) };
        for (auto* c : std::initializer_list<juce::Component*> { &input, &tape, &output, &autoMode, &rec, &solo, &mute }) c->setVisible (hasTrack);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (track == nullptr) return;          // click anywhere on the strip selects it (like FL Studio)
        vs.selectTrack (track.get()); owner.touched (track->getName(), "Selected");
        juce::ignoreUnused (e);
    }
    te::AudioTrack::Ptr track;
private:
    VegaDJEQPlugin* dj() const { return dynamic_cast<VegaDJEQPlugin*> (djPlugin.get()); }
    void updateKnobs()
    {
        if (auto* d = dj())
        {
            for (int i = 0; i < 3; ++i) knobs[i].setValue (d->band[i]->getCurrentValue(), juce::dontSendNotification);
            knobs[3].setValue (d->filter->getCurrentValue(), juce::dontSendNotification);
        }
        repaint();
    }
    void touch (const juce::String& what, const juce::String& value) { owner.touched (name() + "  " + what, value); }
    juce::String currentInput()
    {
        juce::StringArray names;
        for (auto* in : track->edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, *track, 0)) names.add (in->getInputDevice().getName());
        if (names.isEmpty()) return "No input";
        return names.size() == 1 ? names[0] : names[0] + " +" + juce::String (names.size() - 1);
    }
    void inputMenu()
    {
        juce::PopupMenu m;
        int id = 1; std::vector<te::InputDeviceInstance*> list;
        for (auto* in : track->edit.getAllInputDevices())
        {
            const auto type = in->getInputDevice().getDeviceType();
            if (type != te::InputDevice::waveDevice && type != te::InputDevice::physicalMidiDevice) continue;
            m.addItem (id++, (type == te::InputDevice::waveDevice ? "Audio: " : "MIDI: ") + in->getInputDevice().getName(), true, te::isOnTargetTrack (*in, *track, 0));
            list.push_back (in);
        }
        if (list.empty()) m.addItem (-1, "No inputs enabled - Options > Audio / MIDI Settings", false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&input), [this, list] (int r)
        {
            if (r < 1 || r > (int) list.size()) return;
            auto* in = list[(size_t) r - 1];
            if (te::isOnTargetTrack (*in, *track, 0)) [[maybe_unused]] auto res = in->removeTarget (track->itemID, &track->edit.getUndoManager());
            else [[maybe_unused]] auto res = in->setTarget (track->itemID, in->getInputDevice().getDeviceType() == te::InputDevice::waveDevice, &track->edit.getUndoManager(), 0);
        });
    }
    void tapeMenu()
    {
        auto* d = dj(); if (d == nullptr) return;
        juce::PopupMenu m;
        m.addItem (1, "Off", true, d->tapeValue.get() == 0);
        m.addItem (2, "15 ips - warm, softer top, more low end", true, d->tapeValue.get() == 1);
        m.addItem (3, "30 ips - clean, a little glue", true, d->tapeValue.get() == 2);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&tape), [this] (int r)
        {
            if (r < 1) return;
            if (auto* dd = dj()) { dd->tapeValue = r - 1; touch ("Tape", VegaDJEQPlugin::tapeName (r - 1)); }
        });
    }
    void automationMenu()
    {
        auto& arm = vs.edit().getAutomationRecordManager();
        juce::PopupMenu m;
        m.addItem (1, "Read - play automation back", true, arm.isReadingAutomation() && ! arm.isWritingAutomation());
        m.addItem (2, "Write - record moves while playing", true, arm.isWritingAutomation());
        m.addItem (3, "Off - ignore automation", true, ! arm.isReadingAutomation());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&autoMode), [this] (int r)
        {
            auto& a = vs.edit().getAutomationRecordManager();
            if (r == 1) { a.setReadingAutomation (true); a.setWritingAutomation (false); }
            else if (r == 2) { a.setReadingAutomation (true); a.setWritingAutomation (true); }
            else if (r == 3) { a.setWritingAutomation (false); a.setReadingAutomation (false); }
        });
    }
    struct SendRow
    {
        SendRow (te::AuxSendPlugin& s, const juce::String& busName) : send (s)
        {
            name.setButtonText (busName); name.ink = VegaColours::text;
            knob.setRange (-60.0, 6.0, 0.1); knob.setSkewFactorFromMidPoint (-12.0);
            knob.setTextValueSuffix (" dB"); knob.setDoubleClickReturnValue (true, 0.0);
            knob.setValue (s.getGainDb(), juce::dontSendNotification);
            knob.onValueChange = [this] { send.setGainDb ((float) knob.getValue()); };
            knob.setTooltip ("Send level to " + busName);
        }
        te::AuxSendPlugin& send; xl::Knob knob; cs::ComboButton name;
    };
    void chooseInstrument()
    {
        dawui::choosePlugin (vs.edit().engine, true, &instrument, [this, t = track] (juce::String type, std::optional<juce::PluginDescription> d)
        {
            auto p = d ? vs.session.setInstrument (*t, {}, &*d) : vs.session.setInstrument (*t, type);
            rebuildSlots();
            dawui::openPlugin (p.get());
        });
    }
    void chooseFx()
    {
        dawui::choosePlugin (vs.edit().engine, false, &addFx, [this, t = track] (juce::String type, std::optional<juce::PluginDescription> d)
        {
            auto p = d ? vs.session.addPlugin (*t, {}, &*d) : vs.session.addPlugin (*t, type);
            rebuildSlots();
            dawui::openPlugin (p.get());
        });
    }
    void chooseSend()
    {
        juce::PopupMenu m; auto buses = vs.session.getBuses();
        for (int i = 0; i < buses.size(); ++i) if (buses[i] != track.get()) m.addItem (1 + i, buses[i]->getName(), vs.session.findSend (*track, *buses[i]) == nullptr);
        m.addSeparator(); m.addItem (999, "New bus...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addSend), [this, buses] (int r)
        {
            if (r == 999) { if (auto* b = vs.session.addTrack (DawSession::TrackKind::bus)) vs.session.setSend (*track, *b, -6.0f); }
            else if (r > 0) vs.session.setSend (*track, *buses[r - 1], -6.0f);
            rebuildSlots();
        });
    }
    void slotMenu (te::Plugin::Ptr p, juce::Component* target)
    {
        juce::PopupMenu m, sc;
        m.addItem (1, "Open editor");
        m.addItem (2, p->isEnabled() ? "Bypass" : "Enable");
        m.addItem (3, "Move up"); m.addItem (4, "Move down");
        if (p->canSidechain())
        {
            auto names = p->getSidechainSourceNames (true);
            const auto cur = p->getSidechainSourceName();
            for (int i = 0; i < names.size(); ++i) sc.addItem (100 + i, names[i], true, names[i] == cur);
            m.addSubMenu ("Sidechain input", sc);
        }
        m.addSeparator(); m.addItem (9, "Remove");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [this, p] (int r)
        {
            if (r == 1) dawui::openPlugin (p.get());
            else if (r == 2) p->setEnabled (! p->isEnabled());
            else if (r == 3 || r == 4)
            {
                const int idx = track->pluginList.indexOf (p.get()), to = idx + (r == 3 ? -1 : 1);
                if (to >= 0 && to < track->pluginList.size() && ! dynamic_cast<te::VolumeAndPanPlugin*> (track->pluginList[to]) && ! (to == 0 && track->pluginList[0]->isSynth()))
                { auto state = p->state; auto parent = state.getParent(); parent.moveChild (parent.indexOf (state), parent.indexOf (track->pluginList[to]->state), &vs.edit().getUndoManager()); }
            }
            else if (r >= 100) { auto names = p->getSidechainSourceNames (true); p->setSidechainSourceByName (names[r - 100]); if (r > 100) p->guessSidechainRouting(); }
            else if (r == 9) { p->deleteFromParent(); }
            rebuildSlots();
        });
    }
    struct Title { juce::Rectangle<int> r; juce::String text; };
    MixerView& owner;
    ViewState& vs;
    int col = 0;
    xl::Fader fader; xl::Knob knobs[4]; xl::Knob pan;
    cs::SqButton rec { "Rec", juce::Colour (0xffff5a5a) }, solo { "S", VegaColours::accent }, mute { "M", VegaColours::gold };
    cs::ComboButton input, tape, autoMode, addSend;
    cs::ScreenButton instrument, addFx;
    juce::ComboBox output;
    juce::OwnedArray<cs::ScreenButton> slots; juce::OwnedArray<SendRow> sends; juce::Array<te::AudioTrack*> outBuses;
    juce::Array<Title> titles;
    te::Plugin::Ptr djPlugin;
    te::LevelMeasurer::Client client; te::LevelMeasurer* measurer = nullptr;
    bool wasSelected = false;
    float level[2] { -100, -100 }, hold[2] { -100, -100 }, vu = -20.0f;
    int dividerY = 0;
    juce::Rectangle<int> meterArea, dbArea, nameArea, vuArea;
};

//==============================================================================
namespace tp
{
/** Light-grey console key; lit (salmon / cyan / green) when its function is on */
class Key : public juce::Button
{
public:
    enum Shape { square, pill, navUp, navDown, navLeft, navRight };
    explicit Key (Shape sh = square, juce::Colour litColour = juce::Colour (0xfff2867c)) : juce::Button ({}), shape (sh), lit (litColour) {}
    Shape shape; juce::Colour lit;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        vegapaint::key (g, getLocalBounds().toFloat().reduced (1.5f), getToggleState(), over, down, vegaLit (lit));
    }
    static juce::Colour vegaLit (juce::Colour c) { return c.getRed() > 200 && c.getGreen() < 170 ? juce::Colour (0xffff5a5a) : VegaColours::accent; }
};

/** Dark transport key with an icon */
class BigKey : public juce::Button
{
public:
    enum Icon { rew, ffwd, stop, play, rec };
    explicit BigKey (Icon i) : juce::Button ({}), icon (i) {}
    Icon icon;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState();
        const auto lit = icon == rec ? juce::Colour (0xffff5a5a) : VegaColours::accent;
        auto face = vegapaint::key (g, getLocalBounds().toFloat().reduced (1.5f), on, over, down, lit);
        const auto c = face.getCentre(); const float s = face.getHeight() * 0.22f;
        juce::Path p;
        auto tri = [&] (float cx, bool right) { if (right) p.addTriangle (cx - s, c.y - s, cx - s, c.y + s, cx + s * 0.9f, c.y); else p.addTriangle (cx + s, c.y - s, cx + s, c.y + s, cx - s * 0.9f, c.y); };
        switch (icon)
        {
            case rew:  tri (c.x - s * 0.9f, false); tri (c.x + s * 0.9f, false); break;
            case ffwd: tri (c.x - s * 0.9f, true);  tri (c.x + s * 0.9f, true);  break;
            case stop: p.addRectangle (c.x - s * 0.85f, c.y - s * 0.85f, s * 1.7f, s * 1.7f); break;
            case play: tri (c.x + s * 0.1f, true); break;
            case rec:  p.addEllipse (c.x - s * 0.95f, c.y - s * 0.95f, s * 1.9f, s * 1.9f); break;
        }
        g.setColour (on ? vegapaint::keyInk (true) : icon == rec ? juce::Colour (0xffff5a5a) : VegaColours::text); g.fillPath (p);
    }
};

/** Jog wheel: drag around (or up / down) to move the playhead, or scrub when SCRUB is lit */
class Jog : public juce::Component
{
public:
    std::function<void (double turns)> onTurn;
    std::function<void (bool)> onTouch;
    bool plain = false;     // true: all-black wheel (no bright ring)
    void paint (juce::Graphics& g) override
    {
        // a big plug-in knob: machined collar, black face, plus a finger dimple
        auto r = getLocalBounds().toFloat().reduced (6.0f);
        const auto c = r.getCentre(); const float rad = std::min (r.getWidth(), r.getHeight()) * 0.5f;
        vegapaint::knob (g, juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c), angle, true, juce::Colours::transparentWhite);
        const juce::Point<float> d (c.x + std::sin (angle) * rad * 0.52f, c.y - std::cos (angle) * rad * 0.52f);
        const float dr = rad * 0.2f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141414), d.x, d.y - dr, juce::Colour (0xff353535), d.x, d.y + dr, false));
        g.fillEllipse (juce::Rectangle<float> (dr * 2, dr * 2).withCentre (d));
        g.setColour (VegaColours::accent.withAlpha (plain ? 0.0f : 0.5f)); g.drawEllipse (juce::Rectangle<float> (dr * 2, dr * 2).withCentre (d), 1.0f);
    }
    void mouseDown (const juce::MouseEvent& e) override { lastAngle = angleAt (e.position); lastY = e.position.y; if (onTouch) onTouch (true); }
    void mouseUp (const juce::MouseEvent&) override { if (onTouch) onTouch (false); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        double delta;
        if (e.position.getDistanceFrom (c) > getWidth() * 0.12f)
        {   // circular motion
            const float a = angleAt (e.position);
            float da = a - lastAngle;
            if (da > juce::MathConstants<float>::pi) da -= juce::MathConstants<float>::twoPi;
            if (da < -juce::MathConstants<float>::pi) da += juce::MathConstants<float>::twoPi;
            lastAngle = a; delta = da / juce::MathConstants<double>::twoPi;
        }
        else { delta = (lastY - e.position.y) / 300.0; lastAngle = angleAt (e.position); }
        lastY = e.position.y;
        angle += (float) (delta * juce::MathConstants<double>::twoPi);
        if (onTurn) onTurn (delta);
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const double delta = w.deltaY * 0.15;
        angle += (float) (delta * juce::MathConstants<double>::twoPi);
        if (onTurn) onTurn (delta);
        repaint();
    }
private:
    float angleAt (juce::Point<float> p) const { const auto c = getLocalBounds().toFloat().getCentre(); return std::atan2 (p.x - c.x, c.y - p.y); }
    float angle = 0, lastAngle = 0, lastY = 0;
};
/** Reel-to-reel tape machine, drawn like real hardware: walnut cheeks, brushed-aluminium plate, machined
    silver reels (the tape packs shrink / grow as the song plays), chrome rollers and heads, a backlit VU meter
    following the master level, and a counter showing bars | beats.
    Drag either reel (or use the wheel) to move the playhead, or scrub with SCRUB lit. */
class TapeDeck : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (double turns)> onTurn;
    std::function<void (bool)> onTouch;
    void update (double posSec, double lenSec, bool isPlaying, bool isRecording, const juce::String& counterText)
    {
        const double len = std::max ({ lenSec, posSec + 30.0, 60.0 });
        const bool needleMoving = std::abs (vu - vuTarget) > 0.05f;
        vu += (vuTarget - vu) * 0.3f;      // VU ballistics: a slow, damped needle
        if (! needleMoving && std::abs (posSec - pos) < 1e-4 && len == length && isPlaying == playing && isRecording == recording && counterText == counter) return;
        pos = posSec; length = len; playing = isPlaying; recording = isRecording; counter = counterText;
        repaint();
    }
    /** master level in dBFS (0 VU = -18 dBFS) */
    void setLevel (float dbfs) { vuTarget = juce::jlimit (-30.0f, 6.0f, dbfs + 18.0f); }

    // kept for the DRIVE / IPS / CAL knobs and other parts that used the old palette
    static inline const juce::Colour gold { 0xff7ff7f2 }, goldHi { 0xffdcfffd }, goldLo { 0xff1c5f5c };

    void resized() override { background = {}; }
    void paint (juce::Graphics& g) override
    {
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (background.isNull() || std::abs (backgroundScale - scale) > 0.01f) renderBackground (scale);
        g.drawImage (background, getLocalBounds().toFloat());

        const auto plate = plateArea();
        const float reelR = reelRadius = std::min (plate.getHeight() * 0.45f, plate.getWidth() * 0.2f);
        const juce::Point<float> lc (plate.getX() + reelR + 2.0f, plate.getY() + reelR + 4.0f);
        const juce::Point<float> rc (plate.getRight() - reelR - 2.0f, lc.y);
        reelCentres[0] = lc; reelCentres[1] = rc;
        const float hubR = reelR * 0.26f, packMin = reelR * 0.34f, packMax = reelR * 0.9f;
        const double f = juce::jlimit (0.0, 1.0, pos / length);
        const float packL = (float) std::sqrt (packMin * packMin + (packMax * packMax - packMin * packMin) * (1.0 - f));
        const float packR = (float) std::sqrt (packMin * packMin + (packMax * packMax - packMin * packMin) * f);
        const float tapePx = (float) pos * 38.0f * ipsFactor;
        const float angL = -tapePx / packL, angR = -tapePx / packR;
        const float cx = plate.getCentreX();

        // ---- tape path: off each pack on a tangent, round the chrome guide rollers, across the heads
        const float rr = 6.5f, rollerY = plate.getBottom() - 46.0f;
        const juce::Point<float> gl (cx - 40.0f, rollerY), gr (cx + 40.0f, rollerY);
        auto tangentFrom = [] (juce::Point<float> p, juce::Point<float> c, float rad, bool wantRight)
        {
            const auto v = p - c; const float d = v.getDistanceFromOrigin();
            if (d <= rad + 0.5f) return c;
            const float base = std::atan2 (v.y, v.x), al = std::acos (rad / d);
            const juce::Point<float> t1 (c.x + rad * std::cos (base + al), c.y + rad * std::sin (base + al));
            const juce::Point<float> t2 (c.x + rad * std::cos (base - al), c.y + rad * std::sin (base - al));
            return (t1.x > t2.x) == wantRight ? t1 : t2;
        };
        const juce::Point<float> pl (gl.x - rr, gl.y), pr (gr.x + rr, gr.y);
        const auto tL = tangentFrom (pl, lc, packL, true), tR = tangentFrom (pr, rc, packR, false);
        // head assembly (behind the tape): chrome cover with three heads, capstan + rubber pinch roller
        auto heads = juce::Rectangle<float> (62.0f, 18.0f).withCentre ({ cx, rollerY + rr + 9.0f });
        chromeBox (g, heads, 3.0f);
        for (int k = -1; k <= 1; ++k)
        {
            auto h = juce::Rectangle<float> (12.0f, 7.0f).withCentre ({ cx + k * 17.0f, heads.getY() + 4.5f });
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5d60), h.getX(), h.getY(), juce::Colour (0xff1b1c1e), h.getX(), h.getBottom(), false));
            g.fillRoundedRectangle (h, 2.0f);
            g.setColour (juce::Colour (0xff0a0a0a)); g.drawVerticalLine ((int) h.getCentreX(), h.getY() + 1, h.getBottom() - 1);   // the head gap
        }
        juce::Path tape;
        tape.startNewSubPath (tL); tape.lineTo (pl);
        tape.addCentredArc (gl.x, gl.y, rr, rr, 0, juce::MathConstants<float>::pi * 1.5f, juce::MathConstants<float>::pi, false);
        tape.lineTo (gr.x, gr.y + rr);
        tape.addCentredArc (gr.x, gr.y, rr, rr, 0, juce::MathConstants<float>::pi, juce::MathConstants<float>::pi * 0.5f, false);
        tape.lineTo (tR);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.strokePath (tape, juce::PathStrokeType (3.2f), juce::AffineTransform::translation (0.0f, 1.5f));
        g.setColour (juce::Colour (0xff3b2414)); g.strokePath (tape, juce::PathStrokeType (2.6f));
        g.setColour (juce::Colour (0xff8a5d3b).withAlpha (0.55f)); g.strokePath (tape, juce::PathStrokeType (0.8f));
        chromeRoller (g, gl, rr); chromeRoller (g, gr, rr);
        chromeRoller (g, { heads.getRight() + 7.0f, heads.getCentreY() + 2.0f }, 3.6f);        // capstan
        {   // rubber pinch roller
            const juce::Point<float> pc (heads.getRight() + 16.0f, heads.getCentreY() + 2.0f); const float pr2 = 5.5f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3c), pc.x - pr2, pc.y - pr2, juce::Colour (0xff070707), pc.x + pr2, pc.y + pr2, false));
            g.fillEllipse (pc.x - pr2, pc.y - pr2, pr2 * 2, pr2 * 2);
            chromeRoller (g, pc, 2.0f);
        }

        drawReel (g, lc, reelR, hubR, packL, angL);
        drawReel (g, rc, reelR, hubR, packR, angR);

        // ---- backlit VU meter between the reels
        drawVU (g, juce::Rectangle<float> (88.0f, 52.0f).withCentre ({ cx, plate.getY() + 32.0f }), vu, on, on ? "VEGA TAPE  " + ipsText : juce::String ("BYPASSED"));

        // ---- counter window + play / rec jewels
        auto cnt = juce::Rectangle<float> (70.0f, 15.0f).withCentre ({ cx, plate.getY() + 70.0f });
        g.setColour (juce::Colour (0xff6d7174)); g.fillRoundedRectangle (cnt.expanded (2.0f), 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050505), cnt.getX(), cnt.getY(), juce::Colour (0xff1d1d1f), cnt.getX(), cnt.getBottom(), false));
        g.fillRoundedRectangle (cnt, 2.0f);
        g.setColour (juce::Colour (0xfff1ede2)); g.setFont (VegaFonts::mono (10.5f));
        g.drawText (counter, cnt, juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.fillRoundedRectangle (cnt.withHeight (cnt.getHeight() * 0.45f), 2.0f);
        jewel (g, { cnt.getX() - 9.0f, cnt.getCentreY() }, playing, juce::Colour (0xff4cff6a));
        jewel (g, { cnt.getRight() + 9.0f, cnt.getCentreY() }, recording, juce::Colour (0xffff3b30));

    }
    void mouseDown (const juce::MouseEvent& e) override { lastAngle = angleAt (e.position); lastY = e.position.y; if (onTouch) onTouch (true); }
    void mouseUp (const juce::MouseEvent&) override { if (onTouch) onTouch (false); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        double delta;
        const auto c = nearestReel (e.position);
        if (e.position.getDistanceFrom (c) > reelRadius * 0.2f)
        {
            const float a = angleAt (e.position);
            float da = a - lastAngle;
            if (da > juce::MathConstants<float>::pi) da -= juce::MathConstants<float>::twoPi;
            if (da < -juce::MathConstants<float>::pi) da += juce::MathConstants<float>::twoPi;
            lastAngle = a; delta = da / juce::MathConstants<double>::twoPi;
        }
        else { delta = (lastY - e.position.y) / 300.0; lastAngle = angleAt (e.position); }
        lastY = e.position.y;
        if (onTurn) onTurn (delta);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { if (onTurn) onTurn (w.deltaY * 0.25); }

private:
    static constexpr float cheekW = 15.0f;
    juce::Rectangle<float> plateArea() const { return getLocalBounds().toFloat().reduced (2.0f).withTrimmedLeft (cheekW).withTrimmedRight (cheekW); }

    // ---------------------------------------------------------------- the static parts (cached)
    void renderBackground (float scale)
    {
        backgroundScale = scale;
        background = juce::Image (juce::Image::ARGB, std::max (1, juce::roundToInt (getWidth() * scale)), std::max (1, juce::roundToInt (getHeight() * scale)), true);
        juce::Graphics g (background);
        g.addTransform (juce::AffineTransform::scale (scale));
        auto r = getLocalBounds().toFloat();
        {
            juce::Path sh; sh.addRoundedRectangle (r.reduced (2.0f).translated (0, 3), 5.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 10, {}).drawForPath (g, sh);
        }
        auto body = r.reduced (2.0f);
        juce::Random rnd (1972);
        // walnut cheeks with grain and a varnish shine
        for (int side = 0; side < 2; ++side)
        {
            auto ch = side == 0 ? body.withWidth (cheekW + 4.0f) : body.withLeft (body.getRight() - cheekW - 4.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b3a20), ch.getX(), ch.getY(), juce::Colour (0xff3a1b0d), ch.getRight(), ch.getBottom(), false));
            g.fillRoundedRectangle (ch, 4.0f);
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (ch.toNearestInt());
            for (int k = 0; k < 16; ++k)
            {
                const float x0 = ch.getX() + rnd.nextFloat() * ch.getWidth(), amp = 1.0f + rnd.nextFloat() * 2.5f, fr = 0.02f + rnd.nextFloat() * 0.05f, ph = rnd.nextFloat() * 6.28f;
                juce::Path grain;
                for (float y = ch.getY(); y <= ch.getBottom(); y += 3.0f)
                {
                    const float x = x0 + amp * std::sin (y * fr + ph);
                    if (y == ch.getY()) grain.startNewSubPath (x, y); else grain.lineTo (x, y);
                }
                g.setColour ((k % 3 == 0 ? juce::Colour (0xff2a1208) : juce::Colour (0xff8a5230)).withAlpha (0.25f + rnd.nextFloat() * 0.25f));
                g.strokePath (grain, juce::PathStrokeType (0.6f + rnd.nextFloat() * 0.9f));
            }
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), ch.getX(), ch.getY(), juce::Colours::transparentWhite, ch.getX() + ch.getWidth() * 0.6f, ch.getY(), false));
            g.fillRect (ch);
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (ch.reduced (0.5f), 4.0f, 1.0f);
        }
        // brushed aluminium plate
        auto plate = plateArea();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd3d6d8), plate.getX(), plate.getY(), juce::Colour (0xff8e9296), plate.getX(), plate.getBottom(), false));
        g.fillRect (plate);
        for (float y = plate.getY(); y < plate.getBottom(); y += 1.0f)
        {
            g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f + rnd.nextFloat() * 0.05f));
            const float x0 = plate.getX() + rnd.nextFloat() * plate.getWidth() * 0.3f;
            g.drawHorizontalLine ((int) y, x0, std::min (plate.getRight(), x0 + plate.getWidth() * (0.5f + rnd.nextFloat())));
        }
        {   // a broad soft reflection across the metal
            juce::ColourGradient refl (juce::Colours::white.withAlpha (0.0f), plate.getX(), plate.getY(), juce::Colours::white.withAlpha (0.0f), plate.getRight(), plate.getBottom(), false);
            refl.addColour (0.42, juce::Colours::white.withAlpha (0.16f)); refl.addColour (0.5, juce::Colours::white.withAlpha (0.04f));
            g.setGradientFill (refl); g.fillRect (plate);
        }
        g.setColour (juce::Colours::white.withAlpha (0.55f)); g.drawHorizontalLine ((int) plate.getY(), plate.getX(), plate.getRight());
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawHorizontalLine ((int) plate.getBottom() - 1, plate.getX(), plate.getRight());
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawRect (plate, 1.0f);
        // recessed reel wells (subtle) and the transport screws
        for (auto sc : { plate.getTopLeft().translated (6, 6), plate.getTopRight().translated (-6, 6), plate.getBottomLeft().translated (6, -6), plate.getBottomRight().translated (-6, -6) })
            screw (g, sc, 3.0f, rnd.nextFloat() * 3.0f);
        // nameplate: a small chrome plaque, engraved script
        auto plaque = juce::Rectangle<float> (112.0f, 15.0f).withCentre ({ plate.getCentreX(), plate.getBottom() - 11.0f });
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (plaque.translated (0, 1.5f), 7.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c3f), plaque.getX(), plaque.getY(), juce::Colour (0xff17181a), plaque.getX(), plaque.getBottom(), false));
        g.fillRoundedRectangle (plaque, 7.0f);
        g.setColour (juce::Colour (0xffb9bdc0)); g.drawRoundedRectangle (plaque, 7.0f, 1.0f);
        g.setFont (juce::FontOptions ("Georgia", 10.0f, juce::Font::italic));
        g.setColour (juce::Colour (0xffe9e6dc)); g.drawFittedText ("Vega Reel to Reel", plaque.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }

    static void screw (juce::Graphics& g, juce::Point<float> c, float r, float slotAngle)
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (c.x - r - 0.5f, c.y - r + 0.8f, r * 2 + 1, r * 2 + 1);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f2), c.x - r, c.y - r, juce::Colour (0xff6c7074), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colour (0xff3a3c3e));
        g.drawLine (c.x - std::cos (slotAngle) * r * 0.8f, c.y - std::sin (slotAngle) * r * 0.8f, c.x + std::cos (slotAngle) * r * 0.8f, c.y + std::sin (slotAngle) * r * 0.8f, 1.0f);
    }
    static void chromeRoller (juce::Graphics& g, juce::Point<float> c, float r)
    {
        {
            juce::Path sh; sh.addEllipse (c.x - r, c.y - r + 1.5f, r * 2, r * 2);
            juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 4, {}).drawForPath (g, sh);
        }
        juce::ColourGradient cg (juce::Colour (0xffffffff), c.x - r * 0.5f, c.y - r * 0.6f, juce::Colour (0xff595d61), c.x + r * 0.6f, c.y + r * 0.8f, true);
        cg.addColour (0.5, juce::Colour (0xffb5b9bc));
        g.setGradientFill (cg); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
        g.setColour (juce::Colour (0xff4a4d50)); g.fillEllipse (c.x - r * 0.25f, c.y - r * 0.25f, r * 0.5f, r * 0.5f);
    }
    static void chromeBox (juce::Graphics& g, juce::Rectangle<float> b, float corner)
    {
        juce::Path sh; sh.addRoundedRectangle (b.translated (0, 2), corner);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 5, {}).drawForPath (g, sh);
        juce::ColourGradient cg (juce::Colour (0xfff5f6f7), b.getX(), b.getY(), juce::Colour (0xff6d7175), b.getX(), b.getBottom(), false);
        cg.addColour (0.45, juce::Colour (0xffc9ccce)); cg.addColour (0.55, juce::Colour (0xff9a9ea2));
        g.setGradientFill (cg); g.fillRoundedRectangle (b, corner);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (b, corner, 0.8f);
    }
    static void jewel (juce::Graphics& g, juce::Point<float> c, bool lit, juce::Colour col)
    {
        const float r = 3.6f;
        g.setColour (juce::Colour (0xff6d7174)); g.fillEllipse (c.x - r - 1.2f, c.y - r - 1.2f, r * 2 + 2.4f, r * 2 + 2.4f);
        if (lit) { g.setColour (col.withAlpha (0.45f)); g.fillEllipse (c.x - r - 4, c.y - r - 4, r * 2 + 8, r * 2 + 8); }
        g.setGradientFill (juce::ColourGradient (lit ? col.brighter (0.6f) : col.darker (0.6f).withMultipliedSaturation (0.5f), c.x - r * 0.4f, c.y - r * 0.5f,
                                                 lit ? col.darker (0.2f) : juce::Colour (0xff1a0d0d), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colours::white.withAlpha (0.6f)); g.fillEllipse (c.x - r * 0.5f, c.y - r * 0.6f, r * 0.6f, r * 0.45f);
    }

    /** Cream backlit VU meter in a chrome bezel; `vuDb` is in VU (0 VU = -18 dBFS) */
    static void drawVU (juce::Graphics& g, juce::Rectangle<float> b, float vuDb, bool lampOn, const juce::String& subtitle)
    {
        chromeBox (g, b, 4.0f);
        auto face = b.reduced (4.0f);
        juce::ColourGradient fg (juce::Colour (0xfffbf3d9), face.getCentreX(), face.getY() + face.getHeight() * 0.3f, juce::Colour (0xffd9c99e), face.getX(), face.getBottom(), true);
        if (! lampOn) fg = juce::ColourGradient (juce::Colour (0xffc9c1a8), face.getCentreX(), face.getY(), juce::Colour (0xffa09678), face.getX(), face.getBottom(), true);
        g.setGradientFill (fg); g.fillRoundedRectangle (face, 2.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (face.toNearestInt());
        const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.55f);
        const float R = face.getHeight() * 1.5f;
        const float aMin = -0.62f, aMax = 0.62f;
        auto lin = [] (float db) { return std::pow (10.0f, db / 20.0f); };
        auto frac = [&] (float db) { return juce::jlimit (0.0f, 1.0f, (lin (db) - lin (-20.0f)) / (lin (3.0f) - lin (-20.0f))); };
        auto angleOf = [&] (float db) { return aMin + frac (db) * (aMax - aMin); };
        auto pt = [&] (float rad, float a) { return juce::Point<float> (pivot.x + std::sin (a) * rad, pivot.y - std::cos (a) * rad); };
        // scale arcs: black, then red from 0 to +3
        juce::Path arc; arc.addCentredArc (pivot.x, pivot.y, R * 0.78f, R * 0.78f, 0, angleOf (-20.0f), angleOf (0.0f), true);
        g.setColour (juce::Colour (0xff1d1a14)); g.strokePath (arc, juce::PathStrokeType (1.0f));
        juce::Path red; red.addCentredArc (pivot.x, pivot.y, R * 0.785f, R * 0.785f, 0, angleOf (0.0f), angleOf (3.0f), true);
        g.setColour (juce::Colour (0xffc8261c)); g.strokePath (red, juce::PathStrokeType (2.6f));
        const float marks[] { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
        g.setFont (VegaFonts::ui (6.5f, true));
        for (float m : marks)
        {
            const float a = angleOf (m);
            const bool major = m == -20 || m == -10 || m == -7 || m == -5 || m == -3 || m == 0 || m == 3;
            g.setColour (m > 0 ? juce::Colour (0xffc8261c) : juce::Colour (0xff1d1a14));
            g.drawLine (juce::Line<float> (pt (R * 0.78f, a), pt (R * (major ? 0.9f : 0.85f), a)), major ? 1.1f : 0.8f);
            if (major)
                g.drawText (juce::String (std::abs ((int) m)), juce::Rectangle<float> (14.0f, 8.0f).withCentre (pt (R * 0.97f, a)), juce::Justification::centred);
        }
        g.setColour (juce::Colour (0xff1d1a14)); g.setFont (juce::FontOptions ("Georgia", 9.0f, juce::Font::bold));
        g.drawText ("VU", juce::Rectangle<float> (30.0f, 10.0f).withCentre ({ face.getCentreX(), face.getBottom() - 11.0f }), juce::Justification::centred);
        g.setFont (VegaFonts::ui (5.5f, true));
        g.drawText (subtitle, juce::Rectangle<float> (60.0f, 7.0f).withCentre ({ face.getCentreX(), face.getBottom() - 4.0f }), juce::Justification::centred);
        // needle with a soft shadow
        const float a = aMin + frac (vuDb) * (aMax - aMin) + (vuDb < -20.0f ? -0.06f : 0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f)); g.drawLine (juce::Line<float> (pt (R * 0.25f, a).translated (0.8f, 1.0f), pt (R * 0.95f, a).translated (0.8f, 1.0f)), 1.6f);
        g.setColour (juce::Colour (0xff111111)); g.drawLine (juce::Line<float> (pt (R * 0.25f, a + 0.012f), pt (R * 0.95f, a + 0.012f)), 1.1f);
        // glass: a diagonal reflection and darker corners
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), face.getX(), face.getY(), juce::Colours::white.withAlpha (0.0f), face.getX() + face.getWidth() * 0.45f, face.getBottom(), false));
        g.fillRect (face.withWidth (face.getWidth() * 0.55f));
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.drawRoundedRectangle (face, 2.0f, 1.5f);
    }

    /** A machined aluminium NAB-style reel: three cut-outs (the tape pack and the plate show through),
        concentric tooling marks, a light streak that stays put while the reel turns, chrome hub. */
    static void drawReel (juce::Graphics& g, juce::Point<float> c, float r, float hubR, float pack, float ang)
    {
        auto pt = [&] (float rad, float a) { return juce::Point<float> (c.x + std::sin (a) * rad, c.y - std::cos (a) * rad); };
        // tape pack (behind the flange)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a4632), c.x - pack, c.y - pack, juce::Colour (0xff22140b), c.x + pack, c.y + pack, false));
        g.fillEllipse (c.x - pack, c.y - pack, pack * 2, pack * 2);
        for (float rr = hubR * 1.1f; rr < pack; rr += 1.5f)
        { g.setColour (juce::Colours::black.withAlpha (((int) (rr * 7.0f)) % 3 == 0 ? 0.2f : 0.08f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.5f); }
        {
            juce::Path sheen; sheen.addPieSegment (c.x - pack, c.y - pack, pack * 2, pack * 2, -1.0f, -0.25f, hubR * 1.1f / pack);
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillPath (sheen);
        }
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawEllipse (c.x - pack, c.y - pack, pack * 2, pack * 2, 1.0f);

        // flange with three windows (turning with the reel)
        juce::Path flange; flange.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
        juce::Path windows;
        for (int k = 0; k < 3; ++k)
        {
            const float a0 = ang + k * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path w;
            w.startNewSubPath (pt (hubR * 1.45f, a0 - 0.42f)); w.lineTo (pt (r * 0.87f, a0 - 0.62f));
            w.lineTo (pt (r * 0.87f, a0 + 0.62f)); w.lineTo (pt (hubR * 1.45f, a0 + 0.42f)); w.closeSubPath();
            windows.addPath (w.createPathWithRoundedCorners (r * 0.12f));
        }
        flange.setUsingNonZeroWinding (false); flange.addPath (windows);
        {
            juce::Path sh; sh.addEllipse (c.x - r, c.y - r + 4, r * 2, r * 2);
            juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 12, {}).drawForPath (g, sh);
        }
        juce::ColourGradient fg (juce::Colour (0xffe9ebec), c.x - r * 0.7f, c.y - r * 0.8f, juce::Colour (0xff7d8185), c.x + r * 0.8f, c.y + r * 0.9f, false);
        fg.addColour (0.5, juce::Colour (0xffbfc2c5));
        g.setGradientFill (fg); g.fillPath (flange);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (flange);
            // concentric tooling marks
            for (float rr = hubR; rr < r; rr += 1.3f)
            {
                g.setColour ((((int) (rr * 3.1f)) % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.045f));
                g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.7f);
            }
            // anisotropic highlight: a bright "bow-tie" that stays fixed while the reel turns
            for (float base : { -0.78f, 2.36f })
            {
                juce::Path wedge; wedge.addPieSegment (c.x - r, c.y - r, r * 2, r * 2, base - 0.32f, base + 0.32f, 0.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.42f), c.x, c.y, juce::Colours::white.withAlpha (0.05f), pt (r, base).x, pt (r, base).y, true));
                g.fillPath (wedge);
            }
            for (float base : { 0.78f, -2.36f })
            {
                juce::Path wedge; wedge.addPieSegment (c.x - r, c.y - r, r * 2, r * 2, base - 0.3f, base + 0.3f, 0.0f);
                g.setColour (juce::Colours::black.withAlpha (0.10f)); g.fillPath (wedge);
            }
            // window edges: machined bevel (light on the upper-left, dark lower-right)
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.strokePath (windows, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (0.7f, 0.7f));
            g.setColour (juce::Colours::black.withAlpha (0.55f)); g.strokePath (windows, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (-0.6f, -0.6f));
            // engraved maker's mark between two windows
            g.addTransform (juce::AffineTransform::rotation (ang + juce::MathConstants<float>::pi / 3.0f, c.x, c.y));
            g.setFont (juce::FontOptions ("Georgia", r * 0.15f, juce::Font::bold | juce::Font::italic));
            auto tb = juce::Rectangle<float> (r * 0.8f, r * 0.18f).withCentre ({ c.x, c.y - r * 0.66f });
            g.setColour (juce::Colours::white.withAlpha (0.6f)); g.drawText ("Vega", tb.translated (0, 0.8f), juce::Justification::centred);
            g.setColour (juce::Colour (0xff4a4d50)); g.drawText ("Vega", tb, juce::Justification::centred);
        }
        // rim: rolled edge
        g.setColour (juce::Colours::white.withAlpha (0.6f)); g.drawEllipse (c.x - r + 1.2f, c.y - r + 1.2f, r * 2 - 2.4f, r * 2 - 2.4f, 1.0f);
        g.setColour (juce::Colour (0xff3d4043)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.2f);
        // chrome hub with three keys
        {
            juce::Path sh; sh.addEllipse (c.x - hubR, c.y - hubR + 2, hubR * 2, hubR * 2);
            juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 6, {}).drawForPath (g, sh);
        }
        juce::ColourGradient hg (juce::Colour (0xffffffff), c.x - hubR * 0.5f, c.y - hubR * 0.6f, juce::Colour (0xff5c6064), c.x + hubR * 0.6f, c.y + hubR * 0.8f, true);
        hg.addColour (0.55, juce::Colour (0xffaeb2b5));
        g.setGradientFill (hg); g.fillEllipse (c.x - hubR, c.y - hubR, hubR * 2, hubR * 2);
        for (float rr = hubR * 0.3f; rr < hubR; rr += 1.2f)
        { g.setColour (juce::Colours::black.withAlpha (0.05f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.5f); }
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - hubR, c.y - hubR, hubR * 2, hubR * 2, 0.8f);
        for (int k = 0; k < 3; ++k)
        {   // the three spindle keys
            const float a = ang + juce::MathConstants<float>::pi / 3.0f + k * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path key; key.addRoundedRectangle (-hubR * 0.12f, -hubR * 0.78f, hubR * 0.24f, hubR * 0.36f, hubR * 0.06f);
            key.applyTransform (juce::AffineTransform::rotation (a).translated (c.x, c.y));
            g.setColour (juce::Colour (0xff2b2d30)); g.fillPath (key);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e3134), c.x, c.y - hubR * 0.3f, juce::Colour (0xff0d0e0f), c.x, c.y + hubR * 0.3f, false));
        g.fillEllipse (c.x - hubR * 0.28f, c.y - hubR * 0.28f, hubR * 0.56f, hubR * 0.56f);   // spindle hole
        g.setColour (juce::Colour (0xffd7dadc)); g.fillEllipse (c.x - hubR * 0.1f, c.y - hubR * 0.1f, hubR * 0.2f, hubR * 0.2f);
    }

    juce::Point<float> nearestReel (juce::Point<float> p) const { return p.getDistanceFrom (reelCentres[0]) < p.getDistanceFrom (reelCentres[1]) ? reelCentres[0] : reelCentres[1]; }
    float angleAt (juce::Point<float> p) const { const auto c = nearestReel (p); return std::atan2 (p.x - c.x, c.y - p.y); }
    double pos = 0, length = 60; bool playing = false, recording = false; juce::String counter { "1.1.1" };
    float vu = -30.0f, vuTarget = -30.0f;
    juce::Image background; float backgroundScale = 1.0f;
public:
    float ipsFactor = 1.0f; bool on = true; juce::String ipsText { "15 IPS" };
private:
    juce::Point<float> reelCentres[2]; float reelRadius = 60.0f;
    float lastAngle = 0, lastY = 0;
};
/** Brass machined knob on a black skirt with gold ticks (the tape machine's DRIVE / IPS / CAL) */
class GoldKnob : public xl::Knob
{
public:
    void paint (juce::Graphics& g) override
    {
        // like a control surface's jog knob: knurled chrome ring, matte black cap with a finger dimple, blue ticks
        const auto rp = getRotaryParameters();
        auto b = getLocalBounds().toFloat();
        const float rad = std::min (b.getWidth(), b.getHeight() - 4.0f) * 0.5f - 8.0f;
        const auto c = b.getCentre().translated (0, -2.0f);
        const float a = rp.startAngleRadians + (float) valueToProportionOfLength (getValue()) * (rp.endAngleRadians - rp.startAngleRadians);
        const int n = getInterval() >= 1.0 ? (int) std::round ((getMaximum() - getMinimum()) / getInterval()) + 1 : 11;
        for (int i = 0; i < n; ++i)
        {
            const float t = rp.startAngleRadians + (float) i / (float) (n - 1) * (rp.endAngleRadians - rp.startAngleRadians);
            g.setColour (juce::Colour (0xffc9cdd0).withAlpha (i == 0 || i == n - 1 ? 1.0f : 0.6f));
            g.drawLine (c.x + std::sin (t) * (rad + 4), c.y - std::cos (t) * (rad + 4), c.x + std::sin (t) * (rad + 8), c.y - std::cos (t) * (rad + 8), 1.2f);
        }
        {
            juce::Path sh; sh.addEllipse (c.x - rad, c.y - rad + 3, rad * 2, rad * 2);
            juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 9, {}).drawForPath (g, sh);
        }
        // chrome ring with knurling (fine dark/light lines all round)
        juce::ColourGradient chrome (juce::Colour (0xfff4f4f4), c.x, c.y - rad, juce::Colour (0xff6e6e6e), c.x, c.y + rad, false);
        chrome.addColour (0.35, juce::Colour (0xffb0b0b0)); chrome.addColour (0.6, juce::Colour (0xffe6e6e6));
        g.setGradientFill (chrome); g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        for (int k = 0; k < 72; ++k)
        {
            const float t = k * juce::MathConstants<float>::twoPi / 72.0f;
            g.setColour (juce::Colours::black.withAlpha (k % 2 ? 0.22f : 0.08f));
            g.drawLine (c.x + std::sin (t) * rad * 0.8f, c.y - std::cos (t) * rad * 0.8f, c.x + std::sin (t) * rad, c.y - std::cos (t) * rad, 0.9f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
        // black collar, then a machined chrome dome (like a tape machine's INPUT / OUTPUT knobs)
        const float cr = rad * 0.8f;
        g.setColour (juce::Colour (0xff141416)); g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        const float dr2 = cr * 0.82f;
        juce::ColourGradient dome (juce::Colour (0xffffffff), c.x - dr2 * 0.35f, c.y - dr2 * 0.45f, juce::Colour (0xff4f5357), c.x + dr2 * 0.6f, c.y + dr2 * 0.8f, true);
        dome.addColour (0.55, juce::Colour (0xffb2b6b9));
        g.setGradientFill (dome); g.fillEllipse (c.x - dr2, c.y - dr2, dr2 * 2, dr2 * 2);
        for (float rr = dr2 * 0.15f; rr < dr2; rr += 1.2f)
        { g.setColour ((((int) (rr * 2.7f)) % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.06f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f); }
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - dr2, c.y - dr2, dr2 * 2, dr2 * 2, 0.8f);
        // engraved index line
        g.setColour (juce::Colour (0xff1c1e20));
        g.drawLine (c.x + std::sin (a) * dr2 * 0.35f, c.y - std::cos (a) * dr2 * 0.35f, c.x + std::sin (a) * dr2 * 0.95f, c.y - std::cos (a) * dr2 * 0.95f, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawLine (c.x + std::sin (a) * dr2 * 0.35f + 0.8f, c.y - std::cos (a) * dr2 * 0.35f + 0.8f, c.x + std::sin (a) * dr2 * 0.95f + 0.8f, c.y - std::cos (a) * dr2 * 0.95f + 0.8f, 0.6f);
        if (isMouseOverOrDragging()) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawEllipse (c.x - rad - 1, c.y - rad - 1, rad * 2 + 2, rad * 2 + 2, 2.0f); }
    }
};
} // namespace tp

namespace uf
{
/** White soft key; glows green when on */
class SoftKey : public juce::Button
{
public:
    SoftKey() : juce::Button ({}) {}
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float sz = std::min (r.getWidth(), r.getHeight());
        vegapaint::squareButton (g, juce::Rectangle<float> (sz, sz).withCentre (r.getCentre()), getToggleState(), over);
    }
};

/** Square console key with a drawn icon or text; optional lit colour */
class Key : public juce::Button
{
public:
    enum Icon { none, left, right, up, down, dot, loop, metro, dropIn, shiftUp, write, text };
    Key (Icon i = none, juce::String t = {}, juce::Colour litCol = juce::Colour (0xffffd98a), bool darkFace = false)
        : juce::Button (t), icon (i), lit (litCol), dark (false) { juce::ignoreUnused (darkFace); }
    Icon icon; juce::Colour lit; bool dark;
    void paintButton (juce::Graphics& g, bool over, bool isDown) override
    {
        const bool on = getToggleState();
        auto face = vegapaint::key (g, getLocalBounds().toFloat().reduced (2.0f), on, over, isDown, tp::Key::vegaLit (lit));
        const auto ink = vegapaint::keyInk (on);
        const auto c = face.getCentre(); const float s = std::min (face.getWidth(), face.getHeight()) * 0.26f;
        g.setColour (ink);
        juce::Path p;
        switch (icon)
        {
            case left:  p.addTriangle (c.x + s * 0.7f, c.y - s, c.x + s * 0.7f, c.y + s, c.x - s * 0.8f, c.y); break;
            case right: p.addTriangle (c.x - s * 0.7f, c.y - s, c.x - s * 0.7f, c.y + s, c.x + s * 0.8f, c.y); break;
            case up:    p.addTriangle (c.x - s, c.y + s * 0.7f, c.x + s, c.y + s * 0.7f, c.x, c.y - s * 0.8f); break;
            case down:  p.addTriangle (c.x - s, c.y - s * 0.7f, c.x + s, c.y - s * 0.7f, c.x, c.y + s * 0.8f); break;
            case dot:   p.addEllipse (c.x - s * 0.8f, c.y - s * 0.8f, s * 1.6f, s * 1.6f); break;
            case shiftUp: p.addArrow ({ c.x, c.y + s, c.x, c.y - s }, s * 0.7f, s * 1.8f, s * 1.0f); break;
            case loop:
            {
                juce::Path a; a.addCentredArc (c.x, c.y, s, s, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.2f, true);
                g.strokePath (a, juce::PathStrokeType (1.6f));
                p.addTriangle (c.x + s * 0.2f, c.y - s * 1.4f, c.x + s * 0.2f, c.y - s * 0.6f, c.x + s * 0.9f, c.y - s);
                break;
            }
            case metro: p.addTriangle (c.x - s * 0.8f, c.y + s, c.x + s * 0.8f, c.y + s, c.x, c.y - s); g.fillPath (p); p.clear();
                        g.setColour (on ? VegaColours::accent : juce::Colour (0xff262626)); g.drawLine (c.x, c.y + s * 0.6f, c.x + s * 0.6f, c.y - s * 0.6f, 1.5f); return;
            case dropIn: p.addArrow ({ c.x, c.y - s, c.x, c.y + s * 0.4f }, s * 0.6f, s * 1.6f, s * 0.9f); p.addRectangle (c.x - s, c.y + s * 0.7f, s * 2.0f, s * 0.35f); break;
            case write: { juce::Path a; a.addRectangle (c.x - s, c.y - s, s * 2, s * 2); g.strokePath (a, juce::PathStrokeType (1.4f));
                          p.addTriangle (c.x - s * 0.5f, c.y - s * 0.5f, c.x + s * 0.5f, c.y - s * 0.5f, c.x, c.y + s * 0.5f); break; }
            case text: g.setFont (VegaFonts::ui (getButtonText().length() > 3 ? 9.5f : 11.0f, true)); g.drawFittedText (getButtonText(), face.toNearestInt(), juce::Justification::centred, 2); return;
            case none: break;
        }
        g.fillPath (p);
    }
};

/** White transport key */
class TransportKey : public juce::Button
{
public:
    enum Icon { rew, ffwd, stop, play, rec };
    explicit TransportKey (Icon i) : juce::Button ({}), icon (i) {}
    Icon icon;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState();
        auto face = vegapaint::key (g, getLocalBounds().toFloat().reduced (2.0f), on, over, down, icon == rec ? juce::Colour (0xffff5a5a) : VegaColours::accent);
        const auto c = face.getCentre(); const float s = face.getHeight() * 0.2f;
        juce::Path p;
        auto tri = [&] (float cx, bool rgt) { if (rgt) p.addTriangle (cx - s, c.y - s, cx - s, c.y + s, cx + s * 0.9f, c.y); else p.addTriangle (cx + s, c.y - s, cx + s, c.y + s, cx - s * 0.9f, c.y); };
        switch (icon)
        {
            case rew: tri (c.x - s * 0.9f, false); tri (c.x + s * 0.9f, false); break;
            case ffwd: tri (c.x - s * 0.9f, true); tri (c.x + s * 0.9f, true); break;
            case stop: p.addRectangle (c.x - s * 0.9f, c.y - s * 0.9f, s * 1.8f, s * 1.8f); break;
            case play: tri (c.x + s * 0.15f, true); break;
            case rec: p.addEllipse (c.x - s * 0.75f, c.y - s * 0.75f, s * 1.5f, s * 1.5f); break;
        }
        g.setColour (on ? vegapaint::keyInk (true) : icon == rec ? juce::Colour (0xffff5a5a) : VegaColours::text);
        g.fillPath (p);
    }
};

/** Endless chrome encoder (knurled skirt, black top, white index dot) */
class Encoder : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (double)> onDelta;    // +/- per drag step (1.0 = one full sweep)
    std::function<void()> onReset;
    void paint (juce::Graphics& g) override
    {
        // the plug-ins' knob; endless, so a ring of dots instead of end ticks
        auto r = getLocalBounds().toFloat();
        const auto c = r.getCentre(); const float outer = std::min (r.getWidth(), r.getHeight()) * 0.5f;
        g.setColour (VegaColours::textDim.withAlpha (0.6f));
        for (int i = 0; i < 24; ++i)
        {
            const float a = i * juce::MathConstants<float>::twoPi / 24.0f;
            g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre ({ c.x + std::sin (a) * outer * 0.95f, c.y - std::cos (a) * outer * 0.95f }));
        }
        vegapaint::knob (g, juce::Rectangle<float> (outer * 1.5f, outer * 1.5f).withCentre (c), angle, false, juce::Colours::white, isMouseOver());
    }
    void mouseDown (const juce::MouseEvent& e) override { lastY = e.position.y; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const double d = (lastY - e.position.y) / 200.0; lastY = e.position.y;
        angle += (float) (d * juce::MathConstants<double>::twoPi);
        if (onDelta) onDelta (d);
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { const double d = w.deltaY * 0.08; angle += (float) (d * 6.28); if (onDelta) onDelta (d); repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onReset) onReset(); }
private:
    float angle = 0, lastY = 0;
};
} // namespace uf

/** Left-hand column, styled after a console's channel / transport control unit, drawn on a fixed
    460 x 880 canvas and scaled exactly like the master section. */
class MixerView::ControlPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int CW = 460, CH = 880;
    int widthForHeight (int h) const { return juce::roundToInt (CW * (float) h / CH); }

    ControlPanel (MixerView& o, ViewState& v) : owner (o), vs (v)
    {
        addAndMakeVisible (canvas);
        canvas.setSize (CW, CH);
        canvas.paintFn = [this] (juce::Graphics& g) { paintCanvas (g); };

        // soft keys: VU (meters) / EDIT (inserts) / SHIFT (pads arm/select) / LEARN (MIDI learn)
        for (int i = 0; i < 4; ++i) { canvas.addAndMakeVisible (soft[i]); soft[i].setBounds (54 + i * 96, 22, 60, 28); }
        soft[0].setToggleState (true, juce::dontSendNotification);
        soft[0].onClick = [this] { soft[0].setToggleState (true, juce::dontSendNotification); peak[0] = peak[1] = -100; owner.touched ("Meters", "Peak hold reset"); };
        soft[1].onClick = [this] { owner.showEdit = ! owner.showEdit; owner.touched ("Edit", owner.showEdit ? "Inserts shown" : "Inserts hidden"); owner.layoutChanged(); };
        soft[2].onClick = [this] { owner.shift = ! owner.shift; owner.touched ("Pads", owner.shift ? "Arm / Select" : "Solo / Mute"); };
        soft[3].onClick = [this] { auto& ls = vs.edit().engine.getMidiLearnState(); ls.setActive (! ls.isActive()); owner.touched ("MIDI learn", ls.isActive() ? "Move a control, then a knob" : "Off"); };
        soft[0].setTooltip ("VU meters (click to reset the peak hold)"); soft[1].setTooltip ("Edit: show / hide each channel's instrument, inserts and sends");
        soft[2].setTooltip ("Shift: the pads become Record Arm (top) and Select (bottom)"); soft[3].setTooltip ("MIDI learn: move a control on screen, then a knob on your controller");

        // 4 encoders under the screen (page 1: volume / pan / filter / VU ref, page 2 (5-8): HI / MID / LOW / master)
        for (int i = 0; i < 4; ++i)
        {
            canvas.addAndMakeVisible (enc[i]); enc[i].setBounds (encX (i) - 30, 334, 60, 60);
            enc[i].onDelta = [this, i] (double d) { encoderMoved (i, d); };
            enc[i].onReset = [this, i] { encoderReset (i); };
        }
        // (CHANNEL, MODE, 5-8, BANK, LAYER, SCRUB and the arrows gave their place to the tape machine)
        channel.onDelta = [this] (double d) { chanAcc += d * 12.0; while (chanAcc >= 1) { owner.selectNeighbour (1); chanAcc -= 1; } while (chanAcc <= -1) { owner.selectNeighbour (-1); chanAcc += 1; } };
        channel.setTooltip ("CHANNEL: turn to select tracks");

        auto key = [this] (uf::Key& k, int x, int y, int w = 40, int h = 34) { canvas.addAndMakeVisible (k); k.setBounds (x, y, w, h); };

        mode.setClickingTogglesState (true); page58.setClickingTogglesState (true); layer.setClickingTogglesState (true); scrub.setClickingTogglesState (true);
        mode.setTooltip ("MODE: the arrow keys zoom instead of moving"); page58.setTooltip ("5-8: encoders control HI / MID / LOW / master volume");
        chanL.setTooltip ("Shift the mixer by one channel"); chanR.setTooltip ("Shift the mixer by one channel");
        bankL.setTooltip ("Previous 8 channels"); bankR.setTooltip ("Next 8 channels");
        layer.setTooltip ("LAYER: rewind / fast-forward nudge the selected clip by one grid step"); scrub.setTooltip ("SCRUB: hear the audio as you drag the tape reels");
        mode.onClick = [this] { owner.touched ("Mode", mode.getToggleState() ? "Arrows zoom" : "Arrows navigate"); };
        page58.onClick = [this] { owner.touched ("Encoders", page58.getToggleState() ? "5-8: Hi / Mid / Low / Master" : "1-4: Vol / Pan / Filter / VU ref"); };
        chanL.onClick = [this] { owner.scrollStrips (-1); }; chanR.onClick = [this] { owner.scrollStrips (1); };
        bankL.onClick = [this] { owner.pageBy (-1); }; bankR.onClick = [this] { owner.pageBy (1); };
        layer.onClick = [this] { owner.touched ("Nudge", layer.getToggleState() ? "<< >> move the clip" : "Off"); };
        scrub.onClick = [this] { owner.touched ("Jog wheel", scrub.getToggleState() ? "Scrub" : "Move playhead"); };

        // navigation cluster + jog
        // (the old arrow cluster + jog wheel became the tape deck; the arrow actions stay on the keys' callbacks)
        navUp.onClick = [this] { if (mode.getToggleState()) vZoom (1); else owner.selectNeighbour (-1); };
        navDown.onClick = [this] { if (mode.getToggleState()) vZoom (-1); else owner.selectNeighbour (1); };
        navLeft.onClick = [this] { if (mode.getToggleState()) hZoom (-1); else moveBars (-1); };
        navRight.onClick = [this] { if (mode.getToggleState()) hZoom (1); else moveBars (1); };
        navDot.onClick = [this] { toggleMarker(); };
        navUp.setTooltip ("Previous track (MODE: taller tracks)"); navDown.setTooltip ("Next track (MODE: shorter tracks)");
        navLeft.setTooltip ("Back one bar (MODE: zoom out)"); navRight.setTooltip ("Forward one bar (MODE: zoom in)");
        navDot.setTooltip ("Drop a marker at the playhead (or remove the one there)");
        // ---- the master tape machine: reels + DRIVE / IPS / CAL, On / Bypass, presets
        canvas.addAndMakeVisible (deck); deck.setBounds (28, 404, 412, 168);
        deck.onTurn = [this] (double t) { jogTurn (t); };
        deck.onTouch = [this] (bool d) { if (! vs.edit().getTransport().isPlaying()) vs.edit().getTransport().setUserDragging (d); };
        deck.setTooltip ("Vega Tape 2T on the master bus. Drag a reel (or use the wheel) to move the playhead - stopped, you hear the tape scrub.");
        static const char* tapeNames[] { "Drive", "IPS", "Cal" };
        for (int i = 0; i < 3; ++i)
        {
            auto& k = tapeKnob[i];
            k.getProperties().set ("big", true);
            canvas.addAndMakeVisible (k); k.setBounds (99 + i * 90, 574, 90, 76);
            k.onValueChange = [this, i]
            {
                if (auto* tp = tape()) { (i == 0 ? tp->drive : i == 1 ? tp->ips : tp->cal)->setParameter ((float) tapeKnob[i].getValue(), juce::sendNotification); }
                owner.touched (juce::String ("Tape ") + tapeNames[i], tapeKnob[i].getTextFromValue (tapeKnob[i].getValue()));
            };
        }
        tapeKnob[0].setRange (0.0, 1.0, 0.01); tapeKnob[0].setDoubleClickReturnValue (true, 0.35);
        tapeKnob[0].textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100)) + "%"; };
        tapeKnob[1].setRange (0.0, 2.0, 1.0); tapeKnob[1].setDoubleClickReturnValue (true, 1.0);
        tapeKnob[1].textFromValueFunction = [] (double v) { return VegaTapeMachinePlugin::ipsName ((float) v); };
        tapeKnob[2].setRange (-6.0, 6.0, 0.1); tapeKnob[2].setDoubleClickReturnValue (true, 0.0);
        tapeKnob[2].textFromValueFunction = [] (double v) { return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
        tapeKnob[0].setTooltip ("DRIVE: how hard the mix hits the tape (saturation and glue)");
        tapeKnob[1].setTooltip ("IPS: tape speed - 7.5 is darker and fatter, 15 warm, 30 clean and open");
        tapeKnob[2].setTooltip ("CAL: output calibration trim");
        for (auto* b : { &tapeOn, &tapeBypass }) canvas.addAndMakeVisible (b);
        tapeOn.setBounds (184, 662, 44, 38); tapeBypass.setBounds (238, 662, 50, 38);
        tapeOn.onClick = [this] { if (auto* tp = tape()) { tp->setEnabled (true); owner.touched ("Tape", "On"); } };
        tapeBypass.onClick = [this] { if (auto* tp = tape()) { tp->setEnabled (false); owner.touched ("Tape", "Bypassed"); } };
        tapeOn.setTooltip ("Tape machine on (master bus)"); tapeBypass.setTooltip ("Bypass the tape machine");
        canvas.addAndMakeVisible (tapePreset); tapePreset.setBounds (92, 704, 348, 22);
        tapePreset.onClick = [this] { presetMenu(); }; tapePreset.setTooltip ("Tape presets");
        for (auto* b : { &tapeDel, &tapeRen, &tapeSave }) { canvas.addAndMakeVisible (b); }
        tapeDel.setBounds (28, 730, 134, 24); tapeRen.setBounds (167, 730, 134, 24); tapeSave.setBounds (306, 730, 134, 24);
        tapeSave.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0d0d0));
        tapeSave.setToggleState (true, juce::dontSendNotification); tapeSave.setClickingTogglesState (false);
        tapeDel.onClick = [this] { deletePreset(); }; tapeRen.onClick = [this] { renamePreset(); }; tapeSave.onClick = [this] { savePreset(); };

        // automation / function row
        juce::Button* row[] { &autoOff, &autoRead, &autoWrite, &cycle, &click, &drop, &shiftKey };
        for (int i = 0; i < 7; ++i) { canvas.addAndMakeVisible (row[i]); row[i]->setBounds (36 + i * 56, 760, 38, 32); }
        autoOff.setTooltip ("Automation OFF (no read, no write)"); autoRead.setTooltip ("Automation READ"); autoWrite.setTooltip ("Automation WRITE: record moves while playing");
        cycle.setTooltip ("Cycle (loop)"); click.setTooltip ("Metronome"); drop.setTooltip ("Drop-in: record only inside the cycle range"); shiftKey.setTooltip ("Shift: Rew / Ffwd jump between markers");
        autoOff.onClick = [this] { auto& a = vs.edit().getAutomationRecordManager(); a.setReadingAutomation (false); a.setWritingAutomation (false); owner.touched ("Automation", "Off"); };
        autoRead.onClick = [this] { auto& a = vs.edit().getAutomationRecordManager(); a.setReadingAutomation (true); a.setWritingAutomation (false); owner.touched ("Automation", "Read"); };
        autoWrite.onClick = [this] { auto& a = vs.edit().getAutomationRecordManager(); a.setReadingAutomation (true); a.setWritingAutomation (! a.isWritingAutomation()); owner.touched ("Automation", a.isWritingAutomation() ? "Write" : "Read"); };
        cycle.onClick = [this] { auto& t = vs.edit().getTransport(); t.looping = ! t.looping.get(); if (t.looping && t.getLoopRange().getLength() <= 0s) t.setLoopRange ({ 0s, vs.session.barLength() * 4 }); owner.touched ("Cycle", t.looping ? "On" : "Off"); };
        click.onClick = [this] { vs.edit().clickTrackEnabled = ! vs.edit().clickTrackEnabled.get(); owner.touched ("Click", vs.edit().clickTrackEnabled.get() ? "On" : "Off"); };
        drop.onClick = [this] { auto& e = vs.edit(); e.recordingPunchInOut = ! e.recordingPunchInOut.get(); owner.touched ("Drop-in", e.recordingPunchInOut ? "Punch inside the cycle" : "Off"); };
        shiftKey.setClickingTogglesState (true);

        // transport
        juce::Button* tk[] { &rew, &ffwd, &stop, &play, &rec };
        for (int i = 0; i < 5; ++i) { canvas.addAndMakeVisible (tk[i]); tk[i]->setBounds (30 + i * 82, 808, 70, 46); }
        rew.onClick = [this] { if (! held) step (-1); held = false; };
        ffwd.onClick = [this] { if (! held) step (1); held = false; };
        stop.onClick = [this] { auto& t = vs.edit().getTransport(); if (t.isPlaying() || t.isRecording()) t.stop (false, false); else t.setPosition (0s); };
        play.onClick = [this] { auto& t = vs.edit().getTransport(); if (t.isPlaying()) t.stop (false, false); else t.play (false); };
        rec.onClick = [this] { auto& t = vs.edit().getTransport(); if (t.isRecording()) t.stop (true, false); else t.record (false); };
        rew.setTooltip ("Back one bar (hold to keep going). SHIFT: previous marker. LAYER: nudge clip"); ffwd.setTooltip ("Forward one bar (hold to keep going). SHIFT: next marker. LAYER: nudge clip");
        stop.setTooltip ("Stop (again: return to zero)");
        startTimerHz (30);
    }
    ~ControlPanel() override { forgetEdit(); }
    /** Stop metering the current Edit (called before it's destroyed) */
    void forgetEdit() { if (masterAdded && masterCtx != nullptr) masterCtx->masterLevels.removeClient (client); masterAdded = false; masterCtx = nullptr; }

    void show (const juce::String& a, const juce::String& b) { line1 = a; line2 = b; canvas.repaint(); }
    void resized() override { canvas.setTransform (juce::AffineTransform::scale ((float) getHeight() / CH)); }

private:
    struct Canvas : juce::Component
    {
        std::function<void (juce::Graphics&)> paintFn;
        void paint (juce::Graphics& g) override { if (paintFn) paintFn (g); }
    };
    static int encX (int i) { return 84 + i * 97; }

    // ---------------------------------------------------------------- encoders
    te::AudioTrack* target() { return vs.getSelectedTrack(); }
    void encoderMoved (int i, double d)
    {
        auto* t = target();
        const bool p2 = page58.getToggleState();
        if (! p2 && i == 3) { vuRef = juce::jlimit (-24.0f, -8.0f, vuRef + (float) d * 16.0f); owner.touched ("VU reference", juce::String (vuRef, 1) + " dBFS = 0 VU"); return; }
        if (p2 && i == 3)
        {
            if (auto vp = vs.edit().getMasterVolumePlugin()) { vp->setVolumeDb (juce::jlimit (-60.0f, 6.0f, vp->getVolumeDb() + (float) d * 30.0f)); owner.touched ("Master", juce::String (vp->getVolumeDb(), 1) + " dB"); }
            return;
        }
        if (t == nullptr) { owner.touched ("Encoders", "Select a track first"); return; }
        if (! p2 && i == 0) { if (auto vp = t->getVolumePlugin()) { vp->setVolumeDb (juce::jlimit (-60.0f, 6.0f, vp->getVolumeDb() + (float) d * 30.0f)); owner.touched (t->getName() + "  Volume", juce::String (vp->getVolumeDb(), 1) + " dB"); } return; }
        if (! p2 && i == 1) { if (auto vp = t->getVolumePlugin()) { vp->setPan (juce::jlimit (-1.0f, 1.0f, vp->getPan() + (float) d * 2.0f)); owner.touched (t->getName() + "  Pan", panText (vp->getPan())); } return; }
        auto* dj = vs.session.djEq (*t, false);
        if (dj == nullptr) return;
        if (! p2 && i == 2) { dj->filter->setParameter (juce::jlimit (-1.0f, 1.0f, dj->filter->getCurrentValue() + (float) d * 2.0f), juce::sendNotification); owner.touched (t->getName() + "  Filter", VegaDJEQPlugin::describeFilter (dj->filter->getCurrentValue())); return; }
        if (p2 && i < 3)
        {
            auto& b = dj->band[i];
            b->setParameter (juce::jlimit (VegaDJEQPlugin::killDb, VegaDJEQPlugin::maxDb, b->getCurrentValue() + (float) d * 46.0f), juce::sendNotification);
            static const char* nm[] { "Hi", "Mid", "Low" };
            owner.touched (t->getName() + "  " + nm[i], b->getCurrentValueAsString());
        }
    }
    void encoderReset (int i)
    {
        auto* t = target(); const bool p2 = page58.getToggleState();
        if (! p2 && i == 3) { vuRef = -18.0f; return; }
        if (p2 && i == 3) { if (auto vp = vs.edit().getMasterVolumePlugin()) vp->setVolumeDb (0.0f); return; }
        if (t == nullptr) return;
        if (! p2 && i == 0) { if (auto vp = t->getVolumePlugin()) vp->setVolumeDb (0.0f); return; }
        if (! p2 && i == 1) { if (auto vp = t->getVolumePlugin()) vp->setPan (0.0f); return; }
        if (auto* dj = vs.session.djEq (*t, false)) { if (! p2 && i == 2) dj->filter->setParameter (0.0f, juce::sendNotification); else if (p2) dj->band[i]->setParameter (0.0f, juce::sendNotification); }
    }
    static juce::String panText (float v) { return std::abs (v) < 0.01f ? juce::String ("C") : juce::String (juce::roundToInt (std::abs (v) * 100)) + (v < 0 ? "L" : "R"); }
    juce::String encLabel (int i, juce::String& value)
    {
        auto* t = target(); const bool p2 = page58.getToggleState();
        value = "-";
        if (! p2 && i == 3) { value = juce::String (vuRef, 1) + " dB"; return "Ref"; }
        if (p2 && i == 3) { if (auto vp = vs.edit().getMasterVolumePlugin()) value = juce::String (vp->getVolumeDb(), 1); return "Master"; }
        if (t == nullptr) return i == 0 ? "Volume" : i == 1 ? "Pan" : i == 2 ? "Filter" : "";
        if (! p2 && i == 0) { if (auto vp = t->getVolumePlugin()) value = juce::String (vp->getVolumeDb(), 1); return "Volume"; }
        if (! p2 && i == 1) { if (auto vp = t->getVolumePlugin()) value = panText (vp->getPan()); return "Pan"; }
        if (auto* dj = vs.session.djEq (*t, false))
        {
            if (! p2 && i == 2) { value = VegaDJEQPlugin::describeFilter (dj->filter->getCurrentValue()); return "Filter"; }
            if (p2) { const float v = dj->band[i]->getCurrentValue(); value = v <= VegaDJEQPlugin::killDb + 0.05f ? juce::String ("KILL") : juce::String (v, 1); return i == 0 ? "Hi" : i == 1 ? "Mid" : "Low"; }
        }
        return {};
    }

    // ---------------------------------------------------------------- transport helpers
    void step (int dir)
    {
        auto& t = vs.edit().getTransport();
        if (shiftKey.getToggleState() || juce::ModifierKeys::currentModifiers.isShiftDown())
        {
            auto& mm = vs.edit().getMarkerManager();
            auto* m = dir > 0 ? mm.getNextMarker (t.getPosition() + te::TimeDuration::fromSeconds (0.001)) : mm.getPrevMarker (t.getPosition() - te::TimeDuration::fromSeconds (0.001));
            if (m != nullptr) { t.setPosition (m->getPosition().getStart()); owner.touched ("Marker", m->getName()); }
            return;
        }
        if (layer.getToggleState())
        {
            if (auto* c = vs.getSelectedClip())
            {
                const double grid = vs.gridBeats();
                c->setStart (vs.beatsToTime (std::max (0.0, vs.timeToBeats (c->getPosition().getStart()) + dir * grid)), false, true);
                owner.touched ("Nudge " + c->getName(), dawui::formatBarsBeats (vs.edit(), c->getPosition().getStart()));
            }
            else owner.touched ("Nudge", "Select a clip first");
            return;
        }
        moveBars (dir);
    }
    void moveBars (int dir)
    {
        auto& t = vs.edit().getTransport();
        const double bar = vs.session.barLength().inSeconds(), now = t.getPosition().inSeconds();
        t.setPosition (te::TimePosition::fromSeconds (dir > 0 ? (std::floor (now / bar + 1e-6) + 1) * bar : std::max (0.0, (std::ceil (now / bar - 1e-6) - 1) * bar)));
        owner.touched ("Position", dawui::formatBarsBeats (vs.edit(), t.getPosition()));
    }
    void hZoom (int dir) { vs.pixelsPerBeat = juce::jlimit (2.0, 400.0, vs.pixelsPerBeat * (dir > 0 ? 1.25 : 0.8)); owner.touched ("Zoom", juce::String (vs.pixelsPerBeat, 0) + " px / beat"); }
    void vZoom (int dir)
    {
        for (auto* t : vs.session.getTracks (true)) t->state.setProperty ("vegaHeight", juce::jlimit (44, 260, (int) t->state.getProperty ("vegaHeight", 84) + dir * 16), nullptr);
        owner.touched ("Track height", dir > 0 ? "Taller" : "Shorter");
    }
    void toggleMarker()
    {
        auto& mm = vs.edit().getMarkerManager();
        const auto now = vs.edit().getTransport().getPosition();
        for (auto& m : mm.getMarkers())
            if (std::abs ((m->getPosition().getStart() - now).inSeconds()) < 0.05) { m->removeFromParent(); owner.touched ("Marker", "Removed"); return; }
        int next = 1; for (auto& m : mm.getMarkers()) next = std::max (next, m->getMarkerID() + 1);
        if (mm.createMarker (next, now, vs.session.barLength(), nullptr) != nullptr) owner.touched ("Marker " + juce::String (next), dawui::formatBarsBeats (vs.edit(), now));
    }
    void jogTurn (double turns)
    {
        auto& t = vs.edit().getTransport();
        if (scrub.getToggleState()) { te::scrub (t, turns * 40.0); return; }
        t.setPosition (vs.beatsToTime (std::max (0.0, vs.timeToBeats (t.getPosition()) + turns * 4.0)));
        owner.touched ("Jog", dawui::formatBarsBeats (vs.edit(), t.getPosition()));
    }

    // ---------------------------------------------------------------- metering + state
    void timerCallback() override
    {
        auto& t = vs.edit().getTransport();
        auto set = [] (juce::Button& b, bool on) { if (b.getToggleState() != on) b.setToggleState (on, juce::dontSendNotification); };
        auto& arm = vs.edit().getAutomationRecordManager();
        set (rec, t.isRecording()); set (play, t.isPlaying()); set (cycle, t.looping.get()); set (drop, vs.edit().recordingPunchInOut.get());
        set (click, vs.edit().clickTrackEnabled.get());
        set (autoWrite, arm.isWritingAutomation()); set (autoRead, arm.isReadingAutomation() && ! arm.isWritingAutomation()); set (autoOff, ! arm.isReadingAutomation() && ! arm.isWritingAutomation());
        set (soft[1], owner.showEdit); set (soft[2], owner.shift); set (soft[3], vs.edit().engine.getMidiLearnState().isActive());
        for (auto [k, dir] : { std::pair<juce::Button*, int> { &rew, -1 }, { &ffwd, 1 } })
        {
            if (k->isDown())
            {
                if (++holdTicks[dir > 0] > 9)
                {
                    held = true;
                    t.setPosition (vs.beatsToTime (std::max (0.0, vs.timeToBeats (t.getPosition()) + dir * 0.6)));
                    owner.touched (dir > 0 ? "Fast forward" : "Rewind", dawui::formatBarsBeats (vs.edit(), t.getPosition()));
                }
            }
            else holdTicks[dir > 0] = 0;
        }
        // master levels -> VU ballistics (~300 ms integration) + peak hold
        if (auto* ctx = vs.edit().getCurrentPlaybackContext())
        {
            if (! masterAdded || masterCtx != ctx)
            {
                if (masterAdded && masterCtx != nullptr) masterCtx->masterLevels.removeClient (client);
                ctx->masterLevels.addClient (client); masterAdded = true; masterCtx = ctx;
            }
            for (int c = 0; c < 2; ++c)
            {
                const float db = client.getAndClearAudioLevel (c).dB;
                const float vu = db - vuRef - 3.0f;                         // peak -> approx. average, referenced to 0 VU
                const float target = juce::jlimit (-25.0f, 4.0f, db < -99 ? -25.0f : vu);
                vuDb[c] += (target - vuDb[c]) * 0.18f;
                current[c] = db < -99 ? -100.0f : db;
                if (current[c] > peak[c]) peak[c] = current[c];
            }
        }
        if (auto* tp = tape())
        {
            const float vals[] { tp->drive->getCurrentValue(), tp->ips->getCurrentValue(), tp->cal->getCurrentValue() };
            for (int i = 0; i < 3; ++i) if (! tapeKnob[i].isMouseButtonDown() && std::abs (tapeKnob[i].getValue() - vals[i]) > 0.001) tapeKnob[i].setValue (vals[i], juce::dontSendNotification);
            set (tapeOn, tp->isEnabled()); set (tapeBypass, ! tp->isEnabled());
            if (tapePreset.getButtonText() != tp->presetName.get()) tapePreset.setButtonText (tp->presetName.get());
            const int sp = juce::jlimit (0, 2, juce::roundToInt (tp->ips->getCurrentValue()));
            deck.ipsFactor = sp == 0 ? 0.5f : sp == 1 ? 1.0f : 2.0f;
            deck.ipsText = VegaTapeMachinePlugin::ipsName ((float) sp).toUpperCase(); deck.on = tp->isEnabled();
        }
        {
            const double len = vs.edit().getLength().inSeconds();
            deck.setLevel (std::max (current[0], current[1]));
            deck.update (t.getPosition().inSeconds(), len, t.isPlaying(), t.isRecording(), dawui::formatBarsBeats (vs.edit(), t.getPosition()));
        }
        canvas.repaint();
    }

    // ---------------------------------------------------------------- drawing
    void paintCanvas (juce::Graphics& g)
    {
        if (plate.isNull()) plate = makePlate();
        g.drawImageAt (plate, 0, 0);
        // screen
        auto scr = juce::Rectangle<float> (30, 70, 400, 250);
        g.setColour (juce::Colour (0xff121214)); g.fillRoundedRectangle (scr.expanded (6), 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (scr.expanded (6), 6.0f, 2.0f);
        g.setColour (juce::Colour (0xff070809)); g.fillRect (scr);
        // tab bar (soft key labels)
        auto tabs = scr.removeFromTop (20.0f);
        g.setColour (VegaColours::bar); g.fillRect (tabs);
        static const char* tabNames[] { "VU METERS", "EDIT", "SHIFT", "LEARN" };
        for (int i = 0; i < 4; ++i)
        {
            auto tr = juce::Rectangle<float> (tabs.getX() + i * 100.0f, tabs.getY(), 100.0f, tabs.getHeight());
            if (soft[i].getToggleState()) { g.setColour (VegaColours::accent); g.fillRect (tr.reduced (6, 2)); }
            g.setColour (soft[i].getToggleState() ? VegaColours::bar : VegaColours::text);
            g.setFont (VegaFonts::ui (11.0f, true)); g.drawText (tabNames[i], tr, juce::Justification::centred);
        }
        g.setColour (juce::Colour (0xff141414)); g.fillRect (scr);
        // touched line
        g.setColour (VegaColours::accent); g.setFont (VegaFonts::ui (11.0f, true));
        g.drawText (line1 + (line2.isNotEmpty() ? "   " + line2 : juce::String()), scr.removeFromTop (18.0f).reduced (8, 0).toNearestInt(), juce::Justification::centred, true);
        // two VU meters
        for (int c = 0; c < 2; ++c)
        {
            auto mr = juce::Rectangle<float> (scr.getX() + 12 + c * 194.0f, scr.getY() + 6, 182.0f, 118.0f);
            g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (11.0f, true));
            g.drawText (c == 0 ? "L" : "R", mr.withHeight (12).translated (0, -4), juce::Justification::centred);
            drawVu (g, mr.withTrimmedTop (10), vuDb[c]);
            // current / max
            auto rd = juce::Rectangle<float> (mr.getX(), mr.getBottom() + 4, mr.getWidth(), 34);
            g.setFont (VegaFonts::ui (9.5f)); g.setColour (VegaColours::textDim);
            g.drawText ("current", rd.withWidth (rd.getWidth() / 2).withHeight (12), juce::Justification::centred);
            g.setColour (juce::Colour (0xffff5a5a)); g.drawText ("max", rd.withTrimmedLeft (rd.getWidth() / 2).withHeight (12), juce::Justification::centred);
            g.setFont (VegaFonts::ui (16.0f, true)); g.setColour (juce::Colours::white);
            g.drawText (current[c] < -99 ? juce::String ("-inf") : juce::String (current[c], 1) + " dB", rd.withWidth (rd.getWidth() / 2).withTrimmedTop (12), juce::Justification::centred);
            g.setColour (juce::Colour (0xffff5a5a));
            g.drawText (peak[c] < -99 ? juce::String ("-inf") : juce::String (peak[c], 1) + " dB", rd.withTrimmedLeft (rd.getWidth() / 2).withTrimmedTop (12), juce::Justification::centred);
        }
        // bottom bar: what each encoder does
        auto bar = juce::Rectangle<float> (scr.getX(), 300, scr.getWidth(), 20);
        g.setColour (VegaColours::bar); g.fillRect (bar);
        for (int i = 0; i < 4; ++i)
        {
            juce::String value; const auto name = encLabel (i, value);
            auto seg = juce::Rectangle<float> ((float) encX (i) - 48, bar.getY(), 96, bar.getHeight());
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.5f, true));
            g.drawText (name, seg.withWidth (44).toNearestInt(), juce::Justification::centredRight);
            g.setColour (juce::Colour (0xff050505)); g.fillRect (seg.withTrimmedLeft (48).reduced (0, 3));
            g.setColour (VegaColours::accent); g.drawText (value, seg.withTrimmedLeft (48).toNearestInt(), juce::Justification::centred);
        }
        g.setColour (juce::Colour (0xff3a3c40));
        for (int i = 1; i < 4; ++i) g.fillRect ((float) encX (i) - 49, bar.getY() + 3, 1.0f, bar.getHeight() - 6);
        // labels + channel arcs
        // tape machine captions
        g.setFont (VegaFonts::ui (10.5f, true)); g.setColour (console::inkDim);
        static const char* tn[] { "DRIVE", "IPS", "CAL" };
        for (int k = 0; k < 3; ++k) g.drawText (tn[k], 99 + k * 90, 648, 90, 12, juce::Justification::centred);
        g.drawText ("PRESET", 28, 704, 60, 22, juce::Justification::centredLeft);

        // function row labels
        static const char* fn[] { "OFF", "READ", "WRT", "CYCLE", "CLICK", "DROP", "SHIFT" };
        for (int i = 0; i < 7; ++i)
        {
            auto lr = juce::Rectangle<float> (36.0f + i * 56 - 4, 795, 46, 13);
            g.setColour (console::inkDim); g.setFont (VegaFonts::ui (9.5f, true)); g.drawText (fn[i], lr, juce::Justification::centred);
        }
        // brand
        g.setColour (console::line); g.fillRect (24.0f, 752.0f, (float) CW - 48, 2.0f);
        g.setColour (console::inkDim); g.setFont (VegaFonts::ui (11.0f, true));
        g.drawText ("Vega Productions", 30, 858, 200, 14, juce::Justification::centredLeft);
        g.setFont (VegaFonts::ui (9.5f)); g.drawText ("CONTROL SECTION", CW - 230, 858, 200, 14, juce::Justification::centredRight);
    }

    void drawVu (juce::Graphics& g, juce::Rectangle<float> r, float vu)
    {
        // backlit warm-blue VU face
        vegapaint::vuFace (g, r);
        auto face = r.reduced (5.0f);
        const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.55f);
        const float rad = face.getHeight() * 1.04f;
        auto angleFor = [] (float v)
        {
            const float n = std::pow (juce::Decibels::decibelsToGain (juce::jlimit (-20.0f, 3.0f, v)) / juce::Decibels::decibelsToGain (3.0f), 0.85f);
            return juce::degreesToRadians (-40.0f + 80.0f * n);
        };
        juce::Path arc; arc.addCentredArc (pivot.x, pivot.y, rad, rad, 0, angleFor (-20), angleFor (0), true);
        g.setColour (vegapaint::vuInk); g.strokePath (arc, juce::PathStrokeType (1.3f));
        juce::Path red; red.addCentredArc (pivot.x, pivot.y, rad + 2, rad + 2, 0, angleFor (0), angleFor (3), true);
        g.setColour (juce::Colour (0xffff6a5a)); g.strokePath (red, juce::PathStrokeType (3.0f));
        g.setFont (VegaFonts::ui (8.5f, true));
        for (float m : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
        {
            const float a = angleFor (m);
            const auto p1 = pivot.getPointOnCircumference (rad, a), p2 = pivot.getPointOnCircumference (rad + 6, a), pl = pivot.getPointOnCircumference (rad + 13, a);
            g.setColour (m > 0 ? juce::Colour (0xffff6a5a) : vegapaint::vuInk);
            g.drawLine ({ p1, p2 }, 1.2f);
            g.drawText (juce::String ((int) std::abs (m)), juce::Rectangle<float> (16, 10).withCentre (pl), juce::Justification::centred);
        }
        g.setColour (vegapaint::vuInk); g.setFont (VegaFonts::ui (11.0f, true));
        g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.55f).withHeight (14), juce::Justification::centred);
        g.saveState(); g.reduceClipRegion (face.toNearestInt());
        const float a = angleFor (vu);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawLine ({ pivot.translated (1.5f, 1.5f), pivot.getPointOnCircumference (rad + 4, a).translated (1.5f, 1.5f) }, 1.6f);
        g.setColour (vegapaint::vuNeedle); g.drawLine ({ pivot, pivot.getPointOnCircumference (rad + 4, a) }, 1.6f);
        g.restoreState();
    }

    juce::Image makePlate()
    {
        juce::Image img (juce::Image::RGB, CW, CH, true);
        juce::Graphics g (img);
        g.setGradientFill (juce::ColourGradient (VegaColours::panelLight, 0, 0, VegaColours::panel, 0, (float) CH, false));
        g.fillAll();
        g.setColour (VegaColours::line); g.drawRect (0, 0, CW, CH, 1);
        return img;
    }

    MixerView& owner; ViewState& vs;
    Canvas canvas; juce::Image plate;
    uf::SoftKey soft[4];
    uf::Encoder enc[4], channel;
    uf::Key mode { uf::Key::text, "MODE", juce::Colour (0xffffd98a), false }, page58 { uf::Key::text, "5-8", juce::Colour (0xffffd98a), false },
            chanL { uf::Key::left, {}, juce::Colour (0xffffd98a), false }, chanR { uf::Key::right, {}, juce::Colour (0xffffd98a), false },
            bankL { uf::Key::left }, bankR { uf::Key::right }, layer { uf::Key::text, juce::String::fromUTF8 ("360\xc2\xb0"), juce::Colour (0xffffd98a), false },
            scrub { uf::Key::none, {}, juce::Colour (0xffff8a7a), false },
            navUp { uf::Key::up, {}, juce::Colour (0xffffd98a), false }, navDown { uf::Key::down, {}, juce::Colour (0xffffd98a), false },
            navLeft { uf::Key::left, {}, juce::Colour (0xffffd98a), false }, navRight { uf::Key::right, {}, juce::Colour (0xffffd98a), false },
            navDot { uf::Key::dot, {}, juce::Colour (0xffffd98a), false },
            autoOff { uf::Key::left, {}, juce::Colour (0xffffd98a) }, autoRead { uf::Key::right, {}, juce::Colour (0xff7dff5a) }, autoWrite { uf::Key::write, {}, juce::Colour (0xffff6a6a) },
            cycle { uf::Key::loop, {}, juce::Colour (0xffffd98a) }, click { uf::Key::metro, {}, juce::Colour (0xffffd98a) }, drop { uf::Key::dropIn, {}, juce::Colour (0xffff8a7a) },
            shiftKey { uf::Key::shiftUp, {}, juce::Colour (0xffffd98a), false };
    uf::TransportKey rew { uf::TransportKey::rew }, ffwd { uf::TransportKey::ffwd }, stop { uf::TransportKey::stop }, play { uf::TransportKey::play }, rec { uf::TransportKey::rec };
    tp::Jog jog; tp::TapeDeck deck;
    xl::Knob tapeKnob[3];          // the plug-ins' big black knobs, like the Vega Tape panel
    cs::SqButton tapeOn { "On", VegaColours::accent }, tapeBypass { "Bypass", VegaColours::accent };
    cs::ComboButton tapePreset;
    juce::TextButton tapeDel { "DELETE" }, tapeRen { "RENAME" }, tapeSave { "SAVE" };
    VegaTapeMachinePlugin* tape() { return vs.session.masterTape(); }
    void presetMenu()
    {
        auto list = VegaTapeMachinePlugin::presets();
        juce::PopupMenu m;
        m.addSectionHeader ("Vega presets");
        for (int i = 0; i < list.size(); ++i)
        {
            if (i > 0 && ! list[i].builtIn && list[i - 1].builtIn) m.addSectionHeader ("Your presets");
            m.addItem (i + 1, list[i].name, true, tape() != nullptr && tape()->presetName.get() == list[i].name);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&tapePreset), [this, list] (int r)
        {
            if (r < 1) return;
            if (auto* tp = tape()) { tp->apply (list[r - 1]); tp->setEnabled (true); owner.touched ("Tape preset", list[r - 1].name); }
        });
    }
    void askName (const juce::String& title, const juce::String& start, std::function<void (juce::String)> done)
    {
        auto* w = new juce::AlertWindow (title, "Name:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", start); w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int r)
        {
            const auto n = w->getTextEditorContents ("name").trim();
            if (r == 1 && n.isNotEmpty()) done (n);
        }), true);
    }
    bool currentIsUser (juce::String& name)
    {
        auto* tp = tape(); if (tp == nullptr) return false;
        name = tp->presetName.get();
        for (auto& p : VegaTapeMachinePlugin::presets()) if (p.name == name) return ! p.builtIn;
        return false;
    }
    void savePreset()
    {
        juce::String cur; const bool user = currentIsUser (cur);
        askName ("Save tape preset", user ? cur : juce::String ("My Tape"), [this] (juce::String n) { if (auto* tp = tape()) { tp->saveAs (n); owner.touched ("Tape preset saved", n); } });
    }
    void deletePreset()
    {
        juce::String cur;
        if (! currentIsUser (cur)) { owner.touched ("Tape", "Vega presets can't be deleted"); return; }
        auto all = VegaTapeMachinePlugin::presets();
        for (int i = all.size(); --i >= 0;) if (! all[i].builtIn && all[i].name == cur) all.remove (i);
        VegaTapeMachinePlugin::writeUserPresets (all);
        if (auto* tp = tape()) tp->presetName = "Custom";
        owner.touched ("Tape preset deleted", cur);
    }
    void renamePreset()
    {
        juce::String cur;
        if (! currentIsUser (cur)) { owner.touched ("Tape", "Save it as your own preset first"); return; }
        askName ("Rename tape preset", cur, [this, cur] (juce::String n)
        {
            auto all = VegaTapeMachinePlugin::presets();
            for (auto& p : all) if (! p.builtIn && p.name == cur) p.name = n;
            VegaTapeMachinePlugin::writeUserPresets (all);
            if (auto* tp = tape()) tp->presetName = n;
        });
    }
    juce::String line1 { "Vega Productions" }, line2 { "Mixer" };
    float vuRef = -18.0f, vuDb[2] { -25, -25 }, current[2] { -100, -100 }, peak[2] { -100, -100 };
    double chanAcc = 0; int holdTicks[2] {}; bool held = false;
    te::LevelMeasurer::Client client; bool masterAdded = false; te::EditPlaybackContext* masterCtx = nullptr;
};

//==============================================================================
MixerView::MixerView (ViewState& v) : vs (v)
{
    panel = std::make_unique<ControlPanel> (*this, vs);
    addAndMakeVisible (*panel);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    setDetached (false);
    detachBtn.onClick = [this] { if (onDetach) onDetach(); };
    addAndMakeVisible (detachBtn);
    rebuild();
    startTimerHz (40);
}
MixerView::~MixerView() { clearAll(); }
void MixerView::clearAll() { strips.clear(); master.reset(); if (panel != nullptr) panel->forgetEdit(); lastEdit = nullptr; lastLayout.clear(); }

void MixerView::rebuild()
{
    strips.clear();
    int i = 0;
    for (auto* t : vs.session.getTracks (true)) content.addAndMakeVisible (strips.add (new Strip (*this, vs, t, i++)));
    if (master == nullptr || &vs.edit() != lastEdit) { master = std::make_unique<MasterSection> (vs); addAndMakeVisible (*master); lastEdit = &vs.edit(); }
    resized();
}

void MixerView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (headerH);
    detachBtn.setBounds (header.removeFromRight (90).reduced (6, 4));
    r = r.reduced (10, 6);
    const int h = r.getHeight() - 10;
    panel->setBounds (r.removeFromLeft (panel->widthForHeight (h)).withHeight (h));
    r.removeFromLeft (6);
    master->setBounds (r.removeFromRight (master->widthForHeight (h)).withHeight (h));
    r.removeFromRight (10);
    viewport.setBounds (r);
    int editH = 0;
    if (showEdit) for (auto* s : strips) editH = std::max (editH, s->editNeeded());
    int x = 0;
    for (auto* s : strips) { s->editH = editH; s->setBounds (x, 0, Strip::W, h); x += Strip::W; }
    content.setSize (std::max (x, r.getWidth()), h);
    for (auto* s : strips) s->resized();
    repaint();
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (VegaColours::background);
    {   // header bar, like the plug-ins' title bars
        auto hb = getLocalBounds().removeFromTop (headerH);
        g.setColour (VegaColours::bar); g.fillRect (hb);
        g.setColour (VegaColours::line); g.drawHorizontalLine (hb.getBottom() - 1, 0, (float) getWidth());
        g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (13.0f, true));
        g.drawText ("MIXER", hb.withTrimmedLeft (14), juce::Justification::centredLeft);
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.5f));
        g.drawText ("Tab shows / hides the mixer", hb.withTrimmedLeft (74), juce::Justification::centredLeft);
    }
    if (master != nullptr)
    {   // divider before the master
        g.setColour (VegaColours::line); g.fillRect (master->getX() - 6, master->getY(), 1, master->getHeight());
    }
    g.setColour (VegaColours::line); g.drawRect (viewport.getBounds().expanded (1), 1);
}

void MixerView::touched (const juce::String& title, const juce::String& value) { if (panel) panel->show (title, value); }

void MixerView::pageBy (int dir)
{
    const int step = 8 * (Strip::W);
    viewport.setViewPosition (juce::jlimit (0, std::max (0, content.getWidth() - viewport.getWidth()), viewport.getViewPositionX() + dir * step), 0);
    const int first = viewport.getViewPositionX() / (Strip::W) + 1;
    touched ("Page", "Ch " + juce::String (first) + " - " + juce::String (first + 7));
}

void MixerView::scrollStrips (int n)
{
    const int step = Strip::W;
    viewport.setViewPosition (juce::jlimit (0, std::max (0, content.getWidth() - viewport.getWidth()), viewport.getViewPositionX() + n * step), 0);
    const int first = viewport.getViewPositionX() / step + 1;
    touched ("Channel", "Ch " + juce::String (first) + " - " + juce::String (first + 7));
}

void MixerView::selectNeighbour (int dir)
{
    auto tracks = vs.session.getTracks (true);
    if (tracks.isEmpty()) return;
    int i = tracks.indexOf (vs.getSelectedTrack());
    i = juce::jlimit (0, tracks.size() - 1, i < 0 ? 0 : i + dir);
    vs.selectTrack (tracks[i]);
    if (auto* s = strips[i]) viewport.setViewPosition (juce::jlimit (0, std::max (0, content.getWidth() - viewport.getWidth()),
        s->getX() < viewport.getViewPositionX() ? s->getX() : (s->getRight() > viewport.getViewPositionX() + viewport.getWidth() ? s->getRight() - viewport.getWidth() : viewport.getViewPositionX())), 0);
    touched (tracks[i]->getName(), "Selected");
}

void MixerView::timerCallback()
{
    if (! isShowing() || master == nullptr) { if (master == nullptr && isShowing()) rebuild(); return; }
    juce::String layout;
    for (auto* t : vs.session.getTracks (true)) layout << t->itemID.toString() << ",";
    for (auto* s : strips) layout << s->layoutKey();
    if (layout != lastLayout) { lastLayout = layout; rebuild(); }
    for (auto* s : strips) s->refresh();
}

void MixerView::itemDropped (const SourceDetails& d)
{
    te::AudioTrack* t = nullptr;
    const auto sp = localPointToGlobal (d.localPosition.toInt());
    for (auto* s : strips) if (s->getScreenBounds().contains (sp)) t = s->getTrack();
    if (vs.onPluginDropped) vs.onPluginDropped (d.description, t);
}
