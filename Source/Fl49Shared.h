#pragma once
#include "DawUI.h"
#include "HuiSurface.h"
#include "Fl49Hub.h"

/** What the FLkey's knobs and pads mean right now - shared by the bottom hardware strip and the side dock,
    so both always show exactly what the keyboard does. */
namespace fl49ui
{
/** The parameter encoder `knob` (0-7) controls in the current encoder mode, or nullptr. */
inline te::AutomatableParameter* knobParam (int knob, Fl49Hub& fl, HuiSurface& hui, DawSession& session)
{
    if (hui.encMode == HuiSurface::EncMode::plugin) return fl.macroParam (knob);
    auto* t = hui.trackForStrip (knob);
    if (t == nullptr) return nullptr;
    if (hui.encMode == HuiSurface::EncMode::pan) { auto vp = t->getVolumePlugin(); return vp != nullptr ? vp->panParam.get() : nullptr; }
    auto buses = session.getBuses();
    const int si = (int) hui.encMode - (int) HuiSurface::EncMode::sendA;
    if (si < 0 || si >= buses.size() || buses[si] == t) return nullptr;
    auto* snd = session.findSend (*t, *buses[si]);
    return snd != nullptr ? snd->gain.get() : nullptr;
}

inline juce::String knobName (int knob, Fl49Hub& fl, HuiSurface& hui, DawSession& session)
{
    if (hui.encMode == HuiSurface::EncMode::plugin)
    {
        auto* p = fl.macroParam (knob);
        return p != nullptr ? p->getParameterName() : juce::String ("-");
    }
    juce::ignoreUnused (session);
    auto* t = hui.trackForStrip (knob);
    return t != nullptr ? t->getName() : juce::String ("-");
}

inline juce::String knobValue (te::AutomatableParameter* p, HuiSurface& hui)
{
    if (p == nullptr) return "--";
    if (hui.encMode == HuiSurface::EncMode::plugin) return p->getCurrentValueAsString().substring (0, 8);
    if (hui.encMode == HuiSurface::EncMode::pan)
    {
        const int v = juce::roundToInt ((p->getCurrentNormalisedValue() - 0.5f) * 200.0f);
        return v == 0 ? juce::String ("C") : (v < 0 ? "L" + juce::String (-v) : "R" + juce::String (v));
    }
    return juce::String (juce::roundToInt (p->getCurrentNormalisedValue() * 100.0f)) + "%";
}

inline juce::String knobModeName (HuiSurface& hui)
{
    if (hui.encMode == HuiSurface::EncMode::plugin) return "PLUGIN";
    if (hui.encMode == HuiSurface::EncMode::pan) return "PAN";
    return "SEND " + juce::String::charToString ((juce::juce_wchar) ('A' + (int) hui.encMode - 1));
}

/** Rainbow the FLkey uses for its 8 knobs / tracks (left to right) */
inline juce::Colour knobColour (int i)
{
    static const juce::uint32 c[] { 0xffff5a4e, 0xffff9a3c, 0xff3ccfff, 0xff35d6e0, 0xff7ee87a, 0xffd66bff, 0xffffd23c, 0xff3ee08a };
    return juce::Colour (c[i & 7]);
}

struct PadLook { juce::Colour colour { 0xff2a2a2a }; juce::String label; bool lit = false, hasContent = false; };

/** How pad `i` (0-7 top row, 8-15 bottom row) looks in the current pad mode */
inline PadLook padLook (int i, Fl49Hub& fl, HuiSurface& hui, DawSession& session, bool blink)
{
    PadLook l;
    const int row = i / 8, col = i % 8;
    auto tracks = session.getTracks (true);
    auto* t = hui.bankStart + col < tracks.size() ? tracks[hui.bankStart + col] : nullptr;
    switch (fl.padMode())
    {
        case Fl49Hub::PadMode::drums:
        {
            static const char* gm[] { "KICK", "RIM", "SNARE", "CLAP", "SNR 2", "TOM L", "HAT", "TOM", "PEDAL", "TOM M", "OPEN", "TOM", "TOM H", "CRASH", "TOM", "RIDE" };
            const auto map = fl.pads();
            const int n = map.note[(size_t) i];
            l.colour = knobColour (col);
            l.hasContent = true;
            l.label = n >= 36 && n <= 51 && map.channel[(size_t) i] == 10 ? juce::String (gm[n - 36]) : juce::String (n);
            break;
        }
        case Fl49Hub::PadMode::clips:
            if (t != nullptr)
                if (auto* clip = DawSession::slotClip (*t, fl.sceneOffset + row))
                {
                    l.colour = DawSession::trackColour (*t); l.hasContent = true;
                    l.label = clip->getName();
                    if (DawSession::isSlotPlaying (*clip)) l.lit = true;
                    else if (DawSession::isSlotQueued (*clip)) l.lit = blink;
                }
            break;
        case Fl49Hub::PadMode::scenes:
            if (row == 0) { if (fl.sceneOffset + col < session.numScenes()) { l.colour = VegaColours::gold; l.hasContent = true; l.label = "SCENE " + juce::String (fl.sceneOffset + col + 1); } }
            else if (t != nullptr) { l.colour = juce::Colour (0xffd84a4a); l.hasContent = true; l.label = "STOP"; }
            break;
    }
    return l;
}

/** One pad drawn like a backlit rubber FLkey pad */
inline void drawPad (juce::Graphics& g, juce::Rectangle<float> r, const PadLook& l, bool hit, bool teach, bool blink)
{
    auto c = l.hasContent ? l.colour : juce::Colour (0xff2b2c30);
    const float bright = l.lit || hit ? 1.0f : (l.hasContent ? 0.72f : 1.0f);
    auto fill = c.withMultipliedBrightness (bright);
    if (teach) fill = blink ? VegaColours::gold : juce::Colour (0xff3a3220);
    if (l.lit || hit) { g.setColour (fill.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (3.0f), 7.0f); }
    g.setGradientFill (juce::ColourGradient (fill.brighter (0.25f), r.getCentreX(), r.getY(), fill.darker (0.35f), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (l.lit || hit ? 0.55f : 0.10f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

/** Slider-cap style fader, like the FLkey's */
inline void drawFader (juce::Graphics& g, juce::Rectangle<float> slot, float pos, juce::Colour tint, bool selected)
{
    g.setColour (juce::Colour (0xff0c0c0d)); g.fillRoundedRectangle (slot.withSizeKeepingCentre (5.0f, slot.getHeight()), 2.5f);
    // ticks
    g.setColour (juce::Colour (0xff3a3b40));
    for (int i = 0; i <= 10; ++i) { const float y = slot.getY() + slot.getHeight() * i / 10.0f; g.drawHorizontalLine ((int) y, slot.getX() + 2, slot.getX() + 7); }
    const float y = slot.getBottom() - slot.getHeight() * pos;
    auto cap = juce::Rectangle<float> (slot.getCentreX() - 14, y - 8, 28, 16);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (cap.translated (0, 2), 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e6e8), cap.getX(), cap.getY(), juce::Colour (0xff8d8e93), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (selected ? tint : juce::Colour (0xff222226)); g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6, 2.0f));
}

/** A green-to-red segmented level meter (dB in, -60 .. +6) */
inline void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float db)
{
    g.setColour (juce::Colour (0xff0c0c0d)); g.fillRoundedRectangle (r, 1.5f);
    const int segs = 24;
    const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    const float sh = r.getHeight() / segs;
    for (int s = 0; s < segs; ++s)
    {
        const float frac = (s + 1) / (float) segs;
        const auto col = frac > 0.9f ? juce::Colour (0xffff4a3a) : frac > 0.75f ? juce::Colour (0xffffc23a) : frac > 0.45f ? juce::Colour (0xff9be35a) : juce::Colour (0xff2ed4c4);
        auto seg = juce::Rectangle<float> (r.getX() + 1, r.getBottom() - (s + 1) * sh + 1, r.getWidth() - 2, sh - 1.2f);
        g.setColour (frac <= norm ? col : col.withAlpha (0.08f)); g.fillRect (seg);
    }
}
} // namespace fl49ui
