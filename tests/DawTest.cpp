#include "../Source/DawSession.h"
#include "../Source/HuiSurface.h"
#include "../Source/ApcSurface.h"
#include "../Source/StemSeparator.h"
#include "../Source/StemClient.h"
#include "../Source/DawUI.h"
#include "../Source/Fl49Hub.h"
#include <iostream>
static int fails = 0;
static void check (bool ok, const juce::String& m) { std::cout << (ok ? "PASS " : "FAIL ") << m << "\n"; if (! ok) ++fails; }

static float peakOf (const juce::File& f, te::Engine& e)
{
    std::unique_ptr<juce::AudioFormatReader> r (e.getAudioFileFormatManager().readFormatManager.createReaderFor (f));
    if (r == nullptr) return -1;
    juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
    return b.getMagnitude (0, b.getNumSamples());
}


// level (dB) of a sine at frequency f in the middle of a rendered file
static float toneDb (const juce::File& f, te::Engine& e, double hz)
{
    std::unique_ptr<juce::AudioFormatReader> r (e.getAudioFileFormatManager().readFormatManager.createReaderFor (f));
    if (r == nullptr) return -999;
    const int n = 48000, start = (int) (r->sampleRate * 1.0);
    juce::AudioBuffer<float> b (1, n);
    r->read (&b, 0, n, start, true, false);
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / n), ph = 2 * juce::MathConstants<double>::pi * hz * i / r->sampleRate;
        re += b.getSample (0, i) * w * std::cos (ph); im += b.getSample (0, i) * w * std::sin (ph);
    }
    return (float) juce::Decibels::gainToDecibels (std::sqrt (re * re + im * im) * 4.0 / n, -200.0);
}

static juce::File writeTone (const juce::String& name, double hz, double seconds, float amp = 0.5f)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    f.deleteFile(); juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), 48000.0, 2, 24, {}, 0));
    juce::AudioBuffer<float> b (2, (int) (48000 * seconds));
    for (int i = 0; i < b.getNumSamples(); ++i) { const float v = amp * (float) std::sin (2 * juce::MathConstants<double>::pi * hz * i / 48000.0); b.setSample (0, i, v); b.setSample (1, i, v); }
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return f;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    te::Engine engine ("VegaDAWTest", std::make_unique<HeadlessUIBehaviour>(), nullptr);
    DawSession s (engine);
    {
        auto tracks = s.getTracks();
        check (tracks.size() == 2 && DawSession::kindOf (*tracks[0]) == DawSession::TrackKind::audio && DawSession::kindOf (*tracks[1]) == DawSession::TrackKind::midi,
               "new project: an audio track and a synth track");
        check (tracks[1]->pluginList[0] != nullptr && tracks[1]->pluginList[0]->isSynth(), "synth track has the built-in instrument");
    }
    s.setTempo (120.0);
    auto* synth = s.getTracks()[1];
    auto* bus = s.addTrack (DawSession::TrackKind::bus, "Reverb Bus");
    auto* drums = s.addTrack (DawSession::TrackKind::bus, "Drum Bus");
    check (bus != nullptr && s.getBuses().size() == 2 && s.busNumberFor (*bus) != s.busNumberFor (*drums), "buses get unique numbers");
    // routing
    auto* audio = s.getTracks()[0];
    s.routeToBus (*audio, drums);
    check (s.getOutputBus (*audio) == drums, "track output routed to a bus");
    s.setSend (*synth, *bus, -6.0f);
    check (s.findSend (*synth, *bus) != nullptr && std::abs (s.findSend (*synth, *bus)->getGainDb() + 6.0f) < 0.1f, "aux send at -6 dB");
    // plugins
    auto rev = s.addPlugin (*bus, te::ReverbPlugin::xmlTypeName);
    check (rev != nullptr && s.getInsertPlugins (*bus).size() == 1, "insert a reverb on the bus (send/return hidden from the insert list)");
    // MIDI clip with a chord
    auto clip = s.insertMidiClip (*synth, te::TimePosition(), s.barLength());
    check (clip != nullptr, "MIDI clip created");
    for (int n : { 60, 64, 67 }) clip->getSequence().addNote (n, te::BeatPosition::fromBeats (0), te::BeatDuration::fromBeats (2), 100, 0, nullptr);
    check (clip->getSequence().getNumNotes() == 3, "notes added");
    // split
    s.splitClip (*clip, te::TimePosition::fromSeconds (1.0));
    check (synth->getClips().size() == 2, "split a clip into two");
    // render
    auto out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_daw_test.wav");
    const bool ok = s.exportWav (out);
    const float pk = peakOf (out, engine);
    check (ok && pk > 0.01f && pk < 4.0f, "export WAV renders the synth (peak " + juce::String (pk, 3) + ")");
    // save / load round trip
    auto proj = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_daw_test.vegaproj");
    check (s.saveProjectAs (proj), "save project");
    DawSession s2 (engine);
    check (s2.openProject (proj), "open project");
    {
        auto t = s2.getTracks();
        te::AudioTrack* bus2 = nullptr; te::AudioTrack* drums2 = nullptr; te::AudioTrack* synth2 = nullptr; te::AudioTrack* audio2 = nullptr;
        for (auto* x : t) { if (x->getName() == "Reverb Bus") bus2 = x; if (x->getName() == "Drum Bus") drums2 = x; if (x->getName() == "Synth 1") synth2 = x; if (x->getName() == "Audio 1") audio2 = x; }
        check (t.size() == 4 && bus2 && drums2 && synth2 && audio2, "all 4 tracks restored");
        if (bus2 && synth2 && audio2 && drums2)
        {
            check (DawSession::kindOf (*bus2) == DawSession::TrackKind::bus && s2.findSend (*synth2, *bus2) != nullptr, "bus kind and send restored");
            check (s2.getOutputBus (*audio2) == drums2, "output routing restored");
            int notes = 0; for (auto* c : synth2->getClips()) if (auto* mc = dynamic_cast<te::MidiClip*> (c)) notes += mc->getSequence().getNumNotes();
            check (notes >= 3, "MIDI notes restored (" + juce::String (notes) + ")");
        }
    }
    // audio import + fades + warp
    {
        auto* t = s.getTracks()[0];
        auto clip2 = s.insertAudioFile (*t, out, te::TimePosition::fromSeconds (4));
        check (clip2 != nullptr && std::abs (clip2->getPosition().getStart().inSeconds() - 4.0) < 1e-6, "audio file imported at 4 s");
        if (clip2) { DawSession::setClipFades (*clip2, 0.05s, 0.5s); DawSession::setClipWarp (*clip2, true, 3.0f); check (clip2->getPitchChange() == 3.0f && clip2->getAutoTempo(), "clip warp + pitch set"); }
    }
    // ---- HUI control surface (Novation FLkey 2 DAW port)
    {
        DawSession h (engine);
        for (int i = 0; i < 10; ++i) h.addTrack (DawSession::TrackKind::audio);     // 12 tracks total
        h.addTrack (DawSession::TrackKind::bus, "FX");
        HuiSurface hui (h);
        std::vector<juce::MidiMessage> sent; hui.send = [&] (const juce::MidiMessage& m) { sent.push_back (m); };
        te::AudioTrack* selected = nullptr; hui.onSelectTrack = [&] (te::AudioTrack* t) { selected = t; };
        auto cc = [&] (int n, int v) { hui.handleIncoming (juce::MidiMessage::controllerEvent (1, n, v)); };
        auto press = [&] (int zone, int port) { cc (0x0f, zone); cc (0x2f, 0x40 | port); cc (0x0f, zone); cc (0x2f, port); };
        auto tracks = h.getTracks (true);
        // fader 3 to the 0 dB position
        const int pos = juce::roundToInt (HuiSurface::faderPosFromDb (0.0f) * 16383);
        cc (0x02, pos >> 7); cc (0x22, pos & 0x7f);
        check (std::abs (tracks[2]->getVolumePlugin()->getVolumeDb()) < 0.3f, "HUI fader 3 sets track 3 volume (" + juce::String (tracks[2]->getVolumePlugin()->getVolumeDb(), 2) + " dB)");
        cc (0x02, 0); cc (0x22, 0);
        check (tracks[2]->getVolumePlugin()->getVolumeDb() < -60.0f, "HUI fader down = silent");
        press (1, 2);  check (tracks[1]->isMuted (false), "HUI pad: mute strip 2");
        press (1, 3);  check (tracks[1]->isSolo (false), "HUI pad: solo strip 2");
        press (4, 1);  check (selected == tracks[4], "HUI fader button selects track 5");
        cc (0x40, 0x45); check (tracks[0]->getVolumePlugin()->getPan() > 0.05f, "HUI encoder 1 pans right (" + juce::String (tracks[0]->getVolumePlugin()->getPan(), 2) + ")");
        press (0x0a, 3); check (hui.bankStart == 8 && hui.trackForStrip (0) == tracks[8], "HUI bank right shows tracks 9-16");
        cc (0x00, pos >> 7); cc (0x20, pos & 0x7f);
        check (std::abs (tracks[8]->getVolumePlugin()->getVolumeDb()) < 0.3f, "after banking, fader 1 controls track 9");
        press (0x0a, 1); check (hui.bankStart == 0, "HUI bank left back to tracks 1-8");
        press (0x0b, 7); auto* bus = h.getBuses()[0];
        for (int k = 0; k < 10; ++k) cc (0x40, 0x48);
        check (h.findSend (*tracks[0], *bus) != nullptr && h.findSend (*tracks[0], *bus)->getGainDb() > -30.0f, "Send A mode: encoder creates and raises a send to the first bus");
        press (0x0b, 2);
        press (0x0f, 3); check (h.edit().getTransport().looping.get(), "HUI loop button toggles loop");
        press (0x0e, 2); check (h.edit().getTransport().getPosition().inSeconds() >= h.barLength().inSeconds() - 0.01, "HUI fast-forward jumps a bar");
        press (0x0f, 0); check (h.edit().getTransport().getPosition().inSeconds() < 0.01, "HUI return-to-zero");
        // feedback
        sent.clear(); hui.updateFeedback (true);
        bool faderFb = false, muteLed = false, nameSx = false;
        for (size_t i = 0; i + 1 < sent.size(); ++i)
        {
            auto& m = sent[i];
            if (m.isController() && m.getControllerNumber() == 0x0c && m.getControllerValue() == 1 && sent[i + 1].isController() && sent[i + 1].getControllerValue() == (0x40 | 2)) muteLed = true;
            if (m.isController() && m.getControllerNumber() == 0x01 && sent[i + 1].isController() && sent[i + 1].getControllerNumber() == 0x21) faderFb = true;
            if (m.isSysEx() && m.getSysExDataSize() == 11 && m.getSysExData()[5] == 0x10) nameSx = true;
        }
        check (faderFb && muteLed && nameSx, "HUI feedback: motor-fader positions, mute LED and track names are sent");
        sent.clear(); hui.updateFeedback();
        check (sent.empty(), "HUI feedback only sends changes (" + juce::String ((int) sent.size()) + " msgs when idle)");
    }
    // ---- VEGA FL49: scale / chord keys, plug-in macro knobs, pads
    {
        fl49::NoteProcessor p; fl49::NoteProcessor::Out o[32];
        fl49::Settings st; st.root = 9; st.scale = 1; st.keys = fl49::KeyMode::snap; p.setSettings (st);     // A minor, snap
        int n = p.process (1, 61, 100, true, o);
        check (n == 1 && o[0].note == 60 && o[0].on, "FL49 snap: C# plays C in A minor");
        n = p.process (1, 61, 0, false, o);
        check (n == 1 && o[0].note == 60 && ! o[0].on, "FL49 snap: the note-off follows the snapped note");
        st.keys = fl49::KeyMode::filter; p.setSettings (st);
        check (p.process (1, 61, 100, true, o) == 0 && p.process (1, 61, 0, false, o) == 0, "FL49 filter: out-of-key notes (and their note-offs) don't play");
        st.keys = fl49::KeyMode::snap; st.chord = fl49::ChordMode::seventh; p.setSettings (st);
        n = p.process (1, 57, 100, true, o);
        check (n == 4 && o[0].note == 57 && o[1].note == 60 && o[2].note == 64 && o[3].note == 67, "FL49 chords: A plays Am7 (A C E G) in A minor");
        st.chord = fl49::ChordMode::off; p.setSettings (st);
        check (p.process (1, 57, 0, false, o) == 4, "FL49 chords: changing mode while a key is down still releases the whole chord");
        st.chord = fl49::ChordMode::seventh; st.root = 0; st.scale = 0; p.setSettings (st);
        p.process (1, 60, 100, true, o); p.process (1, 64, 100, true, o);
        n = p.process (1, 60, 0, false, o);
        check (n == 1 && o[0].note == 60, "FL49 chords: overlapping chords keep their shared notes");
        check (p.process (1, 64, 0, false, o) == 4, "FL49 chords: last key up ends every note");
        fl49::PadMap pm;
        check (pm.padFor (10, 36) == 8 && pm.padFor (10, 51) == 7 && pm.padFor (1, 36) == -1, "FL49 pads: default Drum-mode map (ch 10, 36 bottom-left)");

        // knobs in PLUGIN mode turn the selected track's plug-in
        DawSession d (engine);
        auto* syn = d.getTracks (false)[1];
        Fl49Hub fl (engine, d);
        fl.getSelectedTrack = [syn] { return syn; };
        fl.getBankStart = [] { return 0; };
        HuiSurface hui (d);
        hui.macroParam = [&fl] (int k) { return fl.macroParam (k); };
        hui.encMode = HuiSurface::EncMode::plugin;
        auto* mp = fl.macroParam (0);
        check (mp != nullptr && fl.macroPlugin() == syn->pluginList[0], "FL49 macros: knob 1 = the instrument's first parameter (" + (mp != nullptr ? mp->getParameterName() : juce::String ("none")) + ")");
        if (mp != nullptr)
        {
            mp->setNormalisedParameter (0.5f, juce::sendNotification);
            const float before = mp->getCurrentNormalisedValue();
            for (int k = 0; k < 5; ++k) hui.handleIncoming (juce::MidiMessage::controllerEvent (1, 0x40, 0x42));
            check (mp->getCurrentNormalisedValue() > before + 0.05f, "FL49 macros: turning encoder 1 turns the plug-in knob (" + juce::String (before, 2) + " -> " + juce::String (mp->getCurrentNormalisedValue(), 2) + ")");
            check (std::abs (d.getTracks (true)[0]->getVolumePlugin()->getPan()) < 0.001f, "FL49 macros: ...and doesn't pan track 1");
        }
        // key settings are saved with the project
        fl49::Settings ks; ks.root = 5; ks.scale = 2; ks.keys = fl49::KeyMode::snap; ks.chord = fl49::ChordMode::soul;
        fl.setKeySettings (ks);
        fl.loadKeySettingsFromEdit();
        const auto back = fl.keySettings();
        check (back.root == 5 && back.scale == 2 && back.keys == fl49::KeyMode::snap && back.chord == fl49::ChordMode::soul && (int) d.edit().state.getProperty ("vegaKeyRoot") == 5,
               "FL49 key / scale / chord mode are stored in the project");
        // pads in CLIPS mode launch the slot under them (pad 2 = track 2, top row = first scene)
        d.ensureScenes (2);
        auto* clip = d.createSlotMidiClip (*syn, 0, 1);
        fl.setPadMode (Fl49Hub::PadMode::clips);
        fl.pressPad (1, 100);
        check (clip != nullptr && (DawSession::isSlotPlaying (*clip) || DawSession::isSlotQueued (*clip)), "FL49 pads: CLIPS mode launches the clip under the pad");
        fl.pressPad (1, 100);
        check (clip != nullptr && (! DawSession::isSlotPlaying (*clip) || DawSession::isSlotQueued (*clip)), "FL49 pads: pressing it again stops it (now or on the next bar)");
        d.edit().getTransport().stop (false, true);
        fl.setPadMode (Fl49Hub::PadMode::drums);
    }
    {   // master tape machine (the reel-to-reel)
        DawSession d (engine);
        auto* tp = d.masterTape (false);
        check (tp != nullptr && ! tp->isEnabled() && d.edit().getMasterPluginList().indexOf (tp) < d.edit().getMasterPluginList().indexOf (d.masterStrip (false)),
               "master tape machine is there, bypassed, before the master strip");
        auto presets = VegaTapeMachinePlugin::presets();
        tp->apply (presets[2]);
        check (std::abs (tp->ips->getCurrentValue() - 0.0f) < 0.01f && std::abs (tp->drive->getCurrentValue() - 0.7f) < 0.01f && tp->presetName.get() == presets[2].name,
               "tape preset sets DRIVE / IPS / CAL (" + presets[2].name + ")");
    }
    {   // FL-style snapping: "Line" follows the zoom, Bar snaps to whole bars (it used to switch snapping off)
        DawSession d (engine);
        ViewState v (d);
        v.snapMode = ViewState::Snap::bar;
        check (std::abs (v.snapBeat (5.2) - 4.0) < 1e-6 && std::abs (v.snapBeat (6.4) - 8.0) < 1e-6, "Snap: Bar snaps to whole bars (5.2 -> " + juce::String (v.snapBeat (5.2)) + ")");
        v.snapMode = ViewState::Snap::line;
        v.pixelsPerBeat = 60;  const double fine = v.snapBeat (1.13);
        v.pixelsPerBeat = 28;  const double mid = v.snapBeat (1.13);
        v.pixelsPerBeat = 6;   const double coarse = v.snapBeat (1.13);
        check (std::abs (fine - 1.25) < 1e-6 && std::abs (mid - 1.0) < 1e-6 && std::abs (coarse - 0.0) < 1e-6,
               "Snap: Line follows the zoom (1/16 zoomed in, 1/8, then bars zoomed out: " + juce::String (fine) + ", " + juce::String (mid) + ", " + juce::String (coarse) + ")");
        check (std::abs (v.snapBeat (1.13, true) - 1.13) < 1e-9, "Shift moves freely");
        v.snapMode = ViewState::Snap::off;
        check (std::abs (v.snapBeat (1.13) - 1.13) < 1e-9, "Snap: Off");
    }
    {   // DJ EQ / filter on every channel
        DawSession d (engine);
        auto tone = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_dj_tones.wav");
        {
            tone.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (tone), 48000.0, 2, 24, {}, 0));
            juce::AudioBuffer<float> b (2, 48000 * 3);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / 48000.0;
                const float v = (float) (0.2 * std::sin (2 * juce::MathConstants<double>::pi * 80 * t) + 0.2 * std::sin (2 * juce::MathConstants<double>::pi * 1000 * t) + 0.2 * std::sin (2 * juce::MathConstants<double>::pi * 8000 * t));
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        auto* t = d.getTracks()[0];
        d.insertAudioFile (*t, tone, te::TimePosition());
        auto* eq = d.djEq (*t, false);
        check (eq != nullptr && d.getInsertPlugins (*t).isEmpty(), "every channel has a DJ EQ, hidden from the insert list");
        auto render = [&] (float hi, float mid, float low, float filt, const char* name)
        {
            eq->band[0]->setParameter (hi, juce::sendNotificationSync); eq->band[1]->setParameter (mid, juce::sendNotificationSync);
            eq->band[2]->setParameter (low, juce::sendNotificationSync); eq->filter->setParameter (filt, juce::sendNotificationSync);
            auto o = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (juce::String ("vega_dj_") + name + ".wav");
            d.exportWav (o, te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.5)));
            std::array<float, 3> r { toneDb (o, engine, 80), toneDb (o, engine, 1000), toneDb (o, engine, 8000) };
            o.deleteFile();
            return r;
        };
        const auto flat = render (0, 0, 0, 0, "flat");
        check (std::abs (flat[0] - flat[1]) < 1.0f && std::abs (flat[2] - flat[1]) < 1.0f && flat[1] > -20.0f,
               "DJ EQ flat at 0 dB (80 Hz " + juce::String (flat[0], 1) + ", 1 kHz " + juce::String (flat[1], 1) + ", 8 kHz " + juce::String (flat[2], 1) + " dB)");
        auto lowKill = render (0, 0, VegaDJEQPlugin::killDb, 0, "lowkill");
        check (lowKill[0] < flat[0] - 30 && std::abs (lowKill[1] - flat[1]) < 1.5f && std::abs (lowKill[2] - flat[2]) < 1.0f, "LOW kill removes the bass (" + juce::String (lowKill[0] - flat[0], 1) + " dB), mids/highs stay");
        auto midKill = render (0, VegaDJEQPlugin::killDb, 0, 0, "midkill");
        check (midKill[1] < flat[1] - 25 && std::abs (midKill[0] - flat[0]) < 1.5f && std::abs (midKill[2] - flat[2]) < 1.5f, "MID kill (" + juce::String (midKill[1] - flat[1], 1) + " dB)");
        auto hiKill = render (VegaDJEQPlugin::killDb, 0, 0, 0, "hikill");
        check (hiKill[2] < flat[2] - 30 && std::abs (hiKill[0] - flat[0]) < 1.0f, "HI kill removes the highs (" + juce::String (hiKill[2] - flat[2], 1) + " dB)");
        auto boost = render (6, 0, 0, 0, "boost");
        check (std::abs (boost[2] - flat[2] - 6.0f) < 1.0f, "HI +6 dB boost (" + juce::String (boost[2] - flat[2], 1) + " dB)");
        auto lpf = render (0, 0, 0, -0.6f, "lpf");
        check (lpf[2] < flat[2] - 20 && std::abs (lpf[0] - flat[0]) < 1.5f, "filter left = low-pass: 8 kHz " + juce::String (lpf[2] - flat[2], 1) + " dB, bass kept");
        auto hpf = render (0, 0, 0, 0.6f, "hpf");
        check (hpf[0] < flat[0] - 20 && std::abs (hpf[2] - flat[2]) < 1.5f, "filter right = high-pass: 80 Hz " + juce::String (hpf[0] - flat[0], 1) + " dB, highs kept");
        // tape stage: 15 ips softens the top, keeps the level (unity), thickens the lows a little
        eq->tapeValue = 1;
        auto tape15 = render (0, 0, 0, 0, "tape15");
        eq->tapeValue = 2;
        auto tape30 = render (0, 0, 0, 0, "tape30");
        eq->tapeValue = 0;
        check (tape15[2] < flat[2] - 1.0f && tape30[2] > tape15[2] && std::abs (tape15[1] - flat[1]) < 1.5f && tape15[0] >= flat[0] - 0.5f && tape15[0] < flat[0] + 3.0f,
               "Tape: 15 ips softens 8 kHz (" + juce::String (tape15[2] - flat[2], 1) + " dB), 30 ips less (" + juce::String (tape30[2] - flat[2], 1)
               + " dB), 1 kHz level kept (" + juce::String (tape15[1] - flat[1], 1) + " dB), lows " + juce::String (tape15[0] - flat[0], 1) + " dB");
        // settings are saved with the project
        eq->tapeValue = 2;
        eq->band[2]->setParameter (-12.0f, juce::sendNotificationSync); eq->filter->setParameter (0.4f, juce::sendNotificationSync);
        auto pf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_dj.vegaproj");
        d.saveProjectAs (pf);
        DawSession d2 (engine); d2.openProject (pf);
        auto* eq2 = d2.djEq (*d2.getTracks()[0], false);
        check (eq2 != nullptr && std::abs (eq2->band[2]->getCurrentValue() + 12.0f) < 0.1f && std::abs (eq2->filter->getCurrentValue() - 0.4f) < 0.01f, "DJ EQ and filter settings restored from the project");
        check (eq2 != nullptr && eq2->tapeValue.get() == 2, "the track's tape setting is restored from the project");
        check (d2.getInsertPlugins (*d2.getTracks()[0]).isEmpty(), "reopened project doesn't get a second DJ EQ");
        int count = 0; for (auto* p : d2.getTracks()[0]->pluginList.getPlugins()) if (dynamic_cast<VegaDJEQPlugin*> (p)) ++count;
        check (count == 1, "exactly one DJ EQ per channel after reopening");
        pf.deleteFile(); tone.deleteFile();
    }
    {   // master strip: EQ, filters, compressor, gate on the master bus
        DawSession m (engine);
        auto* ms = m.masterStrip (false);
        check (ms != nullptr, "master bus has the master strip");
        auto tone = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_ms_tones.wav");
        {
            tone.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (tone), 48000.0, 2, 24, {}, 0));
            juce::AudioBuffer<float> b (2, 48000 * 3);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / 48000.0;
                const float v = (float) (0.2 * std::sin (2 * juce::MathConstants<double>::pi * 50 * t) + 0.2 * std::sin (2 * juce::MathConstants<double>::pi * 1000 * t) + 0.2 * std::sin (2 * juce::MathConstants<double>::pi * 12000 * t));
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        m.insertAudioFile (*m.getTracks()[0], tone, te::TimePosition());
        auto render = [&] (std::function<void()> setup, const char* name)
        {
            for (int i = 0; i < VegaMasterStripPlugin::numParams; ++i) ms->params[i]->setParameter (VegaMasterStripPlugin::def (i).def, juce::sendNotificationSync);
            setup();
            auto o = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (juce::String ("vega_ms_") + name + ".wav");
            m.exportWav (o, te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.5)));
            std::array<float, 3> r { toneDb (o, engine, 50), toneDb (o, engine, 1000), toneDb (o, engine, 12000) };
            o.deleteFile();
            return r;
        };
        using M = VegaMasterStripPlugin;
        auto setp = [&] (int p, float v) { ms->params[p]->setParameter (v, juce::sendNotificationSync); };
        const auto flat = render ([] {}, "flat");
        auto hp = render ([&] { setp (M::hpf, 200); }, "hp");
        check (hp[0] < flat[0] - 20 && std::abs (hp[1] - flat[1]) < 1.0f, "master HP filter at 200 Hz cuts 50 Hz by " + juce::String (flat[0] - hp[0], 1) + " dB");
        auto lp = render ([&] { setp (M::lpf, 3000); }, "lp");
        check (lp[2] < flat[2] - 15 && std::abs (lp[1] - flat[1]) < 1.0f, "master LP filter at 3 kHz cuts 12 kHz by " + juce::String (flat[2] - lp[2], 1) + " dB");
        auto hmf = render ([&] { setp (M::hmfGain, 10); setp (M::hmfFreq, 1000); }, "hmf");
        check (std::abs (hmf[1] - flat[1] - 10.0f) < 1.0f && std::abs (hmf[0] - flat[0]) < 1.0f, "HMF bell +10 dB at 1 kHz (" + juce::String (hmf[1] - flat[1], 1) + " dB)");
        auto lfShelf = render ([&] { setp (M::lfGain, -12); setp (M::lfFreq, 150); }, "lf");
        check (lfShelf[0] < flat[0] - 9 && std::abs (lfShelf[2] - flat[2]) < 0.5f, "LF shelf -12 dB (50 Hz " + juce::String (lfShelf[0] - flat[0], 1) + " dB)");
        auto eqOff = render ([&] { setp (M::hmfGain, 10); setp (M::eqOn, 0); }, "eqoff");
        check (std::abs (eqOff[1] - flat[1]) < 0.3f, "EQ In switch bypasses the EQ");
        auto comp = render ([&] { setp (M::compRatio, 20); setp (M::compThresh, -24); }, "comp");
        float grSeen = ms->grComp.exchange (0.0f);
        check (grSeen > 3.0f, "compressor reduces gain (" + juce::String (grSeen, 1) + " dB GR at -24 dB / 20:1)");
        check (comp[1] < flat[1] + 6.0f, "compressor with auto make-up stays controlled");
        auto trim = render ([&] { setp (M::outGain, -6); }, "trim");
        check (std::abs (trim[1] - flat[1] + 6.0f) < 0.2f, "output trim -6 dB");
        auto cutR = render ([&] { setp (M::cut, 1); }, "cut");
        check (cutR[1] < -100.0f, "CUT silences the master");
        auto gate = render ([&] { setp (M::gateRange, 40); setp (M::gateThresh, -3); }, "gate");
        check (gate[1] < flat[1] - 30, "gate closes below threshold (" + juce::String (gate[1] - flat[1], 1) + " dB)");
        // saved with the project
        setp (M::hfGain, 4.5f);
        auto pf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_ms.vegaproj");
        m.saveProjectAs (pf);
        DawSession m2 (engine); m2.openProject (pf);
        auto* ms2 = m2.masterStrip (false);
        int count = 0; for (auto* p : m2.edit().getMasterPluginList().getPlugins()) if (dynamic_cast<VegaMasterStripPlugin*> (p)) ++count;
        check (ms2 != nullptr && count == 1 && std::abs (ms2->get (M::hfGain) - 4.5f) < 0.01f, "master strip settings restored, one instance");
        pf.deleteFile(); tone.deleteFile();
    }
    {   // stem placement: stems line up with the original clip, original gets muted
        DawSession p (engine);
        auto tmpDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_stems_test");
        tmpDir.deleteRecursively(); tmpDir.createDirectory();
        juce::Array<juce::File> files;
        for (auto name : { "Drums", "Bass", "Other", "Vocals" })
        {
            auto f = tmpDir.getChildFile (juce::String ("song - ") + name + ".wav");
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), 44100.0, 2, 24, {}, 0));
            juce::AudioBuffer<float> b (2, 44100 * 4); b.clear(); b.setSample (0, 100, 0.5f);
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            files.add (f);
        }
        auto* src = p.getTracks()[0];
        auto orig = p.insertAudioFile (*src, files[0], te::TimePosition::fromSeconds (2.0));
        const int before = p.getTracks().size();
        auto made = p.placeStems (files, StemSeparator::stemNames (StemSeparator::Model::fourStems), "song", orig.get());
        bool aligned = made.size() == 4;
        for (auto* t : made) if (t->getClips().size() != 1 || std::abs (t->getClips()[0]->getPosition().getStart().inSeconds() - 2.0) > 1e-6) aligned = false;
        check (aligned && p.getTracks().size() == before + 4 && orig->isMuted(), "separated stems land on 4 new tracks lined up with the clip; original muted");
        check (made.size() == 4 && made[3]->getName() == "song Vocals", "stem tracks are named after the clip (" + (made.size() == 4 ? made[3]->getName() : juce::String()) + ")");
        tmpDir.deleteRecursively();
    }
    {   // real AI separation (runs when the Demucs model is available, e.g. in CI)
        const juce::String modelPath = juce::SystemStats::getEnvironmentVariable ("VEGA_DEMUCS_MODEL", {});
        if (modelPath.isEmpty() || ! juce::File (modelPath).existsAsFile())
            std::cout << "SKIP Demucs separation (set VEGA_DEMUCS_MODEL to a ggml-model-htdemucs-4s-f16.bin to run it)\n";
        else
        {
            const int n = 44100 * 16;
            juce::AudioBuffer<float> mix (2, n); mix.clear();
            juce::Random rnd (3);
            const double tau = 2 * juce::MathConstants<double>::pi;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / 44100.0, beat = std::fmod (t, 0.5), half = std::fmod (t, 0.25);
                const double bassF = std::fmod (t, 2.0) < 1.0 ? 55.0 : 73.4;
                double v = 0.28 * std::sin (tau * bassF * t) * (0.6 + 0.4 * std::exp (-beat * 4));                     // bass line
                v += 0.6 * std::sin (tau * (50 + 90 * std::exp (-beat * 30)) * beat) * std::exp (-beat * 14);          // kick
                v += 0.12 * (rnd.nextFloat() * 2 - 1) * std::exp (-half * 120);                                       // hats
                const double f0 = 220 * std::pow (2.0, (std::floor (t) == 3 || std::floor (t) == 9 ? 3 : 0) / 12.0) * (1 + 0.01 * std::sin (tau * 5.5 * t));
                double voice = 0; for (int k = 1; k <= 12; ++k) voice += std::sin (tau * f0 * k * t) / k * (k == 3 || k == 4 ? 2.0 : 1.0);
                v += 0.08 * voice * (std::fmod (t, 4.0) < 3.0 ? 1.0 : 0.0);                                             // sung line
                mix.setSample (0, i, (float) v); mix.setSample (1, i, (float) v);
            }
            std::vector<juce::AudioBuffer<float>> stems; juce::String err;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            float lastP = 0;
            const bool ok = StemSeparator::separateBuffer (juce::File (modelPath), mix, stems, [&] (float pr, const juce::String&) { lastP = pr; return true; }, err, 2);
            const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
            check (ok && stems.size() == 4, "Demucs separated 16 s into 4 stems in " + juce::String (secs, 1) + " s" + (err.isNotEmpty() ? " (" + err + ")" : juce::String()));
            if (ok && stems.size() == 4)
            {
                double e = 0, r = 0;
                for (int i = 0; i < n; ++i) { float sum = 0; for (auto& st : stems) sum += st.getSample (0, i); e += (sum - mix.getSample (0, i)) * (sum - mix.getSample (0, i)); r += mix.getSample (0, i) * mix.getSample (0, i); }
                const double recon = 10 * std::log10 (e / r + 1e-12);
                check (recon < -10, "stems add back up to the mix (error " + juce::String (recon, 1) + " dB)");
                auto bandEnergy = [&] (const juce::AudioBuffer<float>& b, double lo, double hi)
                {
                    juce::dsp::FFT fft (13); const int N = 1 << 13; double sum = 0;
                    for (int off = 0; off + N <= n; off += N)
                    {
                        std::vector<float> d (2 * N, 0.0f);
                        for (int i = 0; i < N; ++i) d[(size_t) i] = b.getSample (0, off + i) * (float) (0.5 - 0.5 * std::cos (tau * i / N));
                        fft.performFrequencyOnlyForwardTransform (d.data());
                        for (int k = (int) (lo * N / 44100); k <= (int) (hi * N / 44100); ++k) sum += (double) d[(size_t) k] * d[(size_t) k];
                    }
                    return sum;
                };
                // stems: 0 drums, 1 bass, 2 other, 3 vocals
                double bassLow[4], hiBand[4], bassTot = 0, hiTot = 0;
                for (int s2 = 0; s2 < 4; ++s2) { bassLow[s2] = bandEnergy (stems[(size_t) s2], 52, 78); hiBand[s2] = bandEnergy (stems[(size_t) s2], 7000, 16000); bassTot += bassLow[s2]; hiTot += hiBand[s2]; }
                check (bassLow[1] / bassTot > 0.4, "bass line goes to the Bass stem (" + juce::String (100 * bassLow[1] / bassTot, 0) + "% of 52-78 Hz)");
                check (hiBand[0] / hiTot > 0.4, "hi-hats go to the Drums stem (" + juce::String (100 * hiBand[0] / hiTot, 0) + "% of 7-16 kHz)");
            }
        }
    }
    {   // without the model: clean error
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        auto r = StemSeparator::separate (StemSeparator::Model::sixStems, juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("nope.wav"),
                                          juce::File::getSpecialLocation (juce::File::tempDirectory), fm, nullptr);
        check (! r.ok && r.error.isNotEmpty(), "separation reports a clear error when it can't run (" + r.error + ")");
    }
   #if ! JUCE_WINDOWS
    {   // DAW side of the helper: a helper that crashes mid-way is reported (DAW keeps running); a good run is read back
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_helper_test");
        dir.deleteRecursively(); dir.createDirectory();
        auto script = [&] (const char* name, const juce::String& body) { auto f = dir.getChildFile (name); f.replaceWithText ("#!/bin/sh\n" + body, false, false, "\n"); f.setExecutePermission (true); return f; };
        auto crasher = script ("crash.sh", "echo 'PROGRESS 0.0300 Separating: 3%'\nkill -SEGV $$\n");
        auto good = script ("good.sh", "echo 'load_demucs_model: chatter'\necho 'PROGRESS 0.5000 Separating: 50%  Freq encoder 2'\necho \"STEM " + dir.getFullPathName() + "/a - Drums.wav\"\necho \"STEM " + dir.getFullPathName() + "/a - Bass.wav\"\necho DONE\n");
        auto failing = script ("fail.sh", "echo 'ERROR Not enough memory to separate this file.'\nexit 2\n");
        bool crashed = false, cancelled = false; float seen = 0;
        auto prog = [&] (float p, const juce::String&) { if (p >= 0) seen = p; return true; };
        auto r1 = StemClient::runOnce (crasher, StemSeparator::Model::fourStems, dir, dir, prog, crashed, cancelled);
        check (! r1.ok && crashed && r1.error.contains ("stopped unexpectedly") && std::abs (seen - 0.03f) < 1e-4f, "helper crash at 3% is caught and reported, DAW survives (" + r1.error.upToFirstOccurrenceOf ("\n", false, false) + ")");
        auto r2 = StemClient::runOnce (good, StemSeparator::Model::fourStems, dir, dir, prog, crashed, cancelled);
        check (r2.ok && ! crashed && r2.stems.size() == 2 && r2.stems[1].getFileName() == "a - Bass.wav" && std::abs (seen - 0.5f) < 1e-4f, "helper progress and stem files are read back");
        auto r3 = StemClient::runOnce (failing, StemSeparator::Model::fourStems, dir, dir, prog, crashed, cancelled);
        check (! r3.ok && ! crashed && r3.error == "Not enough memory to separate this file.", "helper error message is passed through");
        int calls = 0;
        auto r4 = StemClient::runOnce (good, StemSeparator::Model::fourStems, dir, dir, [&] (float, const juce::String&) { return ++calls < 3; }, crashed, cancelled);
        check (! r4.ok && cancelled && r4.error.isEmpty(), "cancel stops the helper");
        dir.deleteRecursively();
    }
   #endif
    {   // automation: a volume ramp written into the curve plays back in the render, and is saved
        DawSession a (engine);
        auto tone = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_auto_tone.wav");
        {
            tone.deleteFile(); juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (tone), 48000.0, 2, 24, {}, 0));
            juce::AudioBuffer<float> b (2, 48000 * 4);
            for (int i = 0; i < b.getNumSamples(); ++i) { const float v = 0.3f * (float) std::sin (2 * juce::MathConstants<double>::pi * 440 * i / 48000.0); b.setSample (0, i, v); b.setSample (1, i, v); }
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        auto* t = a.getTracks()[0];
        a.insertAudioFile (*t, tone, te::TimePosition());
        auto vp = t->getVolumePlugin();
        auto& curve = vp->volParam->getCurve();
        curve.addPoint (te::TimePosition::fromSeconds (0.5), te::decibelsToVolumeFaderPosition (0.0f), 0.0f, nullptr);
        curve.addPoint (te::TimePosition::fromSeconds (2.5), te::decibelsToVolumeFaderPosition (-24.0f), 0.0f, nullptr);
        check (dawui::laneParam (*t) == vp->volParam.get() && dawui::automatableParams (*t).size() >= 6, "automation lane lists the track's parameters (" + juce::String (dawui::automatableParams (*t).size()) + "), volume by default");
        auto o = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_auto.wav");
        a.exportWav (o, te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (3.5)));
        std::unique_ptr<juce::AudioFormatReader> r (engine.getAudioFileFormatManager().readFormatManager.createReaderFor (o));
        float early = 0, late = 0;
        if (r != nullptr)
        {
            juce::AudioBuffer<float> b (2, (int) r->lengthInSamples); r->read (&b, 0, b.getNumSamples(), 0, true, true);
            early = b.getRMSLevel (0, (int) (0.1 * r->sampleRate), (int) (0.3 * r->sampleRate));
            late = b.getRMSLevel (0, (int) (2.8 * r->sampleRate), (int) (0.4 * r->sampleRate));
        }
        const float drop = juce::Decibels::gainToDecibels (late) - juce::Decibels::gainToDecibels (early);
        check (std::abs (drop + 24.0f) < 2.0f, "volume automation ramp plays back (" + juce::String (drop, 1) + " dB from start to end)");
        auto pf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_auto.vegaproj");
        a.saveProjectAs (pf);
        DawSession a2 (engine); a2.openProject (pf);
        check (a2.getTracks()[0]->getVolumePlugin()->volParam->getCurve().getNumPoints() == 2, "automation points saved with the project");
        o.deleteFile(); pf.deleteFile(); tone.deleteFile();
    }
    {   // freeze: the synth track renders to a freeze file and still plays back
        DawSession fz (engine);
        auto* synth = fz.getTracks()[1];
        auto clip = fz.insertMidiClip (*synth, te::TimePosition(), te::TimeDuration::fromSeconds (2));
        if (clip) clip->getSequence().addNote (60, te::BeatPosition(), te::BeatDuration::fromBeats (2), 100, 0, nullptr);
        auto res = synth->setFrozen (true, te::Track::individualFreeze);
        check (res.wasOk() && synth->isFrozen (te::Track::individualFreeze), "freeze renders the synth track to audio" + (res.failed() ? " (" + res.getErrorMessage() + ")" : juce::String()));
        auto o = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_freeze.wav");
        fz.exportWav (o, te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2)));
        const float pk = peakOf (o, engine);
        check (pk > 0.01f, "frozen track still plays (peak " + juce::String (pk, 3) + ")");
        check (synth->setFrozen (false, te::Track::individualFreeze).wasOk() && ! synth->isFrozen (te::Track::individualFreeze), "unfreeze");
        o.deleteFile();
    }
    {   // MIDI learn mapping: a CC moves the mapped parameter and the mapping is saved
        DawSession ml (engine);
        auto* t = ml.getTracks()[0];
        auto* dj = ml.djEq (*t, false);
        auto& pcm = ml.edit().getParameterControlMappings();
        // the real learn flow: learn on -> touch a control on screen -> move a CC
        engine.getMidiLearnState().setActive (true);
        dj->filter->setParameter (0.2f, juce::sendNotificationSync);
        pcm.sendChange (21, 0.5f, 1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        engine.getMidiLearnState().setActive (false);
        int ch = -1, cc = -1;
        check (pcm.getParameterMapping (*dj->filter, ch, cc) && cc == 21, "MIDI learn maps CC 21 to the touched control (cc " + juce::String (cc) + ")");
        pcm.sendChange (21, 1.0f, 1);
        check (dj->filter->getCurrentValue() > 0.9f, "MIDI-learned CC 21 moves the DJ filter (" + juce::String (dj->filter->getCurrentValue(), 2) + ")");
        pcm.sendChange (21, 0.0f, 1);
        check (dj->filter->getCurrentValue() < -0.9f, "and back down");
        check (vegaFocusedEdit == &ml.edit(), "controllers follow the open project");
        auto pf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_ml.vegaproj");
        pcm.saveToEdit(); ml.saveProjectAs (pf);
        DawSession ml2 (engine); ml2.openProject (pf);
        auto& pcm2 = ml2.edit().getParameterControlMappings();
        pcm2.loadFromEdit();
        auto* dj2 = ml2.djEq (*ml2.getTracks()[0], false);
        pcm2.sendChange (21, 1.0f, 1);
        check (pcm2.getNumControllerIDs() >= 1 && dj2->filter->getCurrentValue() > 0.9f, "MIDI mappings saved with the project");
        pf.deleteFile();
    }
    {   // Mackie Control: Tracktion's MCU driver moves our faders
        DawSession mc (engine);
        te::SelectionManager sel (engine);
        auto& ecm = engine.getExternalControllerManager();
        ecm.setCurrentEdit (&mc.edit(), &sel);
        te::MackieMCU* mcuSurface = nullptr;
        for (auto* c : ecm.getControllers()) if (auto* m = c->getControlSurfaceIfType<te::MackieMCU>()) mcuSurface = m;
        check (mcuSurface != nullptr, "Mackie Control (MCU) driver is available");
        if (mcuSurface != nullptr)
        {
            auto* t1 = mc.getTracks()[0];
            t1->getVolumePlugin()->setVolumeDb (-30.0f);
            mcuSurface->acceptMidiMessage (0, juce::MidiMessage::pitchWheel (1, 16383));   // fader 1 to the top
            juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
            check (t1->getVolumePlugin()->getVolumeDb() > -1.0f, "MCU fader 1 moves track 1's volume (" + juce::String (t1->getVolumePlugin()->getVolumeDb(), 1) + " dB)");
        }
        ecm.detachFromEdit (&mc.edit());
    }
    {   // warp markers: moving a marker moves the audio in time
        DawSession wp (engine);
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_warp_click.wav");
        {
            f.deleteFile(); juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), 48000.0, 2, 24, {}, 0));
            juce::AudioBuffer<float> b (2, 48000 * 4); b.clear();
            for (int i = 0; i < 2400; ++i) { const float v = 0.8f * std::exp (-i / 300.0f) * (float) std::sin (2 * juce::MathConstants<double>::pi * 1000 * i / 48000.0); b.setSample (0, 48000 + i, v); b.setSample (1, 48000 + i, v); }
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        auto clip = wp.insertAudioFile (*wp.getTracks()[0], f, te::TimePosition());
        DawSession::setElastic (*clip, true);
        check (clip->getWarpTime() && clip->getWarpTimeManager().getMarkers().size() == 2, "elastic audio adds start / end warp markers");
        auto& wtm = clip->getWarpTimeManager();
        const auto len = wtm.getSourceLength();
        const int mid = wtm.insertMarker (te::WarpMarker (te::TimePosition::fromSeconds (1.0), te::TimePosition::fromSeconds (1.0)));
        wtm.moveMarker (mid, te::TimePosition::fromSeconds (1.5));        // the hit at 1.0 s should now play at 1.5 s
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1500);
        auto o = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_warp.wav");
        wp.exportWav (o, te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (3.0)));
        std::unique_ptr<juce::AudioFormatReader> r (engine.getAudioFileFormatManager().readFormatManager.createReaderFor (o));
        double peakAt = -1;
        if (r != nullptr)
        {
            juce::AudioBuffer<float> b (1, (int) r->lengthInSamples); r->read (&b, 0, b.getNumSamples(), 0, true, false);
            float best = 0; for (int i = 0; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > best) { best = std::abs (b.getSample (0, i)); peakAt = i / r->sampleRate; }
        }
        check (wtm.getMarkers().size() == 3 && std::abs (peakAt - 1.5) < 0.12, "dragging a warp marker moves the hit from 1.0 s to " + juce::String (peakAt, 2) + " s");
        o.deleteFile(); f.deleteFile();
    }
    {   // ---- folders, duplicate, tempo / signature changes, markers
        DawSession fs (engine);
        auto* a = fs.getTracks()[0];
        auto* f = fs.addFolder ("Drums");
        fs.moveToFolder (*a, f);
        check (DawSession::isFolder (*f) && fs.folderOf (*a) == f && fs.getOutputBus (*a) == f, "a track put in a folder is routed into it");
        auto order = fs.getTracks();
        check (order.indexOf (a) == order.indexOf (f) + 1, "folder members sit right under the folder");
        DawSession::setCollapsed (*f, true);
        check (fs.isHiddenByFolder (*a), "collapsing a folder hides its tracks");
        fs.moveTrack (*f, 0);
        order = fs.getTracks();
        { juce::String o2; for (auto* t : order) o2 << t->getName() << ","; check (order[0] == f && order[1] == a, "moving a folder moves its tracks with it (" + o2 + ")"); }
        fs.moveToFolder (*a, nullptr);
        check (fs.folderOf (*a) == nullptr && fs.getOutputBus (*a) == nullptr, "taking a track out of a folder routes it back to the master");
        te::AudioTrack* synthT = nullptr; for (auto* t : fs.getTracks()) if (DawSession::kindOf (*t) == DawSession::TrackKind::midi) synthT = t;
        auto* d = fs.duplicateTrack (*synthT);
        check (d != nullptr && d->getName().endsWith ("copy") && d->pluginList[0] != nullptr && d->pluginList[0]->isSynth(), "duplicate track copies the instrument ("
               + (d ? d->getName() + " / " + (d->pluginList[0] ? d->pluginList[0]->getName() : juce::String ("no plugin")) : juce::String ("null")) + ")");
        fs.setTempo (120.0);
        const auto bar9 = fs.edit().tempoSequence.toTime (te::BeatPosition::fromBeats (32.0));
        fs.addTempoChange (bar9, 140.0);
        auto& ts = fs.edit().tempoSequence;
        check (ts.getNumTempos() == 2 && std::abs (ts.getBpmAt (te::TimePosition::fromSeconds (4.0)) - 120.0) < 0.01
               && std::abs (ts.getBpmAt (bar9 + 4s) - 140.0) < 0.01, "tempo change at bar 9 (120 -> 140 BPM)");
        fs.addTimeSigChange (fs.edit().tempoSequence.toTime (te::BeatPosition::fromBeats (17.5)), 3, 4);
        check (ts.getNumTimeSigs() == 2 && ts.getTimeSig (1)->numerator.get() == 3 && std::abs (ts.getTimeSig (1)->getStartBeat().inBeats() - 20.0) < 0.01,
               "time signature change snaps to the next bar (beat " + juce::String (ts.getTimeSig (1)->getStartBeat().inBeats()) + ")");
        auto* mk = fs.addMarker (te::TimePosition::fromSeconds (8.0), "Chorus");
        check (mk != nullptr && mk->getName() == "Chorus" && fs.edit().getMarkerManager().getMarkers().size() == 1, "named marker added");
        auto pf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_folder_test.vegaproj");
        fs.saveProjectAs (pf);
        DawSession fs2 (engine);
        fs2.openProject (pf);
        int folders = 0; for (auto* t : fs2.getTracks()) if (DawSession::isFolder (*t)) ++folders;
        check (folders == 1 && fs2.edit().tempoSequence.getNumTempos() == 2 && fs2.edit().getMarkerManager().getMarkers().size() == 1, "folders, tempo changes and markers are saved");
        pf.deleteFile();
    }
    {   // ---- export formats, normalising, stems, bounce, consolidate
        DawSession ex (engine);
        ex.setTempo (120.0);
        auto tone = writeTone ("vega_tone.wav", 440.0, 4.0, 0.25f);
        auto* at = ex.getTracks()[0];
        ex.insertAudioFile (*at, tone, te::TimePosition());
        auto* bus = ex.addTrack (DawSession::TrackKind::bus, "Bus");
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory);
        DawSession::ExportOptions o; o.tailSeconds = 0.2;
        o.format = DawSession::ExportOptions::Format::flac;
        check (ex.exportMix (tmp.getChildFile ("vega_x.flac"), o) && peakOf (tmp.getChildFile ("vega_x.flac"), engine) > 0.1f, "export FLAC");
        o.format = DawSession::ExportOptions::Format::ogg;
        check (ex.exportMix (tmp.getChildFile ("vega_x.ogg"), o) && peakOf (tmp.getChildFile ("vega_x.ogg"), engine) > 0.1f, "export OGG Vorbis");
        o.format = DawSession::ExportOptions::Format::aiff;
        check (ex.exportMix (tmp.getChildFile ("vega_x.aif"), o) && peakOf (tmp.getChildFile ("vega_x.aif"), engine) > 0.1f, "export AIFF");
        o.format = DawSession::ExportOptions::Format::wav; o.normalise = DawSession::ExportOptions::Normalise::peak; o.peakDb = -0.3f;
        ex.exportMix (tmp.getChildFile ("vega_norm.wav"), o);
        const float pk = peakOf (tmp.getChildFile ("vega_norm.wav"), engine);
        check (std::abs (juce::Decibels::gainToDecibels (pk) + 0.3f) < 0.3f, "peak normalise to -0.3 dBFS (" + juce::String (juce::Decibels::gainToDecibels (pk), 2) + " dB)");
        o.normalise = DawSession::ExportOptions::Normalise::lufs; o.lufs = -14.0f;
        ex.exportMix (tmp.getChildFile ("vega_lufs.wav"), o);
        const float pl = peakOf (tmp.getChildFile ("vega_lufs.wav"), engine);
        check (pl > pk * 0.2f && juce::Decibels::gainToDecibels (pl) <= -0.9f, "loudness normalise (-14 LUFS, true peak under -1 dB: peak " + juce::String (juce::Decibels::gainToDecibels (pl), 2) + " dB)");
        if (DawSession::findLame().existsAsFile())
        {
            o.format = DawSession::ExportOptions::Format::mp3; o.normalise = DawSession::ExportOptions::Normalise::none;
            check (ex.exportMix (tmp.getChildFile ("vega_x.mp3"), o) && tmp.getChildFile ("vega_x.mp3").getSize() > 1000, "export MP3 through lame");
        }
        // stems: the audio track goes through the bus; the bus stem is just the bus
        ex.routeToBus (*at, bus);
        o = {}; o.tailSeconds = 0.2;
        auto stemDir = tmp.getChildFile ("vega_stems"); stemDir.deleteRecursively();
        auto stems = ex.exportStems (stemDir, o);
        check (stems.size() == 3, "one stem per track (" + juce::String (stems.size()) + " files)");
        if (stems.size() == 3)
        {
            const float sa = peakOf (stems[0], engine), ss = peakOf (stems[1], engine), sb = peakOf (stems[2], engine);
            check (sa > 0.1f && sb > 0.1f && ss < 0.001f, "stems: audio " + juce::String (sa, 3) + " (through its bus), empty synth " + juce::String (ss, 4) + ", bus " + juce::String (sb, 3));
        }
        stemDir.deleteRecursively();
        ex.routeToBus (*at, nullptr);
        // bounce in place
        const int before = ex.getTracks().size();
        auto* b = ex.bounceToNewTrack (*at, { te::TimePosition(), te::TimePosition::fromSeconds (2.0) });
        check (b != nullptr && ex.getTracks().size() == before + 1 && at->isMuted (false) && b->getClips().size() == 1
               && ex.getTracks().indexOf (b) == ex.getTracks().indexOf (at) + 1, "bounce in place: new track under the original, original muted");
        if (b != nullptr) if (auto* bc = dynamic_cast<te::WaveAudioClip*> (b->getClips()[0])) check (peakOf (bc->getOriginalFile(), engine) > 0.1f, "bounced audio isn't silent");
        // consolidate: split the audio into 3 clips, then join them again
        at->setMute (false);
        auto* c0 = at->getClips()[0];
        ex.splitClip (*c0, te::TimePosition::fromSeconds (1.0));
        ex.splitClip (*at->getClips()[1], te::TimePosition::fromSeconds (2.5));
        const int pieces = at->getClips().size();
        auto* joined = ex.consolidate (*at, { te::TimePosition::fromSeconds (0.5), te::TimePosition::fromSeconds (3.0) });
        check (pieces == 3 && joined != nullptr && at->getClips().size() == 1 && std::abs (joined->getPosition().getLength().inSeconds() - 4.0) < 0.01,
               "consolidate joins 3 audio clips into one (" + juce::String (at->getClips().size()) + " left)");
        te::AudioTrack* syn = nullptr; for (auto* t : ex.getTracks()) if (DawSession::kindOf (*t) == DawSession::TrackKind::midi) syn = t;
        auto m1 = ex.insertMidiClip (*syn, te::TimePosition(), te::TimeDuration::fromSeconds (2.0));
        auto m2 = ex.insertMidiClip (*syn, te::TimePosition::fromSeconds (2.0), te::TimeDuration::fromSeconds (2.0));
        m1->getSequence().addNote (60, te::BeatPosition(), te::BeatDuration::fromBeats (1), 100, 0, nullptr);
        m2->getSequence().addNote (64, te::BeatPosition(), te::BeatDuration::fromBeats (1), 100, 0, nullptr);
        auto* mj = dynamic_cast<te::MidiClip*> (ex.consolidate (*syn, { te::TimePosition(), te::TimePosition::fromSeconds (4.0) }));
        check (mj != nullptr && syn->getClips().size() == 1 && mj->getSequence().getNumNotes() == 2
               && std::abs (mj->getSequence().getNotes()[1]->getStartBeat().inBeats() - 4.0) < 0.01, "consolidate merges MIDI clips, notes keep their place ("
               + juce::String (syn->getClips().size()) + " clips, " + (mj ? juce::String (mj->getSequence().getNumNotes()) + " notes, 2nd at " + juce::String (mj->getSequence().getNumNotes() > 1 ? mj->getSequence().getNotes()[1]->getStartBeat().inBeats() : -1.0) : juce::String ("null")) + ")");
        // controller lane data survives
        if (mj != nullptr)
        {
            mj->getSequence().addControllerEvent (te::BeatPosition::fromBeats (1.0), 1, 100 << 7, nullptr);
            mj->getSequence().addControllerEvent (te::BeatPosition::fromBeats (2.0), te::MidiControllerEvent::pitchWheelType, 12000, nullptr);
            check (mj->getSequence().getNumControllerEvents() == 2, "mod wheel + pitch bend events in a MIDI clip");
        }
        for (auto n : { "vega_x.flac", "vega_x.ogg", "vega_x.aif", "vega_norm.wav", "vega_lufs.wav", "vega_x.mp3" }) tmp.getChildFile (n).deleteFile();
        tone.deleteFile();
    }
    {   // ---- takes and comping
        DawSession cs (engine);
        auto t1 = writeTone ("vega_take1.wav", 300.0, 4.0, 0.3f), t2 = writeTone ("vega_take2.wav", 1200.0, 4.0, 0.3f);
        auto* at = cs.getTracks()[0];
        auto clip = cs.insertAudioFile (*at, t1, te::TimePosition());
        DawSession::addTakes (*clip, { t1, t2 });
        check (DawSession::takeFiles (*clip).size() == 2 && DawSession::currentTake (*clip) == 0, "clip has 2 takes, take 1 playing");
        DawSession::setTake (*clip, 1);
        check (DawSession::currentTake (*clip) == 1 && clip->getOriginalFile() == t2 && DawSession::takeFiles (*clip).size() == 2, "switch to take 2 (takes are kept)");
        DawSession::setTake (*clip, 0);
        auto* piece = cs.compTake (*clip, 1, { te::TimePosition::fromSeconds (1.0), te::TimePosition::fromSeconds (2.0) });
        check (piece != nullptr && at->getClips().size() == 3 && DawSession::currentTake (*piece) == 1, "comp take 2 into 1 s - 2 s (clip split into 3)");
        auto o = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_comp.wav");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
        cs.exportWav (o, te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (3.0)));
        {
            std::unique_ptr<juce::AudioFormatReader> r (engine.getAudioFileFormatManager().readFormatManager.createReaderFor (o));
            auto bandDb = [&r] (double startSec, double hz)
            {
                const int n = 24000; juce::AudioBuffer<float> b (1, n); r->read (&b, 0, n, (juce::int64) (startSec * r->sampleRate), true, false);
                double re = 0, im = 0;
                for (int i = 0; i < n; ++i) { const double ph = 2 * juce::MathConstants<double>::pi * hz * i / r->sampleRate; re += b.getSample (0, i) * std::cos (ph); im += b.getSample (0, i) * std::sin (ph); }
                return juce::Decibels::gainToDecibels (std::sqrt (re * re + im * im) * 2.0 / n, -200.0);
            };
            if (r != nullptr)
                check (bandDb (0.25, 300) > bandDb (0.25, 1200) + 20 && bandDb (1.25, 1200) > bandDb (1.25, 300) + 20 && bandDb (2.25, 300) > bandDb (2.25, 1200) + 20,
                       "the comp plays take 1, then take 2 from 1-2 s, then take 1 again");
        }
        auto made = cs.unpackTakes (*piece);
        check (made.size() == 2, "takes to new tracks");
        DawSession::keepCurrentTakeOnly (*piece);
        check (DawSession::takeFiles (*piece).isEmpty() && piece->getOriginalFile() == t2, "keep the current take only");
        o.deleteFile(); t1.deleteFile(); t2.deleteFile();
    }
    {   // ---- clip launcher
        DawSession ls (engine);
        ls.ensureScenes (8);
        auto* syn = ls.getTracks (false)[1];
        check (ls.numScenes() == 8 && syn->getClipSlotList().getClipSlots().size() == 8, "8 scenes, every track has 8 slots");
        auto* sc = ls.createSlotMidiClip (*syn, 2, 2);
        check (sc != nullptr && DawSession::slotClip (*syn, 2) == sc && sc->isLooping(), "new looping MIDI clip in slot 3");
        sc->getSequence().addNote (60, te::BeatPosition(), te::BeatDuration::fromBeats (1), 100, 0, nullptr);
        auto arr = ls.insertMidiClip (*syn, te::TimePosition(), te::TimeDuration::fromSeconds (2.0));
        auto* copied = ls.copyToSlot (*arr, *syn, 0);
        check (copied != nullptr && DawSession::slotClip (*syn, 0) != nullptr && syn->getClips().size() == 1, "arrangement clip copied into a slot (the original stays)");
        auto* back = ls.copyToArrangement (*sc, te::TimePosition::fromSeconds (8.0));
        check (back != nullptr && syn->getClips().size() == 2 && std::abs (back->getPosition().getStart().inSeconds() - 8.0) < 1e-6, "slot clip copied to the arrangement at 8 s");
        auto tone = writeTone ("vega_slot.wav", 220.0, 2.0);
        check (ls.insertSlotFile (*ls.getTracks()[0], 1, tone) != nullptr, "audio file into an audio track's slot");
        ls.insertScene (0);
        check (ls.numScenes() == 9 && DawSession::slotClip (*syn, 3) == sc, "inserting a scene shifts the clips down");
        ls.deleteScene (0);
        check (ls.numScenes() == 8 && DawSession::slotClip (*syn, 2) == sc, "deleting a scene");
        if (auto* fa = sc->getFollowActions()) { auto& a = fa->addAction(); a.action = te::FollowAction::trackNext; a.weight = 1.0; }
        auto stepC = te::insertNewClip (*DawSession::slot (*syn, 4), te::TrackItem::Type::step, "Steps", te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.0)));
        check (dynamic_cast<te::StepClip*> (stepC) != nullptr, "step-sequencer clip in a slot");
        auto pf = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_launcher.vegaproj");
        ls.saveProjectAs (pf);
        DawSession ls2 (engine);
        ls2.openProject (pf);
        auto* syn2 = ls2.getTracks (false)[1];
        auto* sc2 = syn2 != nullptr ? dynamic_cast<te::MidiClip*> (DawSession::slotClip (*syn2, 2)) : nullptr;
        check (ls2.numScenes() == 8 && sc2 != nullptr && sc2->getSequence().getNumNotes() == 1 && sc2->getFollowActions() != nullptr && sc2->getFollowActions()->getActions().size() == 1,
               "launcher clips, notes and follow actions are saved with the project");
        ls.launchClip (*sc);       // no audio device here: just mustn't crash
        ls.backToArrangement();
        check (! syn->playSlotClips.get(), "back to arrangement");
        pf.deleteFile(); tone.deleteFile();
    }
    {   // ---- step sequencer clip in the arrangement renders the drum pattern
        DawSession st (engine);
        auto* syn = st.getTracks (false)[1];
        auto* clip = dynamic_cast<te::StepClip*> (syn->insertNewClip (te::TrackItem::Type::step, "Steps", { te::TimePosition(), te::TimePosition::fromSeconds (2.0) }, nullptr));
        check (clip != nullptr && clip->getChannels().size() == 8 && clip->getChannels()[0]->noteNumber.get() == 36, "step clip: 8 GM drum rows, kick first");
        if (clip != nullptr)
        {
            auto pat = clip->getPattern (0);
            for (int i = 0; i < 16; i += 4) pat.setNote (0, i, true);
            auto o = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_steps.wav");
            st.exportWav (o, te::TimeRange (te::TimePosition(), te::TimePosition::fromSeconds (2.0)));
            check (peakOf (o, engine) > 0.01f, "step clip plays the instrument (peak " + juce::String (peakOf (o, engine), 3) + ")");
            o.deleteFile();
        }
    }
    {   // ---- Akai APC Key 25 mk2 script
        DawSession as (engine);
        as.ensureScenes (8);
        ApcSurface apc (as);
        std::vector<juce::MidiMessage> sent;
        apc.send = [&sent] (const juce::MidiMessage& m) { sent.push_back (m); };
        auto* syn = as.getTracks (false)[1];
        as.createSlotMidiClip (*syn, 0, 1);
        apc.updateFeedback (true);
        bool padLit = false;
        for (auto& m : sent) if (m.isNoteOn() && m.getNoteNumber() == 33 && m.getChannel() == 3 && m.getVelocity() > 0) padLit = true;   // top row, column 2
        check (padLit, "APC: a clip in scene 1 / track 2 lights the top-row pad (dim track colour)");
        check (ApcSurface::nearestPaletteColour (juce::Colours::red) == 5 && ApcSurface::nearestPaletteColour (juce::Colour (0xff00ff00)) == 21
               && ApcSurface::nearestPaletteColour (juce::Colours::white) == 3, "APC: track colours map to the pad palette");
        sent.clear(); apc.updateFeedback();
        check (sent.empty(), "APC: feedback only sends changes");
        auto note = [&apc] (int n, bool down = true) { apc.handleIncoming (down ? juce::MidiMessage::noteOn (1, n, (juce::uint8) 127) : juce::MidiMessage::noteOff (1, n)); };
        note (0x62); note (0x54); note (0x62, false);                       // shift + MUTE soft key
        check (apc.softMode == ApcSurface::SoftMode::mute, "APC: Shift + scene 3 = mute mode");
        note (0x41);
        check (syn->isMuted (false), "APC: track button 2 mutes track 2");
        note (0x41);
        const float v0 = syn->getVolumePlugin()->getVolumeDb();
        apc.handleIncoming (juce::MidiMessage::controllerEvent (1, 0x31, 10));
        const float v1 = syn->getVolumePlugin()->getVolumeDb();
        apc.handleIncoming (juce::MidiMessage::controllerEvent (1, 0x31, 128 - 20));
        const float v2 = syn->getVolumePlugin()->getVolumeDb();
        check (v1 > v0 && v2 < v1, "APC: knob 2 (relative) turns track 2's volume up and down (" + juce::String (v0, 1) + " > " + juce::String (v1, 1) + " > " + juce::String (v2, 1) + " dB)");
        note (0x62); note (0x45); note (0x62, false);                       // shift + PAN
        apc.handleIncoming (juce::MidiMessage::controllerEvent (1, 0x31, 20));
        check (apc.knobMode == ApcSurface::KnobMode::pan && syn->getVolumePlugin()->getPan() > 0.05f, "APC: Shift + Pan, then a knob pans");
        note (0x62); note (0x41); note (0x62, false);                       // shift + down arrow
        check (apc.sceneOffset == 1, "APC: Shift + down moves the box a scene down");
        note (0x62); note (0x40); note (0x62, false);
        note (0x21);                                                       // pad with the clip: launch (no audio device - mustn't crash)
        note (0x51); note (0x62); note (0x51); note (0x62, false);
        check (! syn->playSlotClips.get(), "APC: Shift + Stop All Clips = back to arrangement");
        {
            int recScene = -1; te::AudioTrack* recTrack = nullptr; bool stopped = false;
            apc.onRecordSlot = [&] (te::AudioTrack& t, int sc) { recTrack = &t; recScene = sc; return true; };
            apc.isRecordingSlot = [&] (te::AudioTrack& t, int sc) { return &t == recTrack && sc == recScene && ! stopped; };
            apc.onStopSlotRecording = [&] { stopped = true; };
            note (25); note (25, false);                                    // row 2, column 2: an empty slot on the synth
            check (recTrack == syn && recScene == 1, "APC: a pad on an empty instrument slot records a new clip there");
            sent.clear(); apc.updateFeedback();
            bool redPad = false; for (auto& m : sent) if (m.isNoteOn() && m.getNoteNumber() == 25 && m.getChannel() == 14 && m.getVelocity() == 5) redPad = true;
            check (redPad, "APC: the recording slot's pad flashes red");
            note (25); note (25, false);
            check (stopped, "APC: pressing it again stops the recording");
            apc.onRecordSlot = nullptr; apc.isRecordingSlot = nullptr; apc.onStopSlotRecording = nullptr;
        }
        note (0x62); note (0x5d); note (0x62, false);                       // shift + record = drum pads
        sent.clear();
        note (0); apc.updateFeedback(); note (0, false);
        check (apc.drumPads && ! sent.empty() && ! as.edit().getTransport().isRecording(), "APC: Shift + Record turns the pads into drum pads (no recording started)");
    }
    {   // ---- phone sketch import (Vega Sketch for Android)
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_sketch_src");
        tmp.deleteRecursively(); tmp.getChildFile ("audio").createDirectory(); tmp.getChildFile ("renders").createDirectory();
        writeTone ("vega_vox.wav", 330.0, 60.0 / 124.0 * 8.0).moveFileTo (tmp.getChildFile ("audio/vox.wav"));
        writeTone ("vega_r0.wav", 60.0, 60.0 / 124.0 * 4.0).moveFileTo (tmp.getChildFile ("renders/t0s0.wav"));
        writeTone ("vega_r1.wav", 220.0, 60.0 / 124.0 * 8.0).moveFileTo (tmp.getChildFile ("renders/t1s0.wav"));
        // the drum track's sample kit rides along (the bundled 808: kick and clap)
        const auto kitSrc = juce::File (__FILE__).getParentDirectory().getSiblingFile ("android/app/src/main/assets/kits/vintage808");
        tmp.getChildFile ("kits/vintage808").createDirectory();
        for (auto n : { "36.wav", "39.wav" }) kitSrc.getChildFile (n).copyFileTo (tmp.getChildFile ("kits/vintage808").getChildFile (n));
        tmp.getChildFile ("sketch.json").replaceWithText (R"({
          "format": "vega-sketch", "version": 1, "name": "Sunday Groove", "bpm": 124.0, "beatsPerBar": 4,
          "scenes": ["Intro", "Drop", "Scene 3", "Scene 4", "Scene 5", "Scene 6", "Scene 7", "Scene 8"],
          "tracks": [
            { "name": "Drums", "type": "drums", "sound": 0, "volume": 0.85, "pan": 0, "colour": "#FFA25E", "kitId": "vintage808",
              "kit": { "id": "vintage808", "name": "Vintage 808", "pads": [ { "note": 36, "name": "Kick", "file": "kits/vintage808/36.wav" }, { "note": 39, "name": "Clap", "file": "kits/vintage808/39.wav" } ] },
              "clips": [ { "scene": 0, "name": "Beat 1", "bars": 1, "lengthBeats": 4.0, "notes": [[36,0,0.25,110],[39,1,0.25,100],[36,2,0.25,110],[39,3,0.25,100]], "render": "renders/t0s0.wav" },
                         { "scene": 1, "name": "Beat 2", "bars": 1, "lengthBeats": 4.0, "notes": [[36,0,0.25,110],[42,0.5,0.25,90]] } ] },
            { "name": "Keys", "type": "keys", "sound": 0, "volume": 0.7, "pan": -0.2, "colour": "#6FB7FF",
              "clips": [ { "scene": 0, "name": "Chords", "bars": 2, "lengthBeats": 8.0, "notes": [[57,0,3.5,96],[60,0,3.5,96],[64,0,3.5,96],[55,4,3.5,96]], "render": "renders/t1s0.wav" } ] },
            { "name": "Vocals", "type": "audio", "sound": 0, "volume": 0.9, "pan": 0, "colour": "#E8708A",
              "clips": [ { "scene": 1, "name": "Take 1", "bars": 2, "lengthBeats": 8.0, "notes": [], "audio": "audio/vox.wav" } ] } ] })");
        auto zipFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Sunday Groove.vegasketch");
        {
            juce::ZipFile::Builder b;
            for (auto& f : tmp.findChildFiles (juce::File::findFiles, true)) b.addFile (f, 9, f.getRelativePathFrom (tmp).replaceCharacter ('\\', '/'));
            zipFile.deleteFile(); juce::FileOutputStream os (zipFile); b.writeToStream (os, nullptr);
        }
        DawSession ps (engine);
        for (auto* t : ps.getTracks()) ps.deleteTrack (*t);
        DawSession::SketchImport opt; opt.sounds = DawSession::SketchImport::Sounds::both;
        auto r = ps.importSketch (zipFile, opt);
        check (r.ok && r.name == "Sunday Groove", "import phone sketch: " + r.message);
        check (std::abs (ps.edit().tempoSequence.getTempo (0)->getBpm() - 124.0) < 0.01, "the sketch's tempo (124 BPM)");
        auto tracks = ps.getTracks();
        // both: drums -> (phone) audio + MIDI, keys -> (phone) audio + MIDI, vocals -> audio  = 5 tracks
        check (tracks.size() == 5, "tracks for both the phone audio and the desktop instruments (" + juce::String (tracks.size()) + ")");
        te::AudioTrack* drumsMidi = nullptr; te::AudioTrack* vox = nullptr; te::AudioTrack* drumsPrint = nullptr;
        for (auto* t : tracks) { if (t->getName() == "Drums" && DawSession::kindOf (*t) == DawSession::TrackKind::midi) drumsMidi = t; if (t->getName() == "Vocals") vox = t; if (t->getName() == "Drums (phone)") drumsPrint = t; }
        auto* beat = drumsMidi != nullptr ? dynamic_cast<te::MidiClip*> (DawSession::slotClip (*drumsMidi, 0)) : nullptr;
        check (beat != nullptr && beat->getSequence().getNumNotes() == 4 && beat->getName() == "Beat 1", "drum clip in scene 1 with its 4 hits");
        check (vox != nullptr && dynamic_cast<te::WaveAudioClip*> (DawSession::slotClip (*vox, 1)) != nullptr, "the vocal take in scene 2");
        check (drumsPrint != nullptr && drumsPrint->isMuted (false), "phone audio of the drums imported, muted (desktop instruments play)");
        {
            te::SamplerPlugin* sp = drumsMidi != nullptr ? dynamic_cast<te::SamplerPlugin*> (drumsMidi->pluginList[0]) : nullptr;
            check (sp != nullptr && sp->getNumSounds() == 2 && sp->getKeyNote (0) == 36 && sp->getMinKey (1) == 39 && sp->getMaxKey (1) == 39 && sp->isSoundOpenEnded (0),
                   "the phone's 808 kit is loaded into a Sampler, one pad per note (" + juce::String (sp != nullptr ? sp->getNumSounds() : -1) + " sounds)");
            check (r.kits == 1, "the import reports the kit");
        }
        auto* sc0 = ps.edit().getSceneList().getScenes()[0];
        check (sc0 != nullptr && sc0->name.get() == "Intro", "scene names come across");
        // arrangement: Intro (2 bars, the longest clip) then Drop (2 bars, the vocal) with markers
        check (ps.edit().getMarkerManager().getMarkers().size() == 2, "a marker per scene in the arrangement");
        if (drumsMidi != nullptr)
        {
            auto arr = drumsMidi->getClips();
            check (arr.size() == 2 && std::abs (ps.edit().tempoSequence.toBeats (arr[1]->getPosition().getStart()).inBeats() - 8.0) < 0.01
                   && std::abs (ps.edit().tempoSequence.toBeats (arr[0]->getPosition().getEnd()).inBeats() - 8.0) < 0.01,
                   "arrangement: the 1-bar beat loops to fill the 2-bar Intro, the Drop starts at bar 3");
        }
        auto o = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_sketch_mix.wav");
        ps.exportWav (o);
        check (peakOf (o, engine) > 0.05f, "the imported song plays (mixdown isn't silent)");
        o.deleteFile(); zipFile.deleteFile(); tmp.deleteRecursively();
    }
    out.deleteFile(); proj.deleteFile();
    std::cout << (fails == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string (fails)) << "\n";
    return fails == 0 ? 0 : 1;
}
