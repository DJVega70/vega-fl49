#pragma once
#include "DawUI.h"
#include "Controllers.h"
#include "Fl49Hub.h"

/**
    VEGA FL49: the FLkey 2 49 on screen, docked on the right (F shows / hides it).
    Everything here mirrors the hardware live and can also be played with the mouse:
      KNOBS  - the 8 encoders: PAN, SENDS or PLUGIN (the selected track's plug-in macros)
      PADS   - DRUMS (play the instrument), CLIPS (2 x 8 clip launcher), SCENES (launch scenes / stop tracks)
      KEYS   - the song key, Snap / Filter to scale, and chord modes (triad, 7th, 9th, soul)
      FADERS - the 8 tracks the faders control right now (follows the bank buttons)
*/
class Fl49Panel : public juce::Component, private juce::Timer
{
public:
    Fl49Panel (Fl49Hub& f, HuiSurface& h, ControllerHub& c, ViewState& v)
        : fl (f), hui (h), hub (c), vs (v), content (*this)
    {
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (viewport);
        startTimerHz (15);
    }
    ~Fl49Panel() override { stopTimer(); }

    static constexpr int preferredWidth = 430;
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff191919));
        g.setColour (VegaColours::line); g.drawVerticalLine (0, 0, (float) getHeight());
    }
    void resized() override
    {
        viewport.setBounds (getLocalBounds().withTrimmedLeft (1));
        content.setSize (viewport.getWidth() - (content.getHeight() > viewport.getHeight() ? 8 : 0), Content::totalHeight);
    }
    void refresh() { content.refresh(); }

private:
    void timerCallback() override { content.refresh(); }

    //==========================================================================
    struct Content : public juce::Component
    {
        static constexpr int totalHeight = 790;
        explicit Content (Fl49Panel& p) : owner (p)
        {
            auto seg = [this] (juce::TextButton& b, const juce::String& text, std::function<void()> click)
            {
                b.setButtonText (text); b.onClick = std::move (click);
                b.setColour (juce::TextButton::buttonOnColourId, VegaColours::accent);
                addAndMakeVisible (b);
            };
            auto& fl = owner.fl; auto& hui = owner.hui;
            seg (knobPan, "PAN", [&hui] { hui.encMode = HuiSurface::EncMode::pan; hui.updateFeedback (true); });
            seg (knobSends, "SENDS", [&hui]
            {   // repeated clicks step through sends A-E
                const bool inSends = (int) hui.encMode >= (int) HuiSurface::EncMode::sendA && (int) hui.encMode <= (int) HuiSurface::EncMode::sendE;
                hui.encMode = ! inSends || hui.encMode == HuiSurface::EncMode::sendE ? HuiSurface::EncMode::sendA : (HuiSurface::EncMode) ((int) hui.encMode + 1);
                hui.updateFeedback (true);
            });
            seg (knobPlugin, "PLUGIN", [&hui] { hui.encMode = HuiSurface::EncMode::plugin; hui.updateFeedback (true); });
            seg (prevPlugin, juce::String::fromUTF8 ("\xe2\x97\x80"), [&fl] { fl.macroSlot = std::max (0, fl.macroSlot - 1); fl.macroPage = 0; });
            seg (nextPlugin, juce::String::fromUTF8 ("\xe2\x96\xb6"), [&fl] { fl.macroSlot = std::min (std::max (0, fl.macroCandidates().size() - 1), fl.macroSlot + 1); fl.macroPage = 0; });
            seg (pageBtn, "1-8", [&fl]
            {
                const int pages = std::max (1, (fl.macroParamCount() + 7) / 8);
                fl.macroPage = (fl.macroPage + 1) % pages;
            });
            seg (openPlugin, "OPEN", [&fl] { if (auto* p = fl.macroPlugin()) dawui::openPlugin (p); });

            seg (padDrums, "DRUMS", [&fl] { fl.setPadMode (Fl49Hub::PadMode::drums); });
            seg (padClips, "CLIPS", [&fl] { fl.setPadMode (Fl49Hub::PadMode::clips); });
            seg (padScenes, "SCENES", [&fl] { fl.setPadMode (Fl49Hub::PadMode::scenes); });
            seg (sceneUp, juce::String::fromUTF8 ("\xe2\x96\xb2"), [&fl] { fl.moveScenes (-1); });
            seg (sceneDown, juce::String::fromUTF8 ("\xe2\x96\xbc"), [&fl] { fl.moveScenes (1); });
            seg (teach, "TEACH PADS", [&fl] { if (fl.isTeaching()) fl.cancelTeach(); else fl.startTeach(); });
            seg (resetPads, "DEFAULT", [&fl] { fl.resetPads(); });
            teach.setTooltip ("Put the FLkey's pads in Drum mode, press TEACH PADS, then hit pads 1-16: the top row left to right, then the bottom row");
            resetPads.setTooltip ("Pads = channel 10, notes 36-51 (bottom-left pad = 36), the FLkey 2's Drum mode for other DAWs");

            for (int i = 0; i < 12; ++i) rootBox.addItem (fl49::noteName (i), i + 1);
            for (int i = 0; i < (int) fl49::scales().size(); ++i) scaleBox.addItem (fl49::scales()[(size_t) i].name, i + 1);
            for (auto* b : { &rootBox, &scaleBox }) { addAndMakeVisible (b); b->onChange = [this] { applyKey(); }; }
            const char* keyNames[] { "OFF", "SNAP", "FILTER" };
            for (int i = 0; i < 3; ++i) seg (keyMode[i], keyNames[i], [this, i] { auto s = owner.fl.keySettings(); s.keys = (fl49::KeyMode) i; owner.fl.setKeySettings (s); });
            const char* chordNames[] { "OFF", "TRIAD", "7TH", "9TH", "SOUL" };
            for (int i = 0; i < 5; ++i) seg (chordMode[i], chordNames[i], [this, i] { auto s = owner.fl.keySettings(); s.chord = (fl49::ChordMode) i; owner.fl.setKeySettings (s); });
            keyMode[1].setTooltip ("Snap to scale: every key plays the nearest note in the key (like the FLkey's Scale mode)");
            keyMode[2].setTooltip ("Filter out of scale: keys outside the key don't play");
            chordMode[4].setTooltip ("Soul: root, 5th, 7th, 9th with the 3rd on top - open Soulful House voicings");
            for (auto& k : knobs) addAndMakeVisible (k);
            for (auto& f : faders) addAndMakeVisible (f);
            for (int i = 0; i < 8; ++i) { knobs[(size_t) i].index = i; knobs[(size_t) i].owner = this; faders[(size_t) i].index = i; faders[(size_t) i].owner = this; }
            pads.owner = this; addAndMakeVisible (pads);
            refresh();
        }

        // ---- layout
        void resized() override
        {
            const int pad = 12, w = getWidth() - pad * 2;
            int y = 64;
            auto row = [&] (int h) { auto r = juce::Rectangle<int> (pad, y, w, h); y += h; return r; };
            // knobs
            y += 26;
            {
                auto r = row (26);
                knobPan.setBounds (r.removeFromLeft (64)); r.removeFromLeft (4);
                knobSends.setBounds (r.removeFromLeft (76)); r.removeFromLeft (4);
                knobPlugin.setBounds (r.removeFromLeft (76));
                openPlugin.setBounds (r.removeFromRight (56));
            }
            y += 6;
            {
                auto r = row (24);
                prevPlugin.setBounds (r.removeFromLeft (28));
                pageBtn.setBounds (r.removeFromRight (54)); r.removeFromRight (4);
                nextPlugin.setBounds (r.removeFromRight (28));
                pluginLabelArea = r.reduced (6, 0);
            }
            y += 6;
            {
                auto r = row (190);
                const int kw = r.getWidth() / 4;
                for (int i = 0; i < 8; ++i) knobs[(size_t) i].setBounds (r.getX() + (i % 4) * kw, r.getY() + (i / 4) * 95, kw, 95);
            }
            // pads
            y += 14; padTitleY = y; y += 26;
            {
                auto r = row (26);
                padDrums.setBounds (r.removeFromLeft (70)); r.removeFromLeft (4);
                padClips.setBounds (r.removeFromLeft (70)); r.removeFromLeft (4);
                padScenes.setBounds (r.removeFromLeft (76));
                sceneDown.setBounds (r.removeFromRight (28)); r.removeFromRight (4);
                sceneUp.setBounds (r.removeFromRight (28));
            }
            y += 8;
            pads.setBounds (row (104));
            y += 6;
            {
                auto r = row (24);
                teach.setBounds (r.removeFromLeft (110)); r.removeFromLeft (6);
                resetPads.setBounds (r.removeFromLeft (80));
                padInfoArea = r.reduced (8, 0);
            }
            // keys
            y += 14; keysTitleY = y; y += 26;
            {
                auto r = row (26);
                rootBox.setBounds (r.removeFromLeft (64)); r.removeFromLeft (6);
                scaleBox.setBounds (r.removeFromLeft (170));
            }
            y += 6;
            {
                auto r = row (24);
                keyLabelArea = r.removeFromLeft (54);
                const int bw = r.getWidth() / 3;
                for (int i = 0; i < 3; ++i) keyMode[i].setBounds (r.removeFromLeft (bw).reduced (2, 0));
            }
            y += 6;
            {
                auto r = row (24);
                chordLabelArea = r.removeFromLeft (54);
                const int bw = r.getWidth() / 5;
                for (int i = 0; i < 5; ++i) chordMode[i].setBounds (r.removeFromLeft (bw).reduced (2, 0));
            }
            // faders
            y += 14; fadersTitleY = y; y += 24;
            {
                auto r = row (118);
                const int fw = r.getWidth() / 8;
                for (int i = 0; i < 8; ++i) faders[(size_t) i].setBounds (r.getX() + i * fw, r.getY(), fw, r.getHeight());
            }
            footerY = y + 8;
        }

        void paint (juce::Graphics& g) override
        {
            auto& fl = owner.fl;
            const int pad = 12, w = getWidth() - pad * 2;
            // header
            g.setColour (juce::Colour (0xff202020)); g.fillRect (0, 0, getWidth(), 56);
            g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (19.0f, true));
            g.drawText ("FLkey 2 49", pad, 6, 200, 26, juce::Justification::centredLeft);
            g.setFont (VegaFonts::ui (11.0f, true));
            auto pill = [&] (juce::Rectangle<float> r, const juce::String& t, bool ok)
            {
                g.setColour (ok ? VegaColours::accent.withAlpha (0.16f) : juce::Colour (0x22ffffff)); g.fillRoundedRectangle (r, 4.0f);
                g.setColour (ok ? VegaColours::accent : VegaColours::textDim); g.drawText (t, r, juce::Justification::centred);
            };
            const bool keysOk = fl.isConnected(), dawOk = owner.hub.isConnected();
            pill ({ (float) pad, 33.0f, 118.0f, 17.0f }, keysOk ? "KEYS: SYNCED" : "KEYS: NOT FOUND", keysOk);
            pill ({ (float) pad + 124.0f, 33.0f, 118.0f, 17.0f }, dawOk ? "DAW: ACTIVE" : "DAW: NOT FOUND", dawOk);
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f));
            const auto sel = owner.vs.getSelectedTrack();
            g.drawFittedText ("Playing: " + (sel != nullptr ? sel->getName() : juce::String ("-")), getWidth() - 170, 8, 158, 18, juce::Justification::centredRight, 1);
            g.drawFittedText ("Map: tracks " + juce::String (owner.hui.bankStart + 1) + "-" + juce::String (owner.hui.bankStart + 8), getWidth() - 170, 30, 158, 18, juce::Justification::centredRight, 1);

            auto title = [&] (int y, const juce::String& t, const juce::String& sub)
            {
                g.setColour (VegaColours::line); g.drawHorizontalLine (y - 6, (float) pad, (float) (pad + w));
                g.setColour (VegaColours::accent); g.setFont (VegaFonts::ui (12.0f, true));
                g.drawText (t, pad, y, 120, 20, juce::Justification::centredLeft);
                g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f));
                g.drawText (sub, pad + 90, y, w - 90, 20, juce::Justification::centredRight);
            };
            title (70, "KNOBS", owner.hui.encMode == HuiSurface::EncMode::plugin ? "encoders = plug-in macros"
                              : owner.hui.encMode == HuiSurface::EncMode::pan ? "encoders = pan" : "encoders = send " + juce::String::charToString ((juce::juce_wchar) ('A' + (int) owner.hui.encMode - 1)));
            // plug-in name
            g.setColour (VegaColours::oled); g.fillRoundedRectangle (pluginLabelArea.toFloat().expanded (6, 0), 3.0f);
            g.setColour (owner.hui.encMode == HuiSurface::EncMode::plugin ? VegaColours::accent : VegaColours::textDim); g.setFont (VegaFonts::mono (12.0f));
            auto* plug = fl.macroPlugin();
            g.drawFittedText (plug != nullptr ? plug->getName() : juce::String ("no plug-in on this track"), pluginLabelArea, juce::Justification::centred, 1);

            const juce::String modeNames[] { "pads play the instrument", "2 x 8 clip launcher", "top: scenes, bottom: stop" };
            title (padTitleY, "PADS", modeNames[(int) fl.padMode()]);
            g.setColour (fl.isTeaching() ? VegaColours::gold : VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f, fl.isTeaching()));
            g.drawFittedText (fl.isTeaching() ? "Hit pad " + juce::String (fl.teachStep() + 1) + " of 16"
                                              : (fl.padMode() == Fl49Hub::PadMode::drums ? juce::String ("Hardware: Drum pad mode") : "Scenes " + juce::String (fl.sceneOffset + 1) + "-" + juce::String (fl.sceneOffset + 2)),
                              padInfoArea, juce::Justification::centredRight, 1);

            title (keysTitleY, "KEYS", "scale & chords - recorded as you hear them");
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f, true));
            g.drawText ("NOTES", keyLabelArea, juce::Justification::centredLeft);
            g.drawText ("CHORDS", chordLabelArea, juce::Justification::centredLeft);

            title (fadersTitleY, "FADERS", "the 8 tracks the faders move");
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.5f));
            g.drawFittedText (owner.hub.isConnected() ? "Transport, faders, track select and mute / solo run over the FLkey's DAW port (HUI)."
                                                      : "Plug in the FLkey 2 49: the DAW finds both of its ports by itself.",
                              pad, footerY, w, 30, juce::Justification::topLeft, 2);
        }

        void refresh()
        {
            auto& fl = owner.fl; auto& hui = owner.hui;
            knobPan.setToggleState (hui.encMode == HuiSurface::EncMode::pan, juce::dontSendNotification);
            knobPlugin.setToggleState (hui.encMode == HuiSurface::EncMode::plugin, juce::dontSendNotification);
            const bool sends = (int) hui.encMode >= (int) HuiSurface::EncMode::sendA && (int) hui.encMode <= (int) HuiSurface::EncMode::sendE;
            knobSends.setToggleState (sends, juce::dontSendNotification);
            knobSends.setButtonText (sends ? "SEND " + juce::String::charToString ((juce::juce_wchar) ('A' + (int) hui.encMode - 1)) : juce::String ("SENDS"));
            const int pages = std::max (1, (fl.macroParamCount() + 7) / 8);
            if (fl.macroPage >= pages) fl.macroPage = 0;
            pageBtn.setButtonText (juce::String (fl.macroPage * 8 + 1) + "-" + juce::String (fl.macroPage * 8 + 8));
            padDrums.setToggleState (fl.padMode() == Fl49Hub::PadMode::drums, juce::dontSendNotification);
            padClips.setToggleState (fl.padMode() == Fl49Hub::PadMode::clips, juce::dontSendNotification);
            padScenes.setToggleState (fl.padMode() == Fl49Hub::PadMode::scenes, juce::dontSendNotification);
            sceneUp.setEnabled (fl.padMode() != Fl49Hub::PadMode::drums); sceneDown.setEnabled (fl.padMode() != Fl49Hub::PadMode::drums);
            teach.setToggleState (fl.isTeaching(), juce::dontSendNotification);
            teach.setButtonText (fl.isTeaching() ? "CANCEL" : "TEACH PADS");
            const auto s = fl.keySettings();
            if (! rootBox.isPopupActive()) rootBox.setSelectedId (s.root + 1, juce::dontSendNotification);
            if (! scaleBox.isPopupActive()) scaleBox.setSelectedId (s.scale + 1, juce::dontSendNotification);
            for (int i = 0; i < 3; ++i) keyMode[i].setToggleState ((int) s.keys == i, juce::dontSendNotification);
            for (int i = 0; i < 5; ++i) chordMode[i].setToggleState ((int) s.chord == i, juce::dontSendNotification);
            repaint();
        }
        void applyKey()
        {
            auto s = owner.fl.keySettings();
            s.root = std::max (0, rootBox.getSelectedId() - 1); s.scale = std::max (0, scaleBox.getSelectedId() - 1);
            owner.fl.setKeySettings (s);
        }

        // ---- one encoder on screen
        struct Knob : public juce::Component
        {
            int index = 0; Content* owner = nullptr; float dragStart = 0;
            te::AutomatableParameter* param()
            {
                auto& o = *owner; auto& hui = o.owner.hui;
                if (hui.encMode == HuiSurface::EncMode::plugin) return o.owner.fl.macroParam (index);
                auto* t = hui.trackForStrip (index);
                if (t == nullptr) return nullptr;
                if (hui.encMode == HuiSurface::EncMode::pan) { auto vp = t->getVolumePlugin(); return vp != nullptr ? vp->panParam.get() : nullptr; }
                auto buses = o.owner.vs.session.getBuses();
                const int si = (int) hui.encMode - (int) HuiSurface::EncMode::sendA;
                if (si < 0 || si >= buses.size() || buses[si] == t) return nullptr;
                auto* snd = o.owner.vs.session.findSend (*t, *buses[si]);
                return snd != nullptr ? snd->gain.get() : nullptr;
            }
            void paint (juce::Graphics& g) override
            {
                auto* p = param();
                const float v = p != nullptr ? p->getCurrentNormalisedValue() : 0.0f;
                auto kr = juce::Rectangle<float> (0, 0, 46, 46).withCentre ({ getWidth() * 0.5f, 30.0f });
                const float a0 = -2.4f, a1 = 2.4f, a = a0 + v * (a1 - a0);
                juce::Path arc; arc.addCentredArc (kr.getCentreX(), kr.getCentreY(), 27, 27, 0, a0, a1, true);
                g.setColour (juce::Colour (0xff2c2c2c)); g.strokePath (arc, juce::PathStrokeType (3.0f));
                if (p != nullptr)
                {
                    juce::Path on; on.addCentredArc (kr.getCentreX(), kr.getCentreY(), 27, 27, 0, a0, a, true);
                    const bool recent = juce::Time::getMillisecondCounter() - owner->touched[(size_t) index] < 600;
                    g.setColour (recent ? VegaColours::accent.brighter (0.3f) : VegaColours::accent); g.strokePath (on, juce::PathStrokeType (3.0f));
                }
                vegapaint::knob (g, kr, a, false, p != nullptr ? juce::Colours::white : juce::Colour (0xff555555));
                g.setColour (p != nullptr ? VegaColours::text : VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f, true));
                juce::String name = p != nullptr ? p->getParameterName() : juce::String ("-");
                if (owner->owner.hui.encMode != HuiSurface::EncMode::plugin)
                    if (auto* t = owner->owner.hui.trackForStrip (index)) name = t->getName();
                g.drawFittedText (name, 2, 62, getWidth() - 4, 15, juce::Justification::centred, 1);
                g.setColour (VegaColours::textDim); g.setFont (VegaFonts::mono (10.5f));
                g.drawFittedText (p != nullptr ? p->getCurrentValueAsString() : juce::String(), 2, 77, getWidth() - 4, 14, juce::Justification::centred, 1);
            }
            void mouseDown (const juce::MouseEvent&) override { if (auto* p = param()) { dragStart = p->getCurrentNormalisedValue(); p->parameterChangeGestureBegin(); } }
            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (auto* p = param())
                {
                    p->setNormalisedParameter (juce::jlimit (0.0f, 1.0f, dragStart - e.getDistanceFromDragStartY() * 0.005f), juce::sendNotification);
                    owner->touched[(size_t) index] = juce::Time::getMillisecondCounter();
                }
            }
            void mouseUp (const juce::MouseEvent&) override { if (auto* p = param()) p->parameterChangeGestureEnd(); }
            void mouseDoubleClick (const juce::MouseEvent&) override { if (auto* p = param()) p->setParameter (p->getDefaultValue().value_or (p->getCurrentValue()), juce::sendNotification); }
        };

        // ---- the 16 pads
        struct Pads : public juce::Component
        {
            Content* owner = nullptr;
            juce::Rectangle<float> padRect (int i) const
            {
                const float gap = 6.0f, pw = (getWidth() - gap * 7) / 8.0f, ph = (getHeight() - gap) / 2.0f;
                return { (i % 8) * (pw + gap), (i / 8) * (ph + gap), pw, ph };
            }
            void paint (juce::Graphics& g) override
            {
                auto& fl = owner->owner.fl; auto& session = owner->owner.vs.session;
                const auto now = juce::Time::getMillisecondCounter();
                const bool blink = (now / 250) % 2 == 0;
                auto tracks = session.getTracks (true);
                const int bank = owner->owner.hui.bankStart;
                for (int i = 0; i < 16; ++i)
                {
                    auto r = padRect (i);
                    const int row = i / 8, col = i % 8;
                    juce::Colour c (0xff2a2a2a); juce::String label; bool lit = false;
                    auto* t = bank + col < tracks.size() ? tracks[bank + col] : nullptr;
                    switch (fl.padMode())
                    {
                        case Fl49Hub::PadMode::drums:
                        {
                            static const char* gm[] { "KICK", "RIM", "SNARE", "CLAP", "SNR 2", "TOM L", "HAT", "TOM", "PEDAL", "TOM M", "OPEN", "TOM", "TOM H", "CRASH", "TOM", "RIDE" };
                            const auto map = fl.pads();
                            const int n = map.note[(size_t) i];
                            c = row == 0 ? juce::Colour (0xff7a5cff) : juce::Colour (0xff2f9e8f);
                            label = n >= 36 && n <= 51 && map.channel[(size_t) i] == 10 ? juce::String (gm[n - 36]) : juce::String (n);
                            break;
                        }
                        case Fl49Hub::PadMode::clips:
                            if (t != nullptr)
                                if (auto* clip = DawSession::slotClip (*t, fl.sceneOffset + row))
                                {
                                    c = DawSession::trackColour (*t);
                                    label = clip->getName();
                                    if (DawSession::isSlotPlaying (*clip)) { lit = true; c = juce::Colour (0xff3ddc84); }
                                    else if (DawSession::isSlotQueued (*clip)) { lit = blink; }
                                }
                            break;
                        case Fl49Hub::PadMode::scenes:
                            if (row == 0) { if (fl.sceneOffset + col < session.numScenes()) { c = VegaColours::gold; label = "SCENE " + juce::String (fl.sceneOffset + col + 1); } }
                            else if (t != nullptr) { c = juce::Colour (0xffb04040); label = "STOP"; }
                            break;
                    }
                    const bool hit = now - fl.padHitTime[(size_t) i] < 160;
                    const bool teachThis = fl.isTeaching() && fl.teachStep() == i;
                    auto fill = c.withMultipliedBrightness (lit || hit ? 1.25f : 0.62f);
                    if (teachThis) fill = blink ? VegaColours::gold : juce::Colour (0xff3a3220);
                    g.setColour (fill.withAlpha (0.25f)); if (lit || hit) g.fillRoundedRectangle (r.expanded (2.5f), 5.0f);
                    g.setColour (fill); g.fillRoundedRectangle (r, 4.0f);
                    g.setColour (juce::Colours::white.withAlpha (hit ? 0.5f : 0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
                    g.setColour (fill.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff141414) : VegaColours::text);
                    g.setFont (VegaFonts::ui (9.5f, true));
                    g.drawFittedText (teachThis ? juce::String (i + 1) : label, r.reduced (3).toNearestInt(), juce::Justification::centred, 2);
                }
            }
            void mouseDown (const juce::MouseEvent& e) override
            {
                for (int i = 0; i < 16; ++i)
                    if (padRect (i).contains (e.position)) { owner->owner.fl.pressPad (i, 100); repaint(); return; }
            }
        };

        // ---- one of the 8 faders
        struct Fader : public juce::Component
        {
            int index = 0; Content* owner = nullptr; float startPos = 0;
            te::AudioTrack* track() { return owner->owner.hui.trackForStrip (index); }
            juce::Rectangle<float> slot() const { return getLocalBounds().toFloat().reduced (getWidth() * 0.5f - 4.0f, 4.0f).withTrimmedBottom (18); }
            void paint (juce::Graphics& g) override
            {
                auto* t = track();
                auto s = slot();
                g.setColour (juce::Colour (0xff0e0e0e)); g.fillRoundedRectangle (s, 3.0f);
                if (t == nullptr) return;
                float pos = 0;
                if (auto vp = t->getVolumePlugin()) pos = HuiSurface::faderPosFromDb (vp->getVolumeDb());
                auto level = s.withTop (s.getBottom() - s.getHeight() * pos);
                const auto col = DawSession::trackColour (*t);
                g.setColour (col.withAlpha (0.7f)); g.fillRoundedRectangle (level, 3.0f);
                g.setColour (juce::Colour (0xffdddddd)); g.fillRoundedRectangle (juce::Rectangle<float> (s.getCentreX() - 9, level.getY() - 3, 18, 6), 1.5f);
                const bool sel = t->itemID == owner->owner.vs.selectedTrack;
                g.setColour (sel ? VegaColours::accent : VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, sel));
                g.drawFittedText (t->getName(), 0, getHeight() - 16, getWidth(), 14, juce::Justification::centred, 1);
                if (t->isMuted (false)) { g.setColour (juce::Colour (0xffe8a23a)); g.fillEllipse (2, 2, 6, 6); }
                if (t->isSolo (false)) { g.setColour (juce::Colour (0xff3ddcff)); g.fillEllipse (getWidth() - 8.0f, 2, 6, 6); }
            }
            void mouseDown (const juce::MouseEvent&) override
            {
                if (auto* t = track())
                {
                    owner->owner.vs.selectTrack (t);
                    if (auto vp = t->getVolumePlugin()) startPos = HuiSurface::faderPosFromDb (vp->getVolumeDb());
                }
            }
            void mouseDrag (const juce::MouseEvent& e) override
            {
                if (auto* t = track())
                    if (auto vp = t->getVolumePlugin())
                        vp->setVolumeDb (HuiSurface::dbFromFaderPos (juce::jlimit (0.0f, 1.0f, startPos - e.getDistanceFromDragStartY() / slot().getHeight())));
                repaint();
            }
        };

        Fl49Panel& owner;
        juce::TextButton knobPan, knobSends, knobPlugin, prevPlugin, nextPlugin, pageBtn, openPlugin;
        juce::TextButton padDrums, padClips, padScenes, sceneUp, sceneDown, teach, resetPads;
        juce::TextButton keyMode[3], chordMode[5];
        juce::ComboBox rootBox, scaleBox;
        std::array<Knob, 8> knobs; std::array<Fader, 8> faders; Pads pads;
        std::array<juce::uint32, 8> touched {};
        juce::Rectangle<int> pluginLabelArea, padInfoArea, keyLabelArea, chordLabelArea;
        int padTitleY = 0, keysTitleY = 0, fadersTitleY = 0, footerY = 0;
    };

    Fl49Hub& fl; HuiSurface& hui; ControllerHub& hub; ViewState& vs;
    juce::Viewport viewport;
    Content content;

public:
    void knobTouched (int k) { if (k >= 0 && k < 8) content.touched[(size_t) k] = juce::Time::getMillisecondCounter(); }
};
