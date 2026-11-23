#include "ApcSurface.h"

ApcSurface::ApcSurface (DawSession& session) : s (session) { lastPad.fill (-1); }

juce::MidiMessage ApcSurface::introduction()
{
    const juce::uint8 d[] { 0x47, 0x7f, 0x4e, 0x60, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00 };
    return juce::MidiMessage::createSysExMessage (d, (int) sizeof (d));
}

juce::Array<te::AudioTrack*> ApcSurface::columns() const
{
    juce::Array<te::AudioTrack*> r;
    for (auto* t : s.getTracks (false)) if (! s.isHiddenByFolder (*t)) r.add (t);
    return r;
}

te::AudioTrack* ApcSurface::trackForColumn (int col) const
{
    auto c = columns();
    const int i = trackOffset + col;
    return i >= 0 && i < c.size() ? c[i] : nullptr;
}

int ApcSurface::nearestPaletteColour (juce::Colour c)
{
    // the pads' colour palette (velocity -> colour), the bright hues only
    static const std::pair<int, juce::uint32> pal[] {
        { 3, 0xffffff }, { 5, 0xff4c4c }, { 9, 0xff5400 }, { 13, 0xffff4c }, { 17, 0x88ff4c }, { 21, 0x4cff4c }, { 29, 0x4cff8b },
        { 33, 0x4cffb7 }, { 37, 0x4cc3ff }, { 41, 0x4c88ff }, { 45, 0x4c4cff }, { 49, 0x874cff }, { 53, 0xff4cff }, { 57, 0xff4c87 }, { 96, 0xff7f00 } };
    int best = 3; float bestD = 1.0e9f;
    for (auto& [vel, rgb] : pal)
    {
        const juce::Colour p (0xff000000 | rgb);
        const float dh = std::abs (p.getHue() - c.getHue()), hue = std::min (dh, 1.0f - dh);
        const float d = c.getSaturation() < 0.15f ? (vel == 3 ? 0.0f : 1.0f) : hue * 4.0f + std::abs (p.getSaturation() - c.getSaturation()) * 0.5f;
        if (d < bestD) { bestD = d; best = vel; }
    }
    return best;
}

//==============================================================================
void ApcSurface::handleIncoming (const juce::MidiMessage& m)
{
    if (m.isController())
    {
        const int cc = m.getControllerNumber(), v = m.getControllerValue();
        if (cc >= knobCc0 && cc < knobCc0 + 8) knob (cc - knobCc0, v < 64 ? v : v - 128);     // relative (two's complement)
        return;
    }
    if (! m.isNoteOnOrOff()) return;
    const int n = m.getNoteNumber();
    const bool down = m.isNoteOn();
    if (n == shiftNote) { shift = down; updateFeedback (true); return; }
    if (drumPads && n < padRows * padCols) { drumPad (n, down); return; }
    if (! down) return;
    if (n < padRows * padCols) { pad (padRows - 1 - n / padCols, n % padCols); return; }       // note 0 = bottom-left pad
    if (n >= trackButton0 && n < trackButton0 + 8) { trackButton (n - trackButton0); return; }
    if (n >= sceneButton0 && n < sceneButton0 + 5) { sceneButton (n - sceneButton0); return; }
    auto& tr = s.edit().getTransport();
    if (n == stopAllNote) { if (shift) s.backToArrangement(); else s.stopAllSlots(); return; }
    if (n == playNote)
    {
        if (shift) tr.setPosition (te::TimePosition());
        else if (tr.isPlaying()) tr.stop (false, false); else tr.play (false);
        return;
    }
    if (n == recordNote)
    {
        if (shift) { drumPads = ! drumPads; lastPad.fill (-1); updateFeedback (true); return; }
        if (tr.isRecording()) tr.stop (true, false); else tr.record (false);
    }
}

te::AudioTrack* ApcSurface::instrumentTrack() const
{
    te::AudioTrack* first = nullptr;
    for (auto* t : s.getTracks (false))
    {
        if (DawSession::kindOf (*t) != DawSession::TrackKind::midi) continue;
        if (t->itemID == selectedID) return t;
        if (first == nullptr) first = t;
    }
    return first;
}

void ApcSurface::drumPad (int index, bool down)
{
    auto* t = instrumentTrack();
    if (t == nullptr) return;
    const int note = juce::jlimit (0, 127, drumBaseNote + index);
    t->injectLiveMidiMessage (down ? juce::MidiMessage::noteOn (1, note, (juce::uint8) 110) : juce::MidiMessage::noteOff (1, note), {});
    padDown[(size_t) index] = down;
}

void ApcSurface::pad (int row, int col)
{
    auto* t = trackForColumn (col);
    if (t == nullptr) return;
    const int scene = sceneOffset + row;
    s.ensureScenes (scene + 1);
    auto* clip = DawSession::slotClip (*t, scene);
    if (shift)
    {
        const int id = row * padCols + col;
        const auto now = juce::Time::getMillisecondCounter();
        if (clip != nullptr)
        {
            if (onSelectClip) onSelectClip (clip);
            if (id == lastPadTapped && now - lastPadTap < 400 && onOpenClip) onOpenClip (clip);   // shift + double tap opens it
        }
        else if (onSelectTrack) onSelectTrack (t);
        lastPadTapped = id; lastPadTap = now;
        return;
    }
    if (isRecordingSlot && isRecordingSlot (*t, scene)) { if (onStopSlotRecording) onStopSlotRecording(); return; }
    if (clip != nullptr) { s.launchClip (*clip); return; }
    bool armed = false;
    for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0) && in->isRecordingEnabled (t->itemID)) armed = true;
    if ((armed || DawSession::kindOf (*t) == DawSession::TrackKind::midi) && onRecordSlot && onRecordSlot (*t, scene)) return;
    s.stopTrackSlots (*t);
}

void ApcSurface::sceneButton (int i)
{
    if (shift) { softMode = (SoftMode) i; updateFeedback (true); return; }
    s.ensureScenes (sceneOffset + i + 1);
    s.launchScene (sceneOffset + i);
}

void ApcSurface::trackButton (int i)
{
    if (shift)
    {
        if (i == 0) moveWindow (0, -1);
        else if (i == 1) moveWindow (0, 1);
        else if (i == 2) moveWindow (-1, 0);
        else if (i == 3) moveWindow (1, 0);
        else { endGestures (true); knobMode = (KnobMode) (i - 4); }
        updateFeedback (true);
        return;
    }
    auto* t = trackForColumn (i);
    if (t == nullptr) return;
    switch (softMode)
    {
        case SoftMode::clipStop: s.stopTrackSlots (*t); break;
        case SoftMode::solo:     t->setSolo (! t->isSolo (false)); break;
        case SoftMode::mute:     t->setMute (! t->isMuted (false)); break;
        case SoftMode::recArm:
        {
            bool has = false;
            for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0)) has = true;
            if (! has)
            {   // give it a default input first: the MIDI keyboards for instruments, the first audio input otherwise
                const bool midi = DawSession::kindOf (*t) == DawSession::TrackKind::midi;
                for (auto* in : t->edit.getAllInputDevices())
                {
                    const auto type = in->getInputDevice().getDeviceType();
                    if (midi ? type == te::InputDevice::physicalMidiDevice : type == te::InputDevice::waveDevice)
                    {
                        [[maybe_unused]] auto r = in->setTarget (t->itemID, ! midi, &t->edit.getUndoManager(), 0);
                        if (! midi) break;
                    }
                }
            }
            bool armed = false;
            for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0) && in->isRecordingEnabled (t->itemID)) armed = true;
            for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0)) in->setRecordingEnabled (t->itemID, ! armed);
            break;
        }
        case SoftMode::select:   selectedID = t->itemID; if (onSelectTrack) onSelectTrack (t); break;
    }
}

void ApcSurface::moveWindow (int dTracks, int dScenes)
{
    const int nTracks = columns().size();
    s.ensureScenes (std::max (s.numScenes(), sceneOffset + dScenes + padRows));
    trackOffset = juce::jlimit (0, std::max (0, nTracks - 1), trackOffset + dTracks);
    sceneOffset = juce::jlimit (0, std::max (0, s.numScenes() - 1), sceneOffset + dScenes);
    if (onWindowMoved) onWindowMoved();
}

te::AutomatableParameter* ApcSurface::knobParam (int i)
{
    if (knobMode == KnobMode::device)
    {
        te::AudioTrack* sel = nullptr;
        for (auto* t : s.getTracks (true)) if (t->itemID == selectedID) sel = t;
        if (sel == nullptr) return nullptr;
        auto inserts = s.getInsertPlugins (*sel);
        te::Plugin* p = nullptr;
        for (auto* pl : inserts) if (! pl->isSynth()) { p = pl; break; }     // the first effect, else the instrument
        if (p == nullptr && ! inserts.isEmpty()) p = inserts[0];
        if (p == nullptr) return nullptr;
        auto params = p->getAutomatableParameters();
        return i < params.size() ? params[i] : nullptr;
    }
    auto* t = trackForColumn (i);
    if (t == nullptr) return nullptr;
    auto vp = t->getVolumePlugin();
    if (knobMode == KnobMode::volume) return vp != nullptr ? vp->volParam.get() : nullptr;
    if (knobMode == KnobMode::pan) return vp != nullptr ? vp->panParam.get() : nullptr;
    auto buses = s.getBuses();
    te::AudioTrack* bus = nullptr;
    for (auto* b : buses) if (b != t && ! DawSession::isFolder (*b)) { bus = b; break; }
    if (bus == nullptr) return nullptr;
    auto* snd = s.findSend (*t, *bus);
    if (snd == nullptr) { s.setSend (*t, *bus, -60.0f); snd = s.findSend (*t, *bus); }     // turning a send up creates it
    return snd != nullptr ? snd->gain.get() : nullptr;
}

void ApcSurface::knob (int i, int delta)
{
    auto* p = knobParam (i);
    if (p == nullptr || delta == 0) return;
    touch (p);
    const float step = knobMode == KnobMode::pan ? 0.006f : 0.004f;
    p->setNormalisedParameter (juce::jlimit (0.0f, 1.0f, p->getCurrentNormalisedValue() + delta * step), juce::sendNotification);
}

void ApcSurface::touch (te::AutomatableParameter* p)
{
    // knob turns count as one gesture each, so automation write records them
    const auto now = juce::Time::getMillisecondCounter();
    for (auto& g : gestures) if (g.param.get() == p) { g.last = now; return; }
    p->parameterChangeGestureBegin();
    gestures.push_back ({ p, now });
}

void ApcSurface::endGestures (bool all)
{
    const auto now = juce::Time::getMillisecondCounter();
    for (auto it = gestures.begin(); it != gestures.end();)
        if (all || now - it->last > 400) { it->param->parameterChangeGestureEnd(); it = gestures.erase (it); }
        else ++it;
}

//==============================================================================
void ApcSurface::allOff()
{
    if (! send) return;
    for (int n = 0; n < 40; ++n) send (juce::MidiMessage::noteOn (1, n, (juce::uint8) 0));
    for (int n : { 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x52, 0x53, 0x54, 0x55, 0x56, playNote, recordNote })
        send (juce::MidiMessage::noteOn (1, n, (juce::uint8) 0));
    reset();
}

void ApcSurface::updateFeedback (bool forceAll)
{
    endGestures (false);
    if (! send) return;
    if (forceAll) reset();
    ++blink;
    const bool blinkOn = (blink / 5) % 2 == 0;                       // ~2 Hz at 20 updates / s
    auto led = [this] (int note, int value)
    {
        auto it = lastLed.find (note);
        if (it != lastLed.end() && it->second == value) return;
        lastLed[note] = value;
        send (juce::MidiMessage::noteOn (1, note, (juce::uint8) value));
    };
    // pads: clip colour (dim), playing = bright green, queued = blinking, empty slot on an armed track = dim red
    std::array<bool, 5> rowPlaying {}, rowQueued {};
    if (drumPads)
    {   // drum pads: the instrument track's colour, white while held; every 4th note (C, E, G#) a little brighter
        auto* t = instrumentTrack();
        const int col = t != nullptr ? nearestPaletteColour (DawSession::trackColour (*t)) : 9;
        for (int note = 0; note < padRows * padCols; ++note)
        {
            const int chan = padDown[(size_t) note] ? 6 : (note % 4 == 0 ? 3 : 1), colour = padDown[(size_t) note] ? 3 : col;
            const int key = (chan << 8) | colour;
            if (lastPad[(size_t) note] != key) { lastPad[(size_t) note] = key; send (juce::MidiMessage::noteOn (chan + 1, note, (juce::uint8) colour)); }
        }
    }
    else
    for (int col = 0; col < padCols; ++col)
    {
        auto* t = trackForColumn (col);
        bool armed = false;
        if (t != nullptr) for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0) && in->isRecordingEnabled (t->itemID)) armed = true;
        for (int row = 0; row < padRows; ++row)
        {
            int chan = 0, colour = 0;                                       // behaviour channel (brightness), palette colour
            if (t != nullptr)
            {
                if (isRecordingSlot && isRecordingSlot (*t, sceneOffset + row)) { chan = 13; colour = 5; rowQueued[(size_t) row] = true; }   // the pad blinks red by itself (behaviour channel 14)
                else if (auto* c = DawSession::slotClip (*t, sceneOffset + row))
                {
                    const bool playing = DawSession::isSlotPlaying (*c), queued = DawSession::isSlotQueued (*c);
                    if (playing) rowPlaying[(size_t) row] = true;
                    if (queued) rowQueued[(size_t) row] = true;
                    if (queued) { chan = 6; colour = blinkOn ? 21 : nearestPaletteColour (DawSession::trackColour (*t)); }
                    else if (playing) { chan = 6; colour = 21; }
                    else { chan = 2; colour = nearestPaletteColour (DawSession::trackColour (*t)); }
                }
                else if (armed) { chan = 0; colour = 5; }
            }
            const int note = (padRows - 1 - row) * padCols + col;
            const int key = (chan << 8) | colour;
            if (lastPad[(size_t) note] != key)
            {
                lastPad[(size_t) note] = key;
                send (juce::MidiMessage::noteOn (chan + 1, note, (juce::uint8) colour));
            }
        }
    }
    // scene buttons: a playing row is lit, a queued one blinks; with Shift held they show the soft-key mode
    for (int i = 0; i < 5; ++i)
        led (sceneButton0 + i, shift ? ((int) softMode == i ? 1 : 0) : rowPlaying[(size_t) i] ? 1 : rowQueued[(size_t) i] ? (blinkOn ? 1 : 0) : 0);
    // track buttons: the soft-key mode's state; with Shift held, the arrows that can move and the knob mode
    const int nTracks = columns().size();
    for (int i = 0; i < 8; ++i)
    {
        int v = 0;
        if (shift)
        {
            if (i == 0) v = sceneOffset > 0;
            else if (i == 1) v = 1;
            else if (i == 2) v = trackOffset > 0;
            else if (i == 3) v = trackOffset + padCols < nTracks;
            else v = (int) knobMode == i - 4;
        }
        else if (auto* t = trackForColumn (i))
        {
            switch (softMode)
            {
                case SoftMode::clipStop:
                    for (auto* sl : t->getClipSlotList().getClipSlots())
                        if (auto* c = sl != nullptr ? sl->getClip() : nullptr) if (DawSession::isSlotPlaying (*c)) v = 1;
                    break;
                case SoftMode::solo:   v = t->isSolo (false); break;
                case SoftMode::mute:   v = ! t->isMuted (false); break;     // lit = playing, like the mixer
                case SoftMode::recArm:
                    for (auto* in : t->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0) && in->isRecordingEnabled (t->itemID)) v = 1;
                    break;
                case SoftMode::select: v = t->itemID == selectedID; break;
            }
        }
        led (trackButton0 + i, v);
    }
    auto& tr = s.edit().getTransport();
    led (playNote, tr.isPlaying() ? 1 : 0);
    led (recordNote, tr.isRecording() ? 1 : (shift && drumPads ? 1 : 0));
}

juce::String ApcSurface::describe (const juce::MidiMessage& m) const
{
    static const char* soft[] { "Clip Stop", "Solo", "Mute", "Rec Arm", "Select" };
    static const char* trk[] { "Up", "Down", "Left", "Right", "Volume", "Pan", "Send", "Device" };
    if (m.isController())
    {
        const int cc = m.getControllerNumber(), v = m.getControllerValue();
        if (cc >= knobCc0 && cc < knobCc0 + 8) return "Knob K" + juce::String (cc - knobCc0 + 1) + " " + (v < 64 ? "+" + juce::String (v) : juce::String (v - 128));
        return "CC " + juce::String (cc) + " = " + juce::String (v);
    }
    if (! m.isNoteOnOrOff()) return m.getDescription();
    const int n = m.getNoteNumber();
    const juce::String state = m.isNoteOn() ? "press" : "release";
    if (n < 40) return "Pad row " + juce::String (padRows - n / padCols) + " col " + juce::String (n % padCols + 1) + " " + state;
    if (n >= trackButton0 && n < trackButton0 + 8) return "Track button " + juce::String (n - trackButton0 + 1) + " (" + trk[n - trackButton0] + ") " + state;
    if (n >= sceneButton0 && n < sceneButton0 + 5) return "Scene " + juce::String (n - sceneButton0 + 1) + " (" + soft[n - sceneButton0] + ") " + state;
    if (n == stopAllNote) return "Stop All Clips " + state;
    if (n == playNote) return "Play " + state;
    if (n == recordNote) return "Record " + state;
    if (n == shiftNote) return "Shift " + state;
    return "Note " + juce::String (n) + " " + state;
}
