#pragma once
#include "Fl49Shared.h"
#include "Controllers.h"

/**
    The FLkey 2 49 along the bottom of the window: wheels, track buttons, 8 faders + master with meters,
    the fader buttons (mute / solo / arm), the 8 knobs and the 16 pads. It mirrors the keyboard live and
    every part also works with the mouse. (H shows / hides it.)
*/
class HardwareStrip : public juce::Component, private juce::Timer
{
public:
    HardwareStrip (Fl49Hub& f, HuiSurface& h, ControllerHub& c, ViewState& v) : fl (f), hui (h), hub (c), vs (v)
    {
        auto small = [this] (juce::TextButton& b, const juce::String& text, std::function<void()> click, const juce::String& tip = {})
        {
            b.setButtonText (text); b.onClick = std::move (click); b.setTooltip (tip);
            b.setColour (juce::TextButton::buttonOnColourId, VegaColours::accent);
            addAndMakeVisible (b);
        };
        small (trackLeft, juce::String::fromUTF8 ("\xe2\x97\x80"), [this] { bank (-1); }, "Previous 8 tracks (the faders, knobs and pads move with it)");
        small (trackRight, juce::String::fromUTF8 ("\xe2\x96\xb6"), [this] { bank (1); }, "Next 8 tracks");
        small (btnMute, "MUTE", [this] { buttonMode = 0; }, "Fader buttons mute their track");
        small (btnSolo, "SOLO", [this] { buttonMode = 1; }, "Fader buttons solo their track");
        small (btnArm, "ARM", [this] { buttonMode = 2; }, "Fader buttons record-arm their track");
        small (knobPan, "PAN", [this] { hui.encMode = HuiSurface::EncMode::pan; hui.updateFeedback (true); });
        small (knobSends, "SENDS", [this]
        {
            const bool inSends = (int) hui.encMode >= (int) HuiSurface::EncMode::sendA && (int) hui.encMode <= (int) HuiSurface::EncMode::sendE;
            hui.encMode = ! inSends || hui.encMode == HuiSurface::EncMode::sendE ? HuiSurface::EncMode::sendA : (HuiSurface::EncMode) ((int) hui.encMode + 1);
            hui.updateFeedback (true);
        }, "Click again for the next send (A-E)");
        small (knobPlugin, "PLUGIN", [this] { hui.encMode = HuiSurface::EncMode::plugin; hui.updateFeedback (true); }, "Knobs = the selected track's plug-in macros");
        small (padDrums, "DRUMS", [this] { fl.setPadMode (Fl49Hub::PadMode::drums); });
        small (padClips, "CLIPS", [this] { fl.setPadMode (Fl49Hub::PadMode::clips); });
        small (padScenes, "SCENES", [this] { fl.setPadMode (Fl49Hub::PadMode::scenes); });
        small (sceneUp, juce::String::fromUTF8 ("\xe2\x96\xb2"), [this] { fl.moveScenes (-1); }, "Pads: previous scene");
        small (sceneDown, juce::String::fromUTF8 ("\xe2\x96\xbc"), [this] { fl.moveScenes (1); }, "Pads: next scene");
        small (stopAll, juce::String::fromUTF8 ("\xe2\x96\xa0"), [this] { vs.session.stopAllSlots(); }, "Stop all clips");
        small (playBtn, juce::String::fromUTF8 ("\xe2\x96\xb6"), [this] { auto& tc = vs.edit().getTransport(); if (tc.isPlaying()) tc.stop (false, false); else tc.play (false); }, "Play / stop");
        startTimerHz (24);
    }
    ~HardwareStrip() override { clearMeters(); }

    static constexpr int preferredHeight = 238;
    /** Call before the Edit is replaced: the meters hold on to the old tracks */
    void clearMeters()
    {
        for (auto& m : meters) { if (m.plugin != nullptr) m.plugin->measurer.removeClient (m.client); m.plugin = nullptr; }
        if (masterCtx != nullptr) masterCtx->masterLevels.removeClient (masterClient);
        masterCtx = nullptr;
    }

    //==========================================================================
    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        // left: track buttons, fader-button mode, wheels
        left = r.removeFromLeft (150);
        {
            auto l = left;
            auto row = l.removeFromTop (24);
            trackLeft.setBounds (row.removeFromLeft (40)); row.removeFromLeft (6); trackRight.setBounds (row.removeFromLeft (40));
            l.removeFromTop (16);   // "Track" label
            auto modes = l.removeFromTop (22);
            const int bw = modes.getWidth() / 3;
            btnMute.setBounds (modes.removeFromLeft (bw).reduced (1, 0)); btnSolo.setBounds (modes.removeFromLeft (bw).reduced (1, 0)); btnArm.setBounds (modes.reduced (1, 0));
            l.removeFromTop (10);
            wheels = l.withTrimmedBottom (16);
        }
        r.removeFromLeft (18);
        // faders
        faderArea = r.removeFromLeft (juce::jmin (470, r.getWidth() / 2 - 40));
        r.removeFromLeft (14);
        // middle column
        auto mid = r.removeFromLeft (44);
        sceneUp.setBounds (mid.removeFromTop (30)); mid.removeFromTop (4);
        sceneDown.setBounds (mid.removeFromTop (30)); mid.removeFromTop (14);
        stopAll.setBounds (mid.removeFromTop (30)); mid.removeFromTop (4);
        playBtn.setBounds (mid.removeFromTop (30));
        r.removeFromLeft (14);
        // right column: knob / pad modes
        auto modeCol = r.removeFromRight (78);
        modeCol.removeFromTop (28);
        for (auto* b : { &knobPan, &knobSends, &knobPlugin }) { b->setBounds (modeCol.removeFromTop (22)); modeCol.removeFromTop (3); }
        modeCol.removeFromTop (12);
        for (auto* b : { &padDrums, &padClips, &padScenes }) { b->setBounds (modeCol.removeFromTop (22)); modeCol.removeFromTop (3); }
        r.removeFromRight (12);
        knobArea = r.removeFromTop (96);
        r.removeFromTop (8);
        padArea = r.withTrimmedBottom (14);
    }

    void paint (juce::Graphics& g) override
    {
        // the keyboard's top panel
        auto body = getLocalBounds().toFloat().reduced (4, 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2d31), 0, body.getY(), juce::Colour (0xff1f2023), 0, body.getBottom(), false));
        g.fillRoundedRectangle (body, 8.0f);
        g.setColour (juce::Colour (0xff3b3c41)); g.drawRoundedRectangle (body.reduced (0.5f), 8.0f, 1.0f);
        const auto now = juce::Time::getMillisecondCounter();
        const bool blink = (now / 250) % 2 == 0;

        g.setFont (VegaFonts::ui (10.0f, true)); g.setColour (VegaColours::textDim);
        g.drawText ("Track", left.getX(), left.getY() + 24, 86, 16, juce::Justification::centred);
        paintWheels (g);
        paintFaders (g);
        paintKnobs (g);
        paintPads (g, now, blink);

        // logo + connection
        auto logo = juce::Rectangle<int> (getWidth() - 96, 10, 82, 20);
        g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (15.0f, true));
        g.drawText ("FLkey", logo, juce::Justification::centredLeft);
        const bool ok = fl.isConnected() && hub.isConnected(), half = fl.isConnected() || hub.isConnected();
        g.setColour (ok ? juce::Colour (0xff3ddc84) : half ? juce::Colour (0xffffc23a) : juce::Colour (0xff6a6a6a));
        g.fillEllipse ((float) logo.getRight() - 10, (float) logo.getCentreY() - 4, 8, 8);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = {};
        const auto p = e.position;
        // wheels
        for (int w = 0; w < 2; ++w)
            if (wheelRect (w).contains (p)) { drag.kind = Drag::wheel; drag.index = w; drag.start = w == 0 ? 0.5f : modValue; return; }
        // faders
        for (int i = 0; i < 9; ++i)
        {
            if (faderSlot (i).expanded (14, 10).contains (p))
            {
                auto* vp = faderVolume (i);
                if (vp == nullptr) return;
                if (i < 8) if (auto* t = hui.trackForStrip (i)) vs.selectTrack (t);
                drag.kind = Drag::fader; drag.index = i; drag.start = HuiSurface::faderPosFromDb (vp->getVolumeDb());
                return;
            }
            if (i < 8 && faderButton (i).contains (p)) { pressFaderButton (i); return; }
            if (i < 8 && numberTag (i).contains (p)) { if (auto* t = hui.trackForStrip (i)) vs.selectTrack (t); return; }
        }
        // knobs
        for (int i = 0; i < 8; ++i)
            if (knobRect (i).contains (p))
            {
                if (auto* prm = fl49ui::knobParam (i, fl, hui, vs.session)) { drag.kind = Drag::knob; drag.index = i; drag.start = prm->getCurrentNormalisedValue(); prm->parameterChangeGestureBegin(); }
                return;
            }
        // pads
        for (int i = 0; i < 16; ++i)
            if (padRect (i).contains (p)) { fl.pressPad (i, 100); repaint(); return; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float dy = (float) e.getDistanceFromDragStartY();
        switch (drag.kind)
        {
            case Drag::wheel:
            {
                const float v = juce::jlimit (0.0f, 1.0f, drag.start - dy / wheelRect (drag.index).getHeight());
                if (drag.index == 0) { pitchValue = v; fl.sendToKeys (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) (v * 16383.0f)))); }
                else { modValue = v; fl.sendToKeys (juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (v * 127.0f))); }
                break;
            }
            case Drag::fader:
                if (auto* vp = faderVolume (drag.index))
                    vp->setVolumeDb (HuiSurface::dbFromFaderPos (juce::jlimit (0.0f, 1.0f, drag.start - dy / faderSlot (drag.index).getHeight())));
                break;
            case Drag::knob:
                if (auto* prm = fl49ui::knobParam (drag.index, fl, hui, vs.session))
                {
                    prm->setNormalisedParameter (juce::jlimit (0.0f, 1.0f, drag.start - dy * 0.005f), juce::sendNotification);
                    touched[(size_t) drag.index] = juce::Time::getMillisecondCounter();
                }
                break;
            case Drag::none: break;
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag.kind == Drag::wheel && drag.index == 0) { pitchValue = 0.5f; fl.sendToKeys (juce::MidiMessage::pitchWheel (1, 8192)); }   // the pitch wheel springs back
        if (drag.kind == Drag::knob) if (auto* prm = fl49ui::knobParam (drag.index, fl, hui, vs.session)) prm->parameterChangeGestureEnd();
        drag = {};
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 9; ++i)
            if (faderSlot (i).expanded (14, 10).contains (e.position)) if (auto* vp = faderVolume (i)) vp->setVolumeDb (0.0f);
        for (int i = 0; i < 8; ++i)
            if (knobRect (i).contains (e.position))
                if (auto* prm = fl49ui::knobParam (i, fl, hui, vs.session)) prm->setParameter (prm->getDefaultValue().value_or (prm->getCurrentValue()), juce::sendNotification);
    }

    void knobTouched (int k) { if (k >= 0 && k < 8) touched[(size_t) k] = juce::Time::getMillisecondCounter(); }

private:
    //==========================================================================
    void timerCallback() override
    {
        // modes on the buttons
        knobPan.setToggleState (hui.encMode == HuiSurface::EncMode::pan, juce::dontSendNotification);
        knobPlugin.setToggleState (hui.encMode == HuiSurface::EncMode::plugin, juce::dontSendNotification);
        const bool sends = (int) hui.encMode >= (int) HuiSurface::EncMode::sendA && (int) hui.encMode <= (int) HuiSurface::EncMode::sendE;
        knobSends.setToggleState (sends, juce::dontSendNotification);
        knobSends.setButtonText (sends ? fl49ui::knobModeName (hui) : juce::String ("SENDS"));
        padDrums.setToggleState (fl.padMode() == Fl49Hub::PadMode::drums, juce::dontSendNotification);
        padClips.setToggleState (fl.padMode() == Fl49Hub::PadMode::clips, juce::dontSendNotification);
        padScenes.setToggleState (fl.padMode() == Fl49Hub::PadMode::scenes, juce::dontSendNotification);
        btnMute.setToggleState (buttonMode == 0, juce::dontSendNotification);
        btnSolo.setToggleState (buttonMode == 1, juce::dontSendNotification);
        btnArm.setToggleState (buttonMode == 2, juce::dontSendNotification);
        playBtn.setToggleState (vs.edit().getTransport().isPlaying(), juce::dontSendNotification);

        // meters follow the 8 tracks the faders show
        for (int i = 0; i < 8; ++i)
        {
            auto& m = meters[(size_t) i];
            auto* t = hui.trackForStrip (i);
            te::LevelMeterPlugin* want = t != nullptr ? t->getLevelMeterPlugin() : nullptr;
            if (want != m.plugin.get())
            {
                if (m.plugin != nullptr) m.plugin->measurer.removeClient (m.client);
                m.plugin = want; m.level = -100.0f;
                if (want != nullptr) want->measurer.addClient (m.client);
            }
            if (m.plugin != nullptr) m.level = ballistics (m.level, std::max (m.client.getAndClearAudioLevel (0).dB, m.client.getAndClearAudioLevel (1).dB));
        }
        if (auto* ctx = vs.edit().getCurrentPlaybackContext())
        {
            if (ctx != masterCtx)
            {
                if (masterCtx != nullptr) masterCtx->masterLevels.removeClient (masterClient);
                ctx->masterLevels.addClient (masterClient); masterCtx = ctx;
            }
            masterLevel = ballistics (masterLevel, std::max (masterClient.getAndClearAudioLevel (0).dB, masterClient.getAndClearAudioLevel (1).dB));
        }
        repaint();
    }
    static float ballistics (float cur, float db) { return db > cur ? cur + (db - cur) * 0.7f : std::max (db, cur - 1.2f); }

    void bank (int dir)
    {
        const int n = vs.session.getTracks (true).size();
        hui.bankStart = juce::jlimit (0, std::max (0, ((n - 1) / 8) * 8), hui.bankStart + dir * 8);
        hui.updateFeedback (true);
    }
    void pressFaderButton (int i)
    {
        auto* t = hui.trackForStrip (i);
        if (t == nullptr) return;
        if (buttonMode == 0) t->setMute (! t->isMuted (false));
        else if (buttonMode == 1) t->setSolo (! t->isSolo (false));
        else dawui::toggleArm (*t);
    }
    te::VolumeAndPanPlugin* faderVolume (int i)
    {
        if (i == 8) return vs.edit().getMasterVolumePlugin().get();
        auto* t = hui.trackForStrip (i);
        return t != nullptr ? t->getVolumePlugin() : nullptr;
    }

    // ---- geometry
    int faderColW() const { return faderArea.getWidth() / 9; }
    juce::Rectangle<float> faderSlot (int i) const
    {
        const int x = faderArea.getX() + i * faderColW() + (i == 8 ? 8 : 0);
        return juce::Rectangle<float> ((float) x + 6, (float) faderArea.getY() + 22, 10, (float) faderArea.getHeight() - 92);
    }
    juce::Rectangle<float> meterRect (int i) const { auto s = faderSlot (i); return { s.getRight() + 14, s.getY(), 6, s.getHeight() }; }
    juce::Rectangle<float> numberTag (int i) const { auto s = faderSlot (i); return { s.getX() - 4, s.getBottom() + 12, (float) faderColW() - 8, 18 }; }
    juce::Rectangle<float> faderButton (int i) const { auto n = numberTag (i); return n.translated (0, 24).withHeight (20); }
    juce::Rectangle<float> wheelRect (int w) const
    {
        const float ww = 30, gap = 22;
        return { (float) wheels.getX() + 10 + w * (ww + gap), (float) wheels.getY() + 4, ww, (float) wheels.getHeight() - 8 };
    }
    juce::Rectangle<float> knobRect (int i) const
    {
        const float kw = knobArea.getWidth() / 8.0f;
        return { knobArea.getX() + i * kw, (float) knobArea.getY(), kw, (float) knobArea.getHeight() };
    }
    juce::Rectangle<float> padRect (int i) const
    {
        const float gap = 8.0f, pw = (padArea.getWidth() - gap * 7) / 8.0f, ph = (padArea.getHeight() - gap) / 2.0f;
        return { padArea.getX() + (i % 8) * (pw + gap), padArea.getY() + (i / 8) * (ph + gap), pw, ph };
    }

    // ---- painting
    void paintWheels (juce::Graphics& g)
    {
        const char* names[] { "Pitch", "Modulation" };
        for (int w = 0; w < 2; ++w)
        {
            auto r = wheelRect (w);
            g.setColour (juce::Colour (0xff101012)); g.fillRoundedRectangle (r.expanded (4), 6.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1b1e), r.getX(), 0, juce::Colour (0xff3a3b40), r.getCentreX(), 0, true));
            g.fillRoundedRectangle (r, 4.0f);
            // ridges move with the value
            const float v = w == 0 ? pitchValue : modValue;
            g.setColour (juce::Colour (0xff55565c));
            for (int k = -6; k <= 6; ++k)
            {
                const float y = r.getCentreY() + (k * 9.0f) - (v - 0.5f) * 60.0f;
                if (y > r.getY() + 3 && y < r.getBottom() - 3) g.drawHorizontalLine ((int) y, r.getX() + 3, r.getRight() - 3);
            }
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, true));
            g.drawText (names[w], r.withY (r.getBottom() + 4).withHeight (14).expanded (16, 0), juce::Justification::centred);
        }
    }

    void paintFaders (juce::Graphics& g)
    {
        g.setFont (VegaFonts::ui (11.0f, true));
        for (int i = 0; i < 9; ++i)
        {
            auto s = faderSlot (i);
            auto* vp = faderVolume (i);
            auto* t = i < 8 ? hui.trackForStrip (i) : nullptr;
            const bool sel = t != nullptr && t->itemID == vs.selectedTrack;
            const auto tint = t != nullptr ? DawSession::trackColour (*t) : VegaColours::accent;
            g.setColour (VegaColours::textDim);
            g.drawText (i == 8 ? juce::String ("MASTER") : juce::String (hui.bankStart + i + 1), juce::Rectangle<float> (s.getX() - 14, (float) faderArea.getY(), 50, 16), juce::Justification::centred);
            if (vp == nullptr)
            {   // no track on this fader yet: the fader is still there, dimmed
                g.beginTransparencyLayer (0.35f);
                fl49ui::drawFader (g, s, HuiSurface::faderPosFromDb (0.0f), VegaColours::textDim, false);
                fl49ui::drawMeter (g, meterRect (i), -100.0f);
                g.endTransparencyLayer();
                auto n = numberTag (i);
                g.setColour (juce::Colour (0xff3a3b40)); g.drawRoundedRectangle (n, 3.0f, 1.0f);
                g.setColour (VegaColours::textDim.withAlpha (0.7f)); g.setFont (VegaFonts::ui (10.0f, true));
                g.drawText ("-", n, juce::Justification::centred);
                g.setFont (VegaFonts::ui (11.0f, true));
                continue;
            }
            fl49ui::drawFader (g, s, HuiSurface::faderPosFromDb (vp->getVolumeDb()), tint, sel || i == 8);
            fl49ui::drawMeter (g, meterRect (i), i == 8 ? masterLevel : meters[(size_t) i].level);
            if (i == 8)
            {
                auto n = numberTag (i).withWidth (60.0f);
                g.setColour (juce::Colour (0xff3a3b40)); g.drawRoundedRectangle (n, 3.0f, 1.0f);
                g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (10.0f, true));
                g.drawText ("MASTER", n, juce::Justification::centred);
                g.setFont (VegaFonts::ui (11.0f, true));
                continue;
            }
            // number tag in the track's colour
            auto n = numberTag (i);
            g.setColour (sel ? tint : tint.withAlpha (0.25f)); g.fillRoundedRectangle (n, 3.0f);
            g.setColour (sel ? juce::Colour (0xff141414) : tint); g.setFont (VegaFonts::ui (11.0f, true));
            g.drawText (t != nullptr ? t->getName().substring (0, 7) : juce::String (i + 1), n, juce::Justification::centred);
            // fader button: mute / solo / arm
            auto b = faderButton (i);
            bool on = false; juce::Colour onCol;
            const juce::String labels[] { "MUTE", "SOLO", "ARM" };
            if (t != nullptr)
            {
                if (buttonMode == 0) { on = t->isMuted (false); onCol = juce::Colour (0xffffc23a); }
                else if (buttonMode == 1) { on = t->isSolo (false); onCol = juce::Colour (0xff3ccfff); }
                else { on = dawui::isArmed (*t); onCol = juce::Colour (0xffff4a4a); }
            }
            const auto modeCol = buttonMode == 0 ? juce::Colour (0xffffc23a) : buttonMode == 1 ? juce::Colour (0xff3ccfff) : juce::Colour (0xffff4a4a);
            g.setColour (on ? onCol : juce::Colour (0xff1a1a1d)); g.fillRoundedRectangle (b, 3.0f);
            g.setColour (modeCol.withAlpha (on ? 1.0f : 0.7f)); g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (on ? juce::Colour (0xff141414) : modeCol); g.setFont (VegaFonts::ui (9.5f, true));
            g.drawText (labels[buttonMode], b, juce::Justification::centred);
        }
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, true));
        g.drawText ("TRACK " + juce::String (hui.bankStart + 1) + "-" + juce::String (hui.bankStart + 8), faderArea.withTop (faderArea.getBottom() - 14), juce::Justification::centred);
    }

    void paintKnobs (juce::Graphics& g)
    {
        const auto now = juce::Time::getMillisecondCounter();
        for (int i = 0; i < 8; ++i)
        {
            auto r = knobRect (i);
            auto* p = fl49ui::knobParam (i, fl, hui, vs.session);
            const auto col = fl49ui::knobColour (i);
            // value box
            auto vb = juce::Rectangle<float> (r.getCentreX() - 26, r.getY(), 52, 18);
            g.setColour (juce::Colour (0xff121214)); g.fillRoundedRectangle (vb, 3.0f);
            g.setColour (p != nullptr ? col : VegaColours::textDim); g.setFont (VegaFonts::mono (11.0f));
            g.drawText (fl49ui::knobValue (p, hui), vb, juce::Justification::centred);
            // knob
            const float v = p != nullptr ? p->getCurrentNormalisedValue() : 0.0f;
            auto kr = juce::Rectangle<float> (0, 0, 58, 58).withCentre ({ r.getCentreX(), r.getY() + 46 });
            const float a = vegacontrols::wheelStart + v * (vegacontrols::wheelEnd - vegacontrols::wheelStart);
            // themed wheel, its fill in the knob's FLkey colour (brighter while you turn it)
            vegacontrols::paintWheel (g, kr, a, p != nullptr ? v : 0.0f, false, now - touched[(size_t) i] < 600,
                                      p != nullptr ? (now - touched[(size_t) i] < 600 ? col.brighter (0.4f) : col) : juce::Colour (0xff555555));
            g.setColour (p != nullptr ? VegaColours::text : VegaColours::textDim); g.setFont (VegaFonts::ui (10.5f, true));
            g.drawFittedText (fl49ui::knobName (i, fl, hui, vs.session), r.withTop (r.getBottom() - 20).toNearestInt(), juce::Justification::centred, 1);
        }
    }

    void paintPads (juce::Graphics& g, juce::uint32 now, bool blink)
    {
        for (int i = 0; i < 16; ++i)
        {
            auto r = padRect (i);
            const auto look = fl49ui::padLook (i, fl, hui, vs.session, blink);
            const bool hit = now - fl.padHitTime[(size_t) i] < 160;
            const bool teachThis = fl.isTeaching() && fl.teachStep() == i;
            fl49ui::drawPad (g, r.withTrimmedBottom (14), look, hit, teachThis, blink);
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.5f, true));
            g.drawFittedText (teachThis ? "HIT PAD " + juce::String (i + 1) : look.label, r.withTop (r.getBottom() - 13).toNearestInt(), juce::Justification::centred, 1);
        }
    }

    Fl49Hub& fl; HuiSurface& hui; ControllerHub& hub; ViewState& vs;
    juce::TextButton trackLeft, trackRight, btnMute, btnSolo, btnArm, knobPan, knobSends, knobPlugin, padDrums, padClips, padScenes, sceneUp, sceneDown, stopAll, playBtn;
    juce::Rectangle<int> left, wheels, faderArea, knobArea, padArea;
    int buttonMode = 0;                 // fader buttons: 0 mute, 1 solo, 2 arm
    float pitchValue = 0.5f, modValue = 0.0f;
    std::array<juce::uint32, 8> touched {};
    struct Meter { juce::ReferenceCountedObjectPtr<te::LevelMeterPlugin> plugin; te::LevelMeasurer::Client client; float level = -100.0f; };
    std::array<Meter, 8> meters;
    te::LevelMeasurer::Client masterClient; te::EditPlaybackContext* masterCtx = nullptr; float masterLevel = -100.0f;
    struct Drag { enum Kind { none, wheel, fader, knob } kind = none; int index = 0; float start = 0; } drag;
};
