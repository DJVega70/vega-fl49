#include "Fl49Hub.h"

static const juce::Identifier keyRootProp ("vegaKeyRoot"), keyScaleProp ("vegaKeyScale"), keyModeProp ("vegaKeyMode"), chordModeProp ("vegaChordMode");

Fl49Hub::Fl49Hub (te::Engine& e, DawSession& s) : engine (e), session (s)
{
    loadSettings();
    loadKeySettingsFromEdit();
    juce::MessageManager::callAsync ([this] { attach(); });
    startTimer (1000);
}

Fl49Hub::~Fl49Hub()
{
    stopTimer();
    cancelPendingUpdate();
    detach();
}

bool Fl49Hub::isKeysPort (const juce::String& n)
{
    if (! n.containsIgnoreCase ("FLkey")) return false;
    // the DAW port carries HUI, not notes: "MIDIIN2 (FLkey 2 49 MIDI)" on Windows, "FLkey 2 49 DAW Out" on Mac
    return ! (n.containsIgnoreCase ("DAW") || n.containsIgnoreCase ("MIDIIN2") || n.containsIgnoreCase ("MIDIOUT2") || n.containsIgnoreCase ("HUI"));
}

juce::File Fl49Hub::settingsFile() const { return engine.getPropertyStorage().getAppPrefsFolder().getChildFile ("VegaFL49.xml"); }

te::MidiInputDevice* Fl49Hub::virtualInput() const { return virtualDev.get(); }

//==============================================================================
void Fl49Hub::attach()
{
    auto& dm = engine.getDeviceManager();
    auto findVirtual = [&]() -> std::shared_ptr<te::MidiInputDevice>
    {
        for (auto& d : dm.getMidiInDevices())
            if (d->getDeviceType() == te::InputDevice::virtualMidiDevice && d->getName() == virtualName) return d;
        return {};
    };
    if (virtualDev == nullptr)
    {
        virtualDev = findVirtual();
        if (virtualDev == nullptr) { [[maybe_unused]] auto r = dm.createVirtualMidiDevice (virtualName); virtualDev = findVirtual(); }
        if (virtualDev != nullptr && ! virtualDev->isEnabled()) virtualDev->setEnabled (true);
        virtualRaw = dynamic_cast<te::VirtualMidiInputDevice*> (virtualDev.get());
    }
    if (attached != nullptr) return;
    for (auto& d : dm.getMidiInDevices())
    {
        if (d->getDeviceType() != te::InputDevice::physicalMidiDevice || ! isKeysPort (d->getName())) continue;
        if (auto* p = dynamic_cast<te::PhysicalMidiInputDevice*> (d.get()))
        {
            if (! d->isEnabled()) d->setEnabled (true);
            p->addListener (this);
            attached = d; attachedName = d->getName();
            log.add ("Keys: " + attachedName);
            if (onChange) onChange();
            return;
        }
    }
}

void Fl49Hub::detach()
{
    if (attached != nullptr)
        if (auto* p = dynamic_cast<te::PhysicalMidiInputDevice*> (attached.get())) p->removeListener (this);   // waits for a callback in progress
    attached.reset(); attachedName = {};
}

void Fl49Hub::timerCallback()
{
    // hot-plug: the engine rebuilds its device list when MIDI devices come and go
    if (attached != nullptr)
    {
        bool stillThere = false;
        for (auto& d : engine.getDeviceManager().getMidiInDevices()) if (d == attached && d->isEnabled()) stillThere = true;
        if (! stillThere) { detach(); log.add ("FLkey keys port went away"); if (onChange) onChange(); }
    }
    if (virtualDev != nullptr)
    {
        bool stillThere = false;
        for (auto& d : engine.getDeviceManager().getMidiInDevices()) if (d == virtualDev) stillThere = true;
        if (! stillThere) { detach(); virtualRaw = nullptr; virtualDev.reset(); }   // no MIDI callback can be using it once detached
    }
    if (attached == nullptr || virtualDev == nullptr) attach();
}

//==============================================================================
void Fl49Hub::send (const juce::MidiMessage& m)
{
    if (auto* vd = virtualRaw.load())
        vd->handleIncomingMidiMessage (m, vd->getMPESourceID());
}

void Fl49Hub::handleIncomingMidiMessage (const juce::MidiMessage& m)
{
    auto push = [this] (Event e)
    {
        int s1, n1, s2, n2; fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { events[(size_t) s1] = e; fifo.finishedWrite (1); triggerAsyncUpdate(); }
    };
    if (m.isNoteOnOrOff())
    {
        const int ch = m.getChannel(), note = m.getNoteNumber();
        if (teachIndex.load() >= 0)
        {
            if (m.isNoteOn()) push ({ -1, m.getVelocity(), true, ch, note });
            return;                                                    // nothing plays while teaching
        }
        int pad;
        { const juce::SpinLock::ScopedLockType sl (padLock); pad = padMap.padFor (ch, note); }
        if (pad >= 0)
        {
            if (m.isNoteOn()) push ({ pad, m.getVelocity(), false, ch, note });
            if (padMode() == PadMode::drums) send (m);                 // drum mode: pads play the instrument as they are
            return;
        }
        fl49::NoteProcessor::Out out[32];
        int n;
        { const juce::SpinLock::ScopedLockType sl (processorLock); n = processor.process (ch, note, m.isNoteOn() ? m.getVelocity() : 0, m.isNoteOn(), out); }
        for (int i = 0; i < n; ++i)
            send (out[i].on ? juce::MidiMessage::noteOn (out[i].channel, out[i].note, (juce::uint8) out[i].velocity)
                            : juce::MidiMessage::noteOff (out[i].channel, out[i].note));
        return;
    }
    send (m);   // wheels, sustain, aftertouch, CCs: as they are
}

void Fl49Hub::handleAsyncUpdate()
{
    int s1, n1, s2, n2;
    const int ready = fifo.getNumReady();
    fifo.prepareToRead (ready, s1, n1, s2, n2);
    std::vector<Event> got;
    for (int i = 0; i < n1; ++i) got.push_back (events[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) got.push_back (events[(size_t) (s2 + i)]);
    fifo.finishedRead (n1 + n2);
    for (auto& e : got)
    {
        if (e.teach)
        {
            const int i = teachIndex.load();
            if (i < 0 || i >= 16) continue;
            bool repeat = false;
            {
                const juce::SpinLock::ScopedLockType sl (padLock);
                for (int k = 0; k < i; ++k) if (padMap.channel[(size_t) k] == e.channel && padMap.note[(size_t) k] == e.note) repeat = true;
                if (! repeat) { padMap.channel[(size_t) i] = (juce::uint8) e.channel; padMap.note[(size_t) i] = (juce::uint8) e.note; }
            }
            if (repeat) continue;   // the same pad twice: wait for the next one
            padHitTime[(size_t) i] = juce::Time::getMillisecondCounter(); padVelocity[(size_t) i] = e.velocity;
            if (i == 15) { teachIndex = -1; saveSettings(); log.add ("Pads learned"); }
            else teachIndex = i + 1;
            continue;
        }
        padHitTime[(size_t) e.pad] = juce::Time::getMillisecondCounter(); padVelocity[(size_t) e.pad] = e.velocity;
        if (padMode() != PadMode::drums) pressPad (e.pad, e.velocity);
    }
    if (onChange) onChange();
}

//==============================================================================
void Fl49Hub::pressPad (int pad, int velocity)
{
    if (pad < 0 || pad >= 16) return;
    padHitTime[(size_t) pad] = juce::Time::getMillisecondCounter(); padVelocity[(size_t) pad] = velocity;
    const int row = pad / 8, col = pad % 8;
    auto tracks = session.getTracks (true);
    const int bank = getBankStart ? getBankStart() : 0;
    auto* t = bank + col < tracks.size() ? tracks[bank + col] : nullptr;
    switch (padMode())
    {
        case PadMode::drums:
        {   // a click on the screen: play the pad's note through the same path as the hardware
            const auto map = pads();
            const int ch = map.channel[(size_t) pad], note = map.note[(size_t) pad];
            send (juce::MidiMessage::noteOn (ch, note, (juce::uint8) juce::jlimit (1, 127, velocity)));
            juce::Timer::callAfterDelay (180, [this, ch, note] { send (juce::MidiMessage::noteOff (ch, note)); });
            break;
        }
        case PadMode::clips:
        {
            if (t == nullptr || DawSession::kindOf (*t) == DawSession::TrackKind::bus) break;
            const int scene = sceneOffset + row;
            if (auto* c = DawSession::slotClip (*t, scene))
            {
                if (DawSession::isSlotPlaying (*c) || DawSession::isSlotQueued (*c)) session.stopTrackSlots (*t);
                else session.launchClip (*c);
            }
            else session.stopTrackSlots (*t);
            break;
        }
        case PadMode::scenes:
            if (row == 0) { if (sceneOffset + col < session.numScenes()) session.launchScene (sceneOffset + col); }
            else if (t != nullptr) session.stopTrackSlots (*t);
            break;
    }
    if (onChange) onChange();
}

void Fl49Hub::setPadMode (PadMode m) { padModeValue = (int) m; saveSettings(); if (onChange) onChange(); }
void Fl49Hub::moveScenes (int d) { sceneOffset = juce::jlimit (0, std::max (0, session.numScenes() - 1), sceneOffset + d); if (onChange) onChange(); }
void Fl49Hub::startTeach() { teachIndex = 0; log.add ("Teach: hit pads 1-16 (top row first, left to right)"); if (onChange) onChange(); }
void Fl49Hub::cancelTeach() { teachIndex = -1; loadSettings(); if (onChange) onChange(); }
void Fl49Hub::resetPads() { { const juce::SpinLock::ScopedLockType sl (padLock); padMap.setDefault(); } saveSettings(); if (onChange) onChange(); }

//==============================================================================
void Fl49Hub::setKeySettings (const fl49::Settings& s)
{
    panic();                         // release anything held under the old settings first
    { const juce::SpinLock::ScopedLockType sl (processorLock); processor.setSettings (s); }
    auto& st = session.edit().state;
    st.setProperty (keyRootProp, s.root, nullptr); st.setProperty (keyScaleProp, s.scale, nullptr);
    st.setProperty (keyModeProp, (int) s.keys, nullptr); st.setProperty (chordModeProp, (int) s.chord, nullptr);
    if (onChange) onChange();
}

void Fl49Hub::loadKeySettingsFromEdit()
{
    auto& st = session.edit().state;
    fl49::Settings s;
    s.root = juce::jlimit (0, 11, (int) st.getProperty (keyRootProp, 0));
    s.scale = juce::jlimit (0, (int) fl49::scales().size() - 1, (int) st.getProperty (keyScaleProp, 0));
    s.keys = (fl49::KeyMode) juce::jlimit (0, 2, (int) st.getProperty (keyModeProp, 0));
    s.chord = (fl49::ChordMode) juce::jlimit (0, 4, (int) st.getProperty (chordModeProp, 0));
    const juce::SpinLock::ScopedLockType sl (processorLock);
    processor.setSettings (s);
}

void Fl49Hub::panic()
{
    fl49::NoteProcessor::Out out[256];
    int n;
    { const juce::SpinLock::ScopedLockType sl (processorLock); n = processor.releaseAll (out, (int) std::size (out)); }
    for (int i = 0; i < n; ++i) send (juce::MidiMessage::noteOff (out[i].channel, out[i].note));
}

//==============================================================================
juce::Array<te::Plugin*> Fl49Hub::macroCandidates()
{
    auto* t = getSelectedTrack ? getSelectedTrack() : nullptr;
    return t != nullptr ? session.getInsertPlugins (*t) : juce::Array<te::Plugin*>();
}

te::Plugin* Fl49Hub::macroPlugin()
{
    auto list = macroCandidates();
    if (list.isEmpty()) return nullptr;
    return list[juce::jlimit (0, list.size() - 1, macroSlot)];
}

int Fl49Hub::macroParamCount()
{
    auto* p = macroPlugin();
    return p != nullptr ? p->getAutomatableParameters().size() : 0;
}

te::AutomatableParameter* Fl49Hub::macroParam (int knob)
{
    auto* p = macroPlugin();
    if (p == nullptr || knob < 0 || knob > 7) return nullptr;
    auto params = p->getAutomatableParameters();
    const int i = macroPage * 8 + knob;
    return i < params.size() ? params[i] : nullptr;
}

//==============================================================================
void Fl49Hub::saveSettings()
{
    juce::XmlElement x ("VegaFL49");
    x.setAttribute ("padMode", padModeValue.load());
    const auto map = pads();
    juce::StringArray pc, pn;
    for (int i = 0; i < 16; ++i) { pc.add (juce::String (map.channel[(size_t) i])); pn.add (juce::String (map.note[(size_t) i])); }
    x.setAttribute ("padChannels", pc.joinIntoString (","));
    x.setAttribute ("padNotes", pn.joinIntoString (","));
    x.writeTo (settingsFile());
}

void Fl49Hub::loadSettings()
{
    auto x = juce::XmlDocument::parse (settingsFile());
    if (x == nullptr) return;
    padModeValue = juce::jlimit (0, 2, x->getIntAttribute ("padMode", (int) PadMode::drums));
    juce::StringArray pc, pn;
    pc.addTokens (x->getStringAttribute ("padChannels"), ",", {});
    pn.addTokens (x->getStringAttribute ("padNotes"), ",", {});
    if (pc.size() == 16 && pn.size() == 16)
    {
        const juce::SpinLock::ScopedLockType sl (padLock);
        for (int i = 0; i < 16; ++i)
        {
            padMap.channel[(size_t) i] = (juce::uint8) juce::jlimit (1, 16, pc[i].getIntValue());
            padMap.note[(size_t) i] = (juce::uint8) juce::jlimit (0, 127, pn[i].getIntValue());
        }
    }
}
