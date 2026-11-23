#include "HuiSurface.h"

HuiSurface::HuiSurface (DawSession& session) : s (session) {}

te::AudioTrack* HuiSurface::trackForStrip (int strip) const
{
    auto tracks = s.getTracks (true);
    const int i = bankStart + strip;
    return i >= 0 && i < tracks.size() ? tracks[i] : nullptr;
}

te::AuxSendPlugin* HuiSurface::sendFor (te::AudioTrack& t, int idx) const
{
    auto buses = s.getBuses();
    if (idx < 0 || idx >= buses.size() || buses[idx] == &t) return nullptr;
    return s.findSend (t, *buses[idx]);
}

void HuiSurface::moveBank (int delta)
{
    const int n = s.getTracks (true).size();
    bankStart = juce::jlimit (0, std::max (0, n - 1), bankStart + delta);
    updateFeedback (true);
}

void HuiSurface::handleIncoming (const juce::MidiMessage& m)
{
    if (m.isNoteOn() && m.getNoteNumber() == 0) return;                // ping reply
    if (! m.isController()) return;
    const int cc = m.getControllerNumber(), v = m.getControllerValue();

    if (cc == 0x0f) { zone = v; return; }                              // switch: zone select
    if (cc == 0x2f) { if (zone >= 0) pressed (zone, v & 0x0f, (v & 0x40) != 0); return; }

    if (cc <= 0x07) { faderMsb[(size_t) cc] = v; return; }             // fader MSB, wait for LSB
    if (cc >= 0x20 && cc <= 0x27)
    {
        const int strip = cc - 0x20;
        const int value = (faderMsb[(size_t) strip] << 7) | v;
        if (auto* t = trackForStrip (strip))
            if (auto vp = t->getVolumePlugin())
            {
                vp->setVolumeDb (dbFromFaderPos (value / 16383.0f));
                lastTouch[strip] = juce::Time::getMillisecondCounter();
            }
        return;
    }
    auto delta = [v] { return v > 0x40 ? v - 0x40 : -v; };
    if (cc >= 0x40 && cc <= 0x47)                                      // V-pots
    {
        const int d = delta();
        if (encMode == EncMode::plugin)
        {
            const int knob = cc - 0x40;
            if (auto* p = macroParam ? macroParam (knob) : nullptr)
            {
                touch (p);
                p->setNormalisedParameter (juce::jlimit (0.0f, 1.0f, p->getCurrentNormalisedValue() + d * 0.008f), juce::sendNotification);
            }
            if (onKnobTouched) onKnobTouched (knob);
            return;
        }
        auto* t = trackForStrip (cc - 0x40); if (t == nullptr) return;
        if (onKnobTouched) onKnobTouched (cc - 0x40);
        if (encMode == EncMode::pan)
        {
            if (auto vp = t->getVolumePlugin()) vp->setPan (juce::jlimit (-1.0f, 1.0f, vp->getPan() + d * 0.02f));
        }
        else
        {
            const int idx = (int) encMode - (int) EncMode::sendA;
            auto* snd = sendFor (*t, idx);
            if (snd == nullptr)
            {   // turning up a send that doesn't exist yet creates it
                auto buses = s.getBuses();
                if (d > 0 && idx < buses.size() && buses[idx] != t) { s.setSend (*t, *buses[idx], -30.0f); snd = sendFor (*t, idx); }
            }
            if (snd != nullptr) snd->setGainDb (juce::jlimit (-60.0f, 6.0f, snd->getGainDb() + d * 0.5f));
        }
        return;
    }
    if (cc == 0x0d)                                                    // jog wheel: move the playhead by beats
    {
        auto& tr = s.edit().getTransport(); auto& ts = s.edit().tempoSequence;
        const double b = std::max (0.0, ts.toBeats (tr.getPosition()).inBeats() + delta() * 0.25);
        tr.setPosition (ts.toTime (te::BeatPosition::fromBeats (b)));
    }
}

void HuiSurface::pressed (int z, int port, bool down)
{
    auto& tr = s.edit().getTransport();
    if (z <= 0x07)                                                     // channel strips
    {
        auto* t = trackForStrip (z);
        if (! down || t == nullptr) return;
        switch (port)
        {
            case 1: if (onSelectTrack) onSelectTrack (t); break;
            case 2: t->setMute (! t->isMuted (false)); break;
            case 3: t->setSolo (! t->isSolo (false)); break;
            case 7:
            {
                bool armed = false;
                for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0) && in->isRecordingEnabled (t->itemID)) armed = true;
                if (! armed)
                {
                    bool has = false; for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0)) has = true;
                    const bool midi = DawSession::kindOf (*t) == DawSession::TrackKind::midi;
                    if (! has)
                        for (auto* in : t->edit.getAllInputDevices())
                            if (in->getInputDevice().getDeviceType() == (midi ? te::InputDevice::physicalMidiDevice : te::InputDevice::waveDevice))
                            { [[maybe_unused]] auto r = in->setTarget (t->itemID, ! midi, nullptr, 0); if (! midi) break; }
                }
                for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0)) in->setRecordingEnabled (t->itemID, ! armed);
                break;
            }
            default: break;
        }
        updateFeedback();
        return;
    }
    if (! down) return;
    auto& ts = s.edit().tempoSequence;
    auto moveBars = [&] (double bars)
    {
        const int num = ts.getTimeSig (0)->numerator;
        const double b = std::max (0.0, std::floor (ts.toBeats (tr.getPosition()).inBeats() / num + bars + 1e-6) * num);
        tr.setPosition (ts.toTime (te::BeatPosition::fromBeats (b)));
    };
    switch (z)
    {
        case 0x0a: moveBank (port == 0 ? -1 : port == 1 ? -8 : port == 2 ? 1 : port == 3 ? 8 : 0); break;
        case 0x0b:
            if (port == 2) encMode = EncMode::pan;
            else if (port >= 3 && port <= 7) encMode = (EncMode) ((int) EncMode::sendE - (port - 3));   // ports 3..7 = send E..A
            updateFeedback (true); break;
        case 0x0e:
            if (port == 1) moveBars (-1);
            else if (port == 2) moveBars (1);
            else if (port == 3) { if (tr.isPlaying()) tr.stop (false, false); else tr.setPosition (0s); }
            else if (port == 4) { if (tr.isPlaying()) tr.stop (false, false); else tr.play (false); }
            else if (port == 5) { if (tr.isRecording()) tr.stop (true, false); else tr.record (false); }
            break;
        case 0x0f:
            if (port == 0) tr.setPosition (0s);
            else if (port == 1) tr.setPosition (s.contentRange().getEnd());
            else if (port == 3) { tr.looping = ! tr.looping.get(); if (tr.looping && tr.getLoopRange().getLength() <= 0s) tr.setLoopRange ({ 0s, s.barLength() * 4 }); }
            break;
        case 0x0d:   // cursor: up/down = select previous/next track, left/right = playhead by a beat
        {
            if (port == 0 || port == 4)
            {
                auto tracks = s.getTracks (true);
                if (tracks.isEmpty()) break;
                int sel = 0;
                for (int i = 0; i < tracks.size(); ++i) if (tracks[i]->itemID == selectedID) sel = i;
                sel = juce::jlimit (0, tracks.size() - 1, sel + (port == 0 ? 1 : -1));
                selectedID = tracks[sel]->itemID;
                if (onSelectTrack) onSelectTrack (tracks[sel]);
            }
            else if (port == 1 || port == 3)
            {
                const double b = std::max (0.0, ts.toBeats (tr.getPosition()).inBeats() + (port == 3 ? 1.0 : -1.0));
                tr.setPosition (ts.toTime (te::BeatPosition::fromBeats (std::round (b))));
            }
            break;
        }
        case 0x08:
            if (port == 3) s.edit().getUndoManager().undo();
            else if (port == 6 && onSave) onSave();
            break;
        case 0x1d: if (port == 2) s.edit().clickTrackEnabled = ! s.edit().clickTrackEnabled.get(); break;
        default: break;
    }
    updateFeedback();
}

void HuiSurface::touch (te::AutomatableParameter* p)
{
    const auto now = juce::Time::getMillisecondCounter();
    for (auto& g : gestures) if (g.param.get() == p) { g.last = now; return; }
    p->parameterChangeGestureBegin();
    gestures.push_back ({ p, now });
}

void HuiSurface::endGestures (bool all)
{
    const auto now = juce::Time::getMillisecondCounter();
    for (auto it = gestures.begin(); it != gestures.end();)
        if (all || now - it->last > 400) { it->param->parameterChangeGestureEnd(); it = gestures.erase (it); }
        else ++it;
}

void HuiSurface::switchLed (int z, int port, bool on)
{
    const int key = z * 8 + port;
    auto it = last.leds.find (key);
    if (it != last.leds.end() && it->second == on) return;
    last.leds[key] = on;
    if (! send) return;
    send (juce::MidiMessage::controllerEvent (1, 0x0c, z));
    send (juce::MidiMessage::controllerEvent (1, 0x2c, (on ? 0x40 : 0x00) | port));
}

void HuiSurface::updateFeedback (bool force)
{
    endGestures();
    if (! send) return;
    if (force) last = {};
    auto& tr = s.edit().getTransport();
    const auto now = juce::Time::getMillisecondCounter();
    for (int strip = 0; strip < 8; ++strip)
    {
        auto* t = trackForStrip (strip);
        // fader position (skip while the user is moving it, so motor faders don't fight)
        int pos = 0;
        if (t != nullptr) if (auto vp = t->getVolumePlugin()) pos = juce::jlimit (0, 16383, juce::roundToInt (faderPosFromDb (vp->getVolumeDb()) * 16383.0f));
        if (pos != last.fader[(size_t) strip] && now - lastTouch[strip] > 300)
        {
            last.fader[(size_t) strip] = pos;
            send (juce::MidiMessage::controllerEvent (1, strip, pos >> 7));
            send (juce::MidiMessage::controllerEvent (1, 0x20 + strip, pos & 0x7f));
        }
        // encoder ring: pan (centre-spread) or send level (fill from left)
        int ring = 0;
        if (encMode == EncMode::plugin)
        {
            if (auto* p = macroParam ? macroParam (strip) : nullptr) ring = 0x20 + juce::jlimit (0, 11, juce::roundToInt (p->getCurrentNormalisedValue() * 11.0f));
        }
        else if (t != nullptr)
        {
            if (encMode == EncMode::pan) { if (auto vp = t->getVolumePlugin()) ring = 1 + juce::jlimit (0, 10, juce::roundToInt ((vp->getPan() + 1.0f) * 5.0f)); }
            else if (auto* snd = sendFor (*t, (int) encMode - (int) EncMode::sendA)) ring = 0x20 + juce::jlimit (0, 11, juce::roundToInt ((snd->getGainDb() + 60.0f) / 66.0f * 11.0f));
        }
        if (ring != last.ring[(size_t) strip]) { last.ring[(size_t) strip] = ring; send (juce::MidiMessage::controllerEvent (1, 0x10 + strip, ring)); }
        // LEDs
        bool armed = false;
        if (t != nullptr) for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0) && in->isRecordingEnabled (t->itemID)) armed = true;
        switchLed (strip, 1, t != nullptr && t->itemID == selectedID);
        switchLed (strip, 2, t != nullptr && t->isMuted (false));
        switchLed (strip, 3, t != nullptr && t->isSolo (false));
        switchLed (strip, 7, armed);
        // 4-character scribble strip (HUI small display SysEx)
        juce::String name = t != nullptr ? t->getName().removeCharacters (" ").substring (0, 4) : juce::String();
        name = name.paddedRight (' ', 4);
        if (name != last.names[(size_t) strip])
        {
            last.names[(size_t) strip] = name;
            juce::uint8 sx[] { 0x00, 0x00, 0x66, 0x05, 0x00, 0x10, (juce::uint8) strip, 0, 0, 0, 0 };
            for (int i = 0; i < 4; ++i) sx[7 + i] = (juce::uint8) (name[i] & 0x7f);
            send (juce::MidiMessage::createSysExMessage (sx, (int) sizeof (sx)));
        }
    }
    switchLed (0x0e, 3, ! tr.isPlaying());
    switchLed (0x0e, 4, tr.isPlaying());
    switchLed (0x0e, 5, tr.isRecording());
    switchLed (0x0f, 3, tr.looping.get());
    switchLed (0x1d, 2, s.edit().clickTrackEnabled.get());
    switchLed (0x0b, 2, encMode == EncMode::pan);
    for (int p = 3; p <= 7; ++p) switchLed (0x0b, p, (int) encMode == (int) EncMode::sendE - (p - 3));
}

juce::String HuiSurface::describe (const juce::MidiMessage& m) const
{
    if (m.isNoteOn() && m.getNoteNumber() == 0) return "ping reply";
    if (! m.isController()) return m.getDescription();
    const int cc = m.getControllerNumber(), v = m.getControllerValue();
    if (cc == 0x0f) return "zone " + juce::String::toHexString (v);
    if (cc == 0x2f)
    {
        static const char* strip[] { "fader touch", "select", "mute", "solo", "auto", "v-sel", "insert", "rec arm" };
        juce::String what;
        if (zone >= 0 && zone <= 7) what = juce::String ("strip ") + juce::String (zone + 1) + " " + strip[v & 7];
        else if (zone == 0x0e) { static const char* t[] { "talkback", "rewind", "fast fwd", "stop", "play", "record", "?", "?" }; what = t[v & 7]; }
        else if (zone == 0x0a) { static const char* b[] { "channel <", "bank <", "channel >", "bank >", "?", "?", "?", "?" }; what = b[v & 7]; }
        else if (zone == 0x0b) { static const char* a[] { "output", "input", "pan", "send E", "send D", "send C", "send B", "send A" }; what = a[v & 7]; }
        else what = "zone " + juce::String::toHexString (zone) + " port " + juce::String (v & 0x0f);
        return what + ((v & 0x40) ? " press" : " release");
    }
    if (cc <= 0x07) return "fader " + juce::String (cc + 1) + " (coarse)";
    if (cc >= 0x20 && cc <= 0x27) return "fader " + juce::String (cc - 0x1f) + " = " + juce::String (((faderMsb[(size_t) (cc - 0x20)] << 7) | v) * 100 / 16383) + "%";
    if (cc >= 0x40 && cc <= 0x47) return "encoder " + juce::String (cc - 0x3f) + " " + (v > 0x40 ? "+" + juce::String (v - 0x40) : "-" + juce::String (v));
    if (cc == 0x0d) return "jog " + juce::String (v > 0x40 ? v - 0x40 : -v);
    return "CC " + juce::String (cc) + " = " + juce::String (v);
}
