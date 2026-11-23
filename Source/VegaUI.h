#pragma once
// =====================================================================
//  Vega Productions — shared UI kit (suite look & feel + widgets)
//  Used by: Mastering, Reverb, Bus Compressor, Channel Strip, Workstation
// =====================================================================
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

// =====================================================================
//  Vega Productions — hardware‑style look & feel
//  (charcoal panel, machined knobs, cyan illuminated switches, OLED)
// =====================================================================
#include "ControlStyle.h"

namespace VegaColours
{
    const juce::Colour background { 0xff1e1e1e };
    const juce::Colour panel      { 0xff2a2a2a };
    const juce::Colour panelLight { 0xff303030 };
    const juce::Colour bar        { 0xff232323 };
    const juce::Colour line       { 0xff3e3e3e };
    const juce::Colour text       { 0xffd8d8d8 };
    const juce::Colour textDim    { 0xff8e8e8e };
    const juce::Colour accent     { 0xff7ff7f2 };
    const juce::Colour accentDim  { 0xff2f6f6c };
    const juce::Colour oled       { 0xff050505 };
    const juce::Colour gold       { 0xffd9b45a };
}

namespace VegaFonts
{
    inline juce::Font ui (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
    inline juce::Font mono (float h)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
    }
}

/** The plug-ins' controls as free painters, so custom components (the DAW's mixer, master section,
    transport keys) draw exactly what the plug-ins draw. */
namespace vegapaint
{
/** Black knob body + face + white pointer (no ticks). `pointer` tints the pointer (band colours). */
inline void knob (juce::Graphics& g, juce::Rectangle<float> knobR, float angle, bool big = false,
                  juce::Colour pointer = juce::Colours::white, bool hover = false)
{
    // themed (Control Creator): knobR is the body, so the wheel's area grows to leave room for its rail
    // no rail here: these callers are endless (jog wheel, bank knob) or draw their own scale
    juce::ignoreUnused (big);
    auto st = vegacontrols::Theme::get().wheel();
    st.rail = 0;
    vegacontrols::paintWheel (g, knobR.withSizeKeepingCentre (knobR.getWidth() / st.size, knobR.getHeight() / st.size), angle, -1.0f, hover, false,
                              pointer == juce::Colours::white || pointer.isTransparent() ? juce::Colour() : pointer, &st);
}

/** The 11 tick marks around a knob */
inline void knobTicks (juce::Graphics&, juce::Rectangle<float>, float, float, bool = false, int = 11)
{
    // the themed wheel draws its own rail / ruler (Control Creator > Wheel > Rail)
}

/** The plug-ins' illuminated square push-button (no label). Lit in `lit` (cyan by default). */
inline void squareButton (juce::Graphics& g, juce::Rectangle<float> sq, bool on, bool over = false, juce::Colour lit = VegaColours::accent)
{
    vegacontrols::paintButton (g, sq, on, over, false, lit == VegaColours::accent ? juce::Colour() : lit);
}

/** A wider key in the same style (bezel, grey face, lit face when on). Returns the face for an icon / text. */
inline juce::Rectangle<float> key (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over, bool down, juce::Colour lit = VegaColours::accent)
{
    return vegacontrols::paintButton (g, r, on, over, down, lit == VegaColours::accent ? juce::Colour() : lit);
}
inline juce::Colour keyInk (bool on) { return vegacontrols::buttonText (on); }

/** The plug-ins' fader: tick marks both sides, thin black slot, dark cap with a white line.
    `capY` is the cap centre. */
inline void fader (juce::Graphics& g, juce::Rectangle<float> area, float capY, bool hover = false, float capW = 28.0f, float capH = 16.0f,
                   juce::Colour line = juce::Colours::white, int ticks = 8, bool litCap = false)
{
    // the cap travels between area.y + 8 and area.bottom - 8 (callers hit-test that range)
    juce::ignoreUnused (line, ticks);
    auto a = area; a.setTop (area.getY() + 8.0f - capH * 0.5f); a.setBottom (area.getBottom() - 8.0f + capH * 0.5f);
    vegacontrols::paintSlider (g, a, capY, true, hover, litCap, {}, nullptr, capW, capH);
}

/** Hardware-style fader (like a control surface's): silver cap with a scooped top and grip ridges,
    long slot with a printed dB scale. `lit` = the selected channel (cyan cap line + glow). */
inline void hardwareFader (juce::Graphics& g, juce::Rectangle<float> area, float capY, bool hover, bool lit, float capW = 34.0f, float capH = 44.0f)
{
    // the cap travels between area.y + capH / 2 and area.bottom - capH / 2
    vegacontrols::paintSlider (g, area, capY, true, hover, lit, {}, nullptr, capW, capH);
}

/** The plug-ins' LED meter column (cyan, white above -18 dB, red at 0), with small blocks and a smooth top:
    the segment the level is inside lights partly, so the meter glides instead of jumping block to block. */
inline float meterFraction (float db)
{
    static constexpr float tDb[] { 6, 0, -6, -12, -18, -24, -30, -42, -60 };
    static constexpr float tF[]  { 1.0f, 0.9f, 0.78f, 0.66f, 0.54f, 0.42f, 0.31f, 0.16f, 0.0f };
    if (db >= tDb[0]) return 1.0f;
    for (int i = 1; i < 9; ++i)
        if (db >= tDb[i]) return tF[i] + (db - tDb[i]) / (tDb[i - 1] - tDb[i]) * (tF[i - 1] - tF[i]);
    return 0.0f;
}
inline void ledColumn (juce::Graphics& g, juce::Rectangle<float> b, float levelDb, float holdDb = -100.0f)
{
    const int n = std::max (12, (int) (b.getHeight() / 3.6f));
    const float segH = b.getHeight() / (float) n;
    const float lf = meterFraction (levelDb), hf = meterFraction (holdDb);
    auto colourAt = [] (float f) { return f >= meterFraction (0.0f) - 0.001f ? juce::Colour (0xffff5a5a) : (f > meterFraction (-18.0f) ? juce::Colours::white : VegaColours::accent); };
    for (int i = 0; i < n; ++i)
    {
        const float f0 = (float) (n - 1 - i) / (float) n, f1 = f0 + 1.0f / (float) n;   // this block's range, bottom to top
        auto seg = juce::Rectangle<float> (b.getX(), b.getY() + i * segH, b.getWidth(), segH).reduced (0, segH * 0.16f);
        const auto c = colourAt (f1 - 0.5f / n);
        g.setColour (juce::Colour (0xff333333)); g.fillRect (seg);
        const float lit = juce::jlimit (0.0f, 1.0f, (lf - f0) * (float) n);
        if (lit > 0.0f) { g.setColour (c.withAlpha (0.25f + 0.75f * lit)); g.fillRect (seg); }
        if (holdDb > -99.0f && hf >= f0 && hf < f1 && lf < f0) { g.setColour (c.withAlpha (0.8f)); g.fillRect (seg); }
    }
}

/** Backlit VU face: warm blue glow, light scale. Draw scale / needle on top in vuInk / vuNeedle. */
inline const juce::Colour vuInk { 0xffe4efff }, vuNeedle { 0xfffff6e8 };
inline void vuFace (juce::Graphics& g, juce::Rectangle<float> r, float corner = 3.0f)
{
    g.setColour (juce::Colour (0xff081626)); g.fillRoundedRectangle (r, corner);
    juce::ColourGradient glow (juce::Colour (0xff4f8fd6), r.getCentreX(), r.getBottom() - r.getHeight() * 0.15f,
                               juce::Colour (0xff10284a), r.getCentreX(), r.getY() - r.getHeight() * 0.6f, true);
    glow.addColour (0.55, juce::Colour (0xff2c64a6));
    g.setGradientFill (glow); g.fillRoundedRectangle (r.reduced (1.0f), corner);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false));
    g.fillRoundedRectangle (r.reduced (1.0f).withHeight (r.getHeight() * 0.45f), corner);     // glass
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

/** Black display screen (inserts, readouts) */
inline void screen (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff141414)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

/** Combo-box look (dark field, outline, chevron) */
inline void comboField (juce::Graphics& g, juce::Rectangle<float> r, bool over)
{
    g.setColour (juce::Colour (0xff1b1b1b)); g.fillRoundedRectangle (r.reduced (0.5f), 2.0f);
    g.setColour (VegaColours::line); g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
    juce::Path p;
    const float cx = r.getRight() - 9.0f, cy = r.getCentreY();
    p.startNewSubPath (cx - 3.5f, cy - 1.5f); p.lineTo (cx, cy + 2.0f); p.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (over ? VegaColours::text : VegaColours::textDim);
    g.strokePath (p, juce::PathStrokeType (1.2f));
}

/** Small-caps section title, as in the plug-ins */
inline void sectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, juce::Colour c = VegaColours::textDim.withAlpha (0.75f))
{
    g.setFont (VegaFonts::ui (9.5f, true)); g.setColour (c);
    g.drawText (t.toUpperCase(), r, juce::Justification::centred);
}
} // namespace vegapaint

class VegaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VegaLookAndFeel()
    {
        using namespace VegaColours;
        setColour (juce::Slider::textBoxTextColourId, textDim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, accentDim);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff151515));
        setColour (juce::Slider::trackColourId, accentDim);
        setColour (juce::Slider::thumbColourId, accent);
        setColour (juce::ProgressBar::foregroundColourId, accent);
        setColour (juce::ProgressBar::backgroundColourId, juce::Colour (0xff151515));
        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.7f));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.25f));
        setColour (juce::Label::textColourId, text);
        setColour (juce::Label::textWhenEditingColourId, text);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff151515));
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, accentDim);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1b1b1b));
        setColour (juce::ComboBox::textColourId, textDim);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1c1c));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3a3a));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0d0d0));
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a1a1a));
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
    }

    // ------------------------------------------------------------------ knobs
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        using namespace VegaColours;
        const bool big = s.getProperties()["big"];
        const auto stepLabels = s.getProperties()["stepLabels"].toString();

        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float tickSpace = big ? 16.0f : 9.0f;
        const float labelSpace = stepLabels.isNotEmpty() ? 18.0f : 0.0f;
        const float size = std::min (area.getWidth() - 2 * (tickSpace + labelSpace),
                                     area.getHeight() - 2 * tickSpace - labelSpace * 1.4f);
        auto knob = juce::Rectangle<float> (size, size).withCentre (area.getCentre().translated (0.0f, labelSpace * 0.5f));
        const auto c = knob.getCentre();
        const float r = size * 0.5f;
        const float angle = startA + pos * (endA - startA);

        if (stepLabels.isNotEmpty())
        {
            auto labels = juce::StringArray::fromTokens (stepLabels, "|", "");
            g.setFont (VegaFonts::ui (9.5f));
            for (int i = 0; i < labels.size(); ++i)
            {
                const float t = labels.size() > 1 ? (float) i / (float) (labels.size() - 1) : 0.0f;
                const float a = vegacontrols::wheelStart + t * (vegacontrols::wheelEnd - vegacontrols::wheelStart);
                const float rl = r + 18.0f;
                auto lc = juce::Point<float> (c.x + rl * std::sin (a), c.y - rl * std::cos (a));
                const bool sel = juce::roundToInt (pos * (labels.size() - 1)) == i;
                g.setColour (sel ? accent : textDim);
                g.drawFittedText (labels[i], juce::Rectangle<int> (40, 12).withCentre (lc.toInt()), juce::Justification::centred, 1);
            }
        }
        juce::ignoreUnused (angle);
        // the themed wheel (Control Creator); it always sweeps its own arc so the rail and pointer agree
        const float themedAngle = vegacontrols::wheelStart + pos * (vegacontrols::wheelEnd - vegacontrols::wheelStart);
        vegacontrols::paintWheel (g, knob.expanded (tickSpace * 0.8f), themedAngle, pos, s.isMouseOverOrDragging(), s.isMouseButtonDown());
    }

    // ------------------------------------------------------------------ vertical fader
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        using namespace VegaColours;
        if (style != juce::Slider::LinearVertical)
        {
            if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
            {
                auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
                g.setColour (juce::Colour (0xff151515)); g.fillRoundedRectangle (r, 2.0f);
                g.setColour (accentDim.withAlpha (s.isEnabled() ? 0.9f : 0.4f));
                if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)   // bipolar: fill from zero
                {
                    const float zx = (float) x + (float) w * (float) s.valueToProportionOfLength (0.0);
                    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (zx, sliderPos), r.getY() + 1, std::max (zx, sliderPos), r.getBottom() - 1));
                    g.setColour (textDim.withAlpha (0.5f)); g.drawVerticalLine ((int) zx, r.getY() + 2, r.getBottom() - 2);
                }
                else
                    g.fillRoundedRectangle (r.withRight (sliderPos), 2.0f);
                g.setColour (line); g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
                return;
            }
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
            return;
        }
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        juce::ignoreUnused (minPos, maxPos);
        vegapaint::fader (g, area, sliderPos, s.isMouseOverOrDragging(), 28.0f, 16.0f);
    }

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        auto layout = LookAndFeel_V4::getSliderLayout (s);
        if (s.getTextBoxPosition() == juce::Slider::TextBoxBelow)
        {
            auto b = s.getLocalBounds();
            layout.textBoxBounds = b.removeFromBottom (16);
            layout.sliderBounds = b;
        }
        return layout;
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
            return VegaFonts::ui (11.0f);
        return l.getFont();
    }

    void drawLabel (juce::Graphics& g, juce::Label& l) override
    {
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr && ! l.isBeingEdited())
        {
            g.setColour (VegaColours::textDim);
            g.setFont (VegaFonts::ui (11.0f));
            g.drawFittedText (l.getText(), l.getLocalBounds(), juce::Justification::centred, 1);
            return;
        }
        LookAndFeel_V4::drawLabel (g, l);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::textColourId, VegaColours::textDim);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    // ------------------------------------------------------------------ toggles
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace VegaColours;
        const bool on = b.getToggleState();
        auto area = b.getLocalBounds().toFloat();

        if (b.getProperties()["switch"])
        {
            // small vertical toggle switch with two labels (e.g. Stereo / M‑S)
            const auto labels = juce::StringArray::fromTokens (b.getButtonText(), "|", "");
            auto sw = juce::Rectangle<float> (10.0f, 22.0f).withCentre ({ area.getX() + 12.0f, area.getCentreY() });
            g.setColour (juce::Colour (0xff111111));
            g.fillRoundedRectangle (sw, 5.0f);
            g.setColour (line);
            g.drawRoundedRectangle (sw, 5.0f, 1.0f);
            auto lever = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ sw.getCentreX(), on ? sw.getBottom() - 5.5f : sw.getY() + 5.5f });
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e8e8), lever.getX(), lever.getY(),
                                                     juce::Colour (0xff7a7a7a), lever.getRight(), lever.getBottom(), false));
            g.fillEllipse (lever);
            g.setFont (VegaFonts::ui (10.0f));
            if (labels.size() >= 2)
            {
                g.setColour (on ? textDim : text);
                g.drawText (labels[0], (int) sw.getRight() + 5, (int) sw.getY() - 5, 70, 12, juce::Justification::left);
                g.setColour (on ? text : textDim);
                g.drawText (labels[1], (int) sw.getRight() + 5, (int) sw.getBottom() - 7, 70, 12, juce::Justification::left);
            }
            return;
        }

        if (b.getButtonText().isNotEmpty())
        {
            // labelled check box (e.g. option toggles inside pages)
            auto box = juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ area.getX() + 10.0f, area.getCentreY() });
            g.setColour (juce::Colour (0xff161616)); g.fillRoundedRectangle (box, 2.0f);
            g.setColour (on ? accent : juce::Colour (0xff5c5c5c)); g.drawRoundedRectangle (box, 2.0f, 1.0f);
            if (on) { g.setColour (accent); g.fillRoundedRectangle (box.reduced (4.0f), 1.0f); }
            g.setColour (over ? juce::Colours::white : text);
            g.setFont (VegaFonts::ui (12.0f));
            g.drawText (b.getButtonText(), area.withTrimmedLeft (26.0f).toNearestInt(), juce::Justification::centredLeft);
            return;
        }

        // illuminated square push-button (themed)
        const float sz = std::min (area.getWidth(), area.getHeight() - 16.0f);
        auto sq = juce::Rectangle<float> (sz, sz).withCentre ({ area.getCentreX(), area.getY() + sz * 0.5f + 1.0f });
        vegacontrols::paintButton (g, sq, on, over);
        g.setColour (textDim);
        g.setFont (VegaFonts::ui (11.0f));
        g.drawText (on ? "On" : "Off", area.withTop (sq.getBottom() + 3.0f).toNearestInt(), juce::Justification::centredTop);
    }

    // ------------------------------------------------------------------ text buttons (top bar)
    /** a button's lit colour: its own buttonOnColourId if the component set one, else the theme's */
    static juce::Colour litColourFor (juce::Button& b)
    {
        return b.isColourSpecified (juce::TextButton::buttonOnColourId) ? b.findColour (juce::TextButton::buttonOnColourId) : juce::Colour();
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        vegacontrols::paintButton (g, b.getLocalBounds().toFloat(), b.getToggleState(), over && b.isEnabled(), down, litColourFor (b));
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        auto c = vegacontrols::buttonText (b.getToggleState(), litColourFor (b));
        if (! b.isEnabled()) c = c.withMultipliedAlpha (0.4f);
        g.setColour (c);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1).translated (0, down ? 1 : 0), juce::Justification::centred, 1);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return VegaFonts::ui (10.5f, true); }

    // ------------------------------------------------------------------ combo boxes
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
        // chevron
        juce::Path p;
        const float cx = (float) w - 10.0f, cy = (float) h * 0.5f;
        p.startNewSubPath (cx - 4, cy + 2); p.lineTo (cx, cy - 2); p.lineTo (cx + 4, cy + 2);
        g.setColour (box.isMouseOver() ? VegaColours::text : VegaColours::textDim);
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return VegaFonts::ui (10.5f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centred);
    }
    juce::Font getPopupMenuFont() override { return VegaFonts::ui (13.0f); }

    // ------------------------------------------------------------------ menus (popup + menu bar), in the plug-ins' look
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (0xff1b1b1b));
        g.setColour (VegaColours::line); g.drawRect (0, 0, w, h, 1);
        g.setColour (VegaColours::accent.withAlpha (0.6f)); g.fillRect (1, 1, w - 2, 1);      // thin cyan top edge, like the plug-in headers
    }
    int getPopupMenuBorderSize() override { return 4; }
    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColourToUse) override
    {
        if (isSeparator)
        {
            g.setColour (VegaColours::line);
            g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }
        auto r = area.reduced (3, 0);
        if (isHighlighted && isActive)
        {
            g.setColour (VegaColours::accent.withAlpha (0.14f)); g.fillRoundedRectangle (r.toFloat(), 2.0f);
            g.setColour (VegaColours::accent); g.fillRect (r.getX(), r.getY() + 3, 2, r.getHeight() - 6);
        }
        const auto ink = ! isActive ? VegaColours::textDim.withAlpha (0.5f)
                       : textColourToUse != nullptr ? *textColourToUse
                       : isHighlighted ? juce::Colours::white : VegaColours::text;
        auto iconArea = r.removeFromLeft (22).toFloat();
        if (isTicked)
        {   // a small lit square, like the plug-ins' buttons
            auto sq = juce::Rectangle<float> (9.0f, 9.0f).withCentre (iconArea.getCentre());
            g.setColour (VegaColours::accent.withAlpha (0.3f)); g.fillRect (sq.expanded (2.0f));
            g.setColour (VegaColours::accent); g.fillRect (sq);
        }
        else if (icon != nullptr)
            icon->drawWithin (g, iconArea.reduced (4.0f), juce::RectanglePlacement::centred, 1.0f);
        g.setColour (ink); g.setFont (getPopupMenuFont());
        if (hasSubMenu)
        {
            auto ar = r.removeFromRight (14).toFloat();
            juce::Path p; const auto c = ar.getCentre();
            p.startNewSubPath (c.x - 2.0f, c.y - 4.0f); p.lineTo (c.x + 2.5f, c.y); p.lineTo (c.x - 2.0f, c.y + 4.0f);
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }
        r.removeFromRight (6);
        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::mono (11.0f));
            g.drawText (shortcutKeyText, r, juce::Justification::centredRight);
            g.setColour (ink); g.setFont (getPopupMenuFont());
        }
        g.drawFittedText (text, r, juce::Justification::centredLeft, 1);
    }
    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name) override
    {
        g.setFont (VegaFonts::ui (10.0f, true)); g.setColour (VegaColours::accent.withAlpha (0.8f));
        g.drawText (name.toUpperCase(), area.reduced (10, 0), juce::Justification::bottomLeft);
    }
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override
    {
        if (isSeparator) { idealWidth = 50; idealHeight = 9; return; }
        juce::LookAndFeel_V4::getIdealPopupMenuItemSize (text, false, standardMenuItemHeight, idealWidth, idealHeight);
        idealHeight = std::max (idealHeight, 24);
        idealWidth += 18;
    }
    juce::Font getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) override { return VegaFonts::ui (13.5f); }
    int getMenuBarItemWidth (juce::MenuBarComponent& mb, int index, const juce::String& t) override
    {
        return juce::GlyphArrangement::getStringWidthInt (getMenuBarFont (mb, index, t), t) + 22;
    }
    void drawMenuBarBackground (juce::Graphics&, int, int, bool, juce::MenuBarComponent&) override {}
    void drawMenuBarItem (juce::Graphics& g, int w, int h, int, const juce::String& text, bool isMouseOver, bool isMenuOpen,
                          bool, juce::MenuBarComponent& mb) override
    {
        if (isMenuOpen || isMouseOver)
        {
            g.setColour (VegaColours::accent.withAlpha (isMenuOpen ? 0.16f : 0.08f)); g.fillRoundedRectangle (1.0f, 1.0f, w - 2.0f, h - 2.0f, 2.0f);
            if (isMenuOpen) { g.setColour (VegaColours::accent); g.fillRect (4, h - 2, w - 8, 2); }
        }
        g.setColour (isMenuOpen ? VegaColours::accent : VegaColours::text);
        g.setFont (getMenuBarFont (mb, 0, text));
        g.drawText (text, 0, 0, w, h, juce::Justification::centred);
    }
};


namespace vegaui
{
using APVTS = juce::AudioProcessorValueTreeState;

inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// ---------------------------------------------------------------- knob
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& title, bool big = false,
          std::function<juce::String (double)> formatter = {}, const juce::String& stepLabels = {})
    {
        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (VegaFonts::ui (big ? 12.5f : 11.5f));
        label.setColour (juce::Label::textColourId, VegaColours::text);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setTextBoxStyle (stepLabels.isNotEmpty() ? juce::Slider::NoTextBox : juce::Slider::TextBoxBelow, false, 90, 16);
        slider.setMouseDragSensitivity (big ? 320 : 220);
        slider.getProperties().set ("big", big);
        if (stepLabels.isNotEmpty()) slider.getProperties().set ("stepLabels", stepLabels);
        addAndMakeVisible (slider);

        if (auto* p = state.getParameter (paramId))
        {
            attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
            slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
        }
        else
            jassertfalse; // unknown parameter id

        if (formatter)
        {
            slider.textFromValueFunction = [formatter] (double v) { return formatter (v); };
            slider.updateText();
        }
    }
    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (16));
        slider.setBounds (b);
    }
    juce::Slider slider;
    juce::Label label;
private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------------------------------------------------------- toggle / switch
class Toggle : public juce::Component
{
public:
    /** switchLabels: "Off|On" style text turns it into a small lever switch */
    Toggle (APVTS& state, const juce::String& paramId, const juce::String& title, const juce::String& switchLabels = {})
    {
        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (VegaFonts::ui (11.5f));
        label.setColour (juce::Label::textColourId, VegaColours::text);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
        isSwitch = switchLabels.isNotEmpty();
        if (isSwitch) { button.getProperties().set ("switch", true); button.setButtonText (switchLabels); }
        addAndMakeVisible (button);
        if (state.getParameter (paramId) != nullptr)
            attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
        else
            jassertfalse;
    }
    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (16));
        if (isSwitch) button.setBounds (b.withSizeKeepingCentre (std::min (b.getWidth(), 84), 34).translated (8, 0));
        else          button.setBounds (b.withSizeKeepingCentre (34, 52).withY (b.getY() + 6));
    }
    juce::ToggleButton button;
    juce::Label label;
private:
    bool isSwitch = false;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// ---------------------------------------------------------------- choice selector (combo)
class Choice : public juce::Component
{
public:
    Choice (APVTS& state, const juce::String& paramId, const juce::String& title)
    {
        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (VegaFonts::ui (11.5f));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
            box.addItemList (p->choices, 1);
        addAndMakeVisible (box);
        if (state.getParameter (paramId) != nullptr)
            attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, box);
    }
    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (16));
        box.setBounds (b.withSizeKeepingCentre (b.getWidth() - 6, 22).withY (b.getY() + 6));
    }
    juce::ComboBox box;
    juce::Label label;
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// ---------------------------------------------------------------- vertical LED level meter
class LedMeter : public juce::Component
{
public:
    void setLevel (float linearPeak)
    {
        const float db = gainToDb (linearPeak);
        level = db > level ? db : level - 1.2f;
        if (db > hold) { hold = db; holdTicks = 45; } else if (--holdTicks <= 0) hold -= 0.8f;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const int n = (int) std::size (segDb);
        const float segH = b.getHeight() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            auto seg = juce::Rectangle<float> (b.getX(), b.getY() + i * segH, b.getWidth(), segH).reduced (0, segH * 0.18f);
            const float th = segDb[i];
            const juce::Colour c = th >= -0.1f ? juce::Colour (0xffff5a5a) : (th > -18.0f ? juce::Colours::white : VegaColours::accent);
            const bool lit = level >= th;
            g.setColour (lit ? c : juce::Colour (0xff3a3a3a));
            g.fillRect (seg);
            if (! lit && i < n - 1 && hold <= th && hold > segDb[i + 1]) { g.setColour (c.withAlpha (0.7f)); g.fillRect (seg); }
        }
    }
    static constexpr float segDb[] { 0, -1, -2, -3, -4, -5, -6, -8, -10, -12, -15, -18, -21, -24, -30, -36, -42, -48, -54, -60 };
private:
    float level = -100, hold = -100; int holdTicks = 0;
};

class LedScale : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        const int n = (int) std::size (LedMeter::segDb);
        const float segH = (float) getHeight() / (float) n;
        g.setFont (VegaFonts::ui (9.5f));
        g.setColour (VegaColours::textDim);
        for (int i = 0; i < n; ++i)
        {
            const int db = (int) LedMeter::segDb[i];
            if (db == 0 || db == -3 || db == -6 || db == -12 || db == -18 || db == -24 || db == -36 || db == -48 || db == -60)
                g.drawText (juce::String (std::abs (db)), 0, (int) (i * segH + segH * 0.5f) - 6, getWidth(), 12, juce::Justification::centred);
        }
    }
};

/** Stereo output meter block: scale + L + R with captions */
class StereoMeter : public juce::Component
{
public:
    StereoMeter() { addAndMakeVisible (scale); addAndMakeVisible (l); addAndMakeVisible (r); }
    void setLevels (float left, float right) { l.setLevel (left); r.setLevel (right); }
    void resized() override
    {
        auto b = getLocalBounds().withTrimmedTop (16).withTrimmedBottom (16);
        scale.setBounds (b.removeFromLeft (22));
        b.removeFromLeft (4);  l.setBounds (b.removeFromLeft (14));
        b.removeFromLeft (6);  r.setBounds (b.removeFromLeft (14));
    }
    void paint (juce::Graphics& g) override
    {
        g.setFont (VegaFonts::ui (9.5f));
        g.setColour (VegaColours::textDim);
        g.drawText ("dB", l.getX(), 0, 34, 12, juce::Justification::centred);
        g.drawText (capA, l.getX() - 6, getHeight() - 13, 26, 12, juce::Justification::centred);
        g.drawText (capB, r.getX() - 6, getHeight() - 13, 26, 12, juce::Justification::centred);
    }
    juce::String capA { "L" }, capB { "R" };
private:
    LedScale scale; LedMeter l, r;
};

// ---------------------------------------------------------------- gain‑reduction LED column (0..12 dB)
class GrColumn : public juce::Component
{
public:
    void setGr (float db) { value = db > value ? db : std::max (0.0f, value - 0.25f); repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setFont (VegaFonts::ui (10.5f));
        g.setColour (VegaColours::text);
        g.drawText (title, b.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredLeft);
        auto meter = b.removeFromLeft (14.0f);
        constexpr int n = 16;
        const float segH = meter.getHeight() / n;
        for (int i = 0; i < n; ++i)
        {
            const float th = (i + 0.5f) * maxDb / n;
            auto seg = juce::Rectangle<float> (meter.getX(), meter.getY() + i * segH, meter.getWidth(), segH).reduced (0, segH * 0.18f);
            g.setColour (value >= th ? VegaColours::accent : juce::Colour (0xff3a3a3a));
            g.fillRect (seg);
        }
        g.setFont (VegaFonts::ui (9.0f));
        g.setColour (VegaColours::textDim);
        for (int i = 0; i <= 4; ++i)
        {
            const float db = maxDb * i / 4.0f;
            g.drawText (juce::String ((int) db), (int) b.getX() + 3,
                        juce::jlimit ((int) meter.getY(), (int) meter.getBottom() - 12, (int) (meter.getY() + db / maxDb * meter.getHeight()) - 6),
                        22, 12, juce::Justification::left);
        }
    }
    float maxDb = 12.0f;
    juce::String title { "GR" };
private:
    float value = 0;
};

// ---------------------------------------------------------------- OLED read‑out with up to 3 lines
class Oled : public juce::Component
{
public:
    void setLines (const juce::String& title, const juce::String& big,
                   const juce::String& l1 = {}, const juce::String& v1 = {},
                   const juce::String& l2 = {}, const juce::String& v2 = {})
    {
        t = title; b = big; a1 = l1; b1 = v1; a2 = l2; b2 = v2; repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff101010)); g.fillRoundedRectangle (r, 3.0f);
        auto s = r.reduced (4.0f);
        g.setColour (VegaColours::oled); g.fillRect (s);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        const bool hasBottom = a1.isNotEmpty() || a2.isNotEmpty();
        auto top = s.removeFromTop (hasBottom ? s.getHeight() * 0.5f : s.getHeight());
        g.setFont (VegaFonts::mono (11.0f));
        g.drawText (t, top.removeFromTop (top.getHeight() * 0.5f), juce::Justification::centredBottom);
        g.setFont (VegaFonts::mono (12.5f));
        g.drawText (b, top, juce::Justification::centred);
        if (hasBottom)
        {
            g.setFont (VegaFonts::mono (10.5f));
            auto left = s.removeFromLeft (s.getWidth() * 0.5f);
            g.drawText (a1, left.removeFromTop (left.getHeight() * 0.5f), juce::Justification::centredBottom);
            g.drawText (b1, left, juce::Justification::centredTop);
            g.drawText (a2, s.removeFromTop (s.getHeight() * 0.5f), juce::Justification::centredBottom);
            g.drawText (b2, s, juce::Justification::centredTop);
        }
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        for (float y = r.getY() + 4; y < r.getBottom() - 4; y += 2.0f) g.drawHorizontalLine ((int) y, r.getX() + 4, r.getRight() - 4);
    }
private:
    juce::String t, b, a1, b1, a2, b2;
};

// ---------------------------------------------------------------- dark screen panel (for scopes/analysers)
inline void drawScreen (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff141414));
    g.fillRoundedRectangle (r, 3.0f);
}

// ---------------------------------------------------------------- chrome: top bar, body, sections, bottom bar
/** Paints the standard Vega Productions top bar (logo + product line). Returns the bar height. */
inline int drawTopBar (juce::Graphics& g, int width, const juce::String& productLine)
{
    using namespace VegaColours;
    constexpr int h = 34;
    g.setColour (bar); g.fillRect (0, 0, width, h);
    g.setColour (line); g.drawHorizontalLine (h, 0, (float) width);
    const juce::Point<float> c (22.0f, 17.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawEllipse (c.x - 9, c.y - 9, 18, 18, 1.4f);
    juce::Path v;
    v.startNewSubPath (c.x - 4.5f, c.y - 4); v.lineTo (c.x, c.y + 4.5f); v.lineTo (c.x + 4.5f, c.y - 4);
    g.strokePath (v, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (VegaFonts::ui (14.5f, true));
    g.drawText ("Vega Productions", 38, 0, 160, h, juce::Justification::centredLeft);
    g.setFont (VegaFonts::ui (10.0f));
    g.setColour (textDim);
    g.drawText (productLine, 182, 0, 260, h, juce::Justification::centredLeft);
    return h;
}

/** Brushed charcoal body for the control area. */
inline void drawBody (juce::Graphics& g, juce::Rectangle<int> r)
{
    using namespace VegaColours;
    g.setGradientFill (juce::ColourGradient (panelLight, 0, (float) r.getY(), panel, 0, (float) r.getBottom(), false));
    g.fillRect (r);
}

/** Section divider on the left edge + small caps title. */
inline void drawSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, bool leftDivider = true)
{
    using namespace VegaColours;
    if (leftDivider)
    {
        g.setColour (line);
        g.drawVerticalLine (r.getX(), (float) r.getY() + 4.0f, (float) r.getBottom() - 4.0f);
    }
    g.setFont (VegaFonts::ui (9.5f, true));
    g.setColour (textDim.withAlpha (0.7f));
    g.drawText (title.toUpperCase(), r.getX(), r.getY(), r.getWidth(), 14, juce::Justification::centred);
}

inline void drawBottomBar (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& caption = {})
{
    using namespace VegaColours;
    g.setColour (bar); g.fillRect (r);
    g.setColour (line); g.drawHorizontalLine (r.getY(), (float) r.getX(), (float) r.getRight());
    if (caption.isNotEmpty())
    {
        g.setFont (VegaFonts::ui (10.0f, true));
        g.setColour (textDim.withAlpha (0.45f));
        g.drawText (caption, r, juce::Justification::centred);
    }
}

/** Resets every parameter to its default (for a RESET / INIT button). */
inline void resetAllParameters (juce::AudioProcessor& p)
{
    for (auto* prm : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}
} // namespace vegaui
