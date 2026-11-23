#include "Controllers.h"

ControllerHub::ControllerHub (te::Engine& e, DawSession& s) : surface (s), engine (e), session (s)
{
    surface.send = [this] (const juce::MidiMessage& m) { if (output) output->sendMessageNow (m); };
    surface.onSelectTrack = [this] (te::AudioTrack* t) { if (t) surface.selectedID = t->itemID; if (onSelectTrack) onSelectTrack (t); };
    surface.onSave = [this] { if (onSave) onSave(); };
    startTimer (50);
}
ControllerHub::~ControllerHub() { stopTimer(); connect ({}, {}); }

juce::File ControllerHub::settingsFile() const { return engine.getPropertyStorage().getAppPrefsFolder().getChildFile ("VegaControllers.xml"); }

bool ControllerHub::looksLikeDawPort (const juce::String& n)
{
    // FLkey 2: "FLkey 2 49 DAW Out" (Mac) or "MIDIIN2 (FLkey 2 49)" (Windows). Also matches other Novation/HUI DAW ports.
    return n.containsIgnoreCase ("DAW") || n.containsIgnoreCase ("MIDIIN2") || n.containsIgnoreCase ("MIDIOUT2") || n.containsIgnoreCase ("HUI");
}

void ControllerHub::releaseFromEngine (const juce::String& inName, const juce::String& outName)
{
    // Windows only allows one app/connection per MIDI port: take it away from the engine's note inputs
    auto& dm = engine.getDeviceManager();
    for (auto& d : dm.getMidiInDevices()) if (d->getName() == inName || (inName.isNotEmpty() && d->getName().contains (inName))) d->setEnabled (false);
    for (int i = 0; i < dm.getNumMidiOutDevices(); ++i) if (auto* d = dm.getMidiOutDevice (i)) if (d->getName() == outName) d->setEnabled (false);
}

bool ControllerHub::connect (const juce::String& inName, const juce::String& outName, bool userChoice)
{
    if (input) input->stop();
    input.reset(); output.reset();
    inputName = outputName = {};
    surface.reset();
    if (inName.isEmpty())
    {
        if (userChoice) { juce::XmlElement x ("VegaController"); x.setAttribute ("off", 1); x.writeTo (settingsFile()); log.add ("Disconnected"); }
        return false;
    }
    releaseFromEngine (inName, outName);
    for (auto& d : juce::MidiInput::getAvailableDevices()) if (d.name == inName) input = juce::MidiInput::openDevice (d.identifier, this);
    for (auto& d : juce::MidiOutput::getAvailableDevices()) if (d.name == outName) output = juce::MidiOutput::openDevice (d.identifier);
    if (input == nullptr) { log.add ("Couldn't open " + inName + " (is another program using it?)"); return false; }
    input->start();
    inputName = inName; outputName = output ? outName : juce::String();
    log.add ("Connected: in = " + inName + (output ? ", out = " + outName : juce::String (", no feedback output")));
    juce::XmlElement x ("VegaController"); x.setAttribute ("in", inName); x.setAttribute ("out", outName);
    x.writeTo (settingsFile());
    surface.sendPing(); surface.updateFeedback (true);
    if (onLog) onLog();
    return true;
}

void ControllerHub::autoConnect (bool forceAuto)
{
    // remembered ports first
    juce::String in, out;
    if (auto x = juce::XmlDocument::parse (settingsFile()))
    {
        if (x->getBoolAttribute ("off") && ! forceAuto) return;
        in = x->getStringAttribute ("in"); out = x->getStringAttribute ("out");
    }
    auto ins = juce::MidiInput::getAvailableDevices(); auto outs = juce::MidiOutput::getAvailableDevices();
    auto hasIn = [&] (const juce::String& n) { for (auto& d : ins) if (d.name == n) return true; return false; };
    auto hasOut = [&] (const juce::String& n) { for (auto& d : outs) if (d.name == n) return true; return false; };
    if (in.isEmpty() || ! hasIn (in))
    {
        in = {};
        for (auto& d : ins) if (looksLikeDawPort (d.name) && ! ApcHub::isApcControlPort (d.name) && (d.name.containsIgnoreCase ("FLkey") || d.name.containsIgnoreCase ("MIDIIN2"))) { in = d.name; break; }
        if (in.isEmpty()) for (auto& d : ins) if (looksLikeDawPort (d.name) && ! ApcHub::isApcControlPort (d.name)) { in = d.name; break; }
    }
    if (in.isEmpty()) return;
    if (out.isEmpty() || ! hasOut (out))
    {
        out = {};
        juce::String want = in.replace ("DAW Out", "DAW In").replace ("MIDIIN2", "MIDIOUT2");
        for (auto& d : outs) if (d.name == want) out = d.name;
        if (out.isEmpty()) for (auto& d : outs) if (looksLikeDawPort (d.name) && ! ApcHub::isApcControlPort (d.name)) { out = d.name; break; }
    }
    connect (in, out);
}

void ControllerHub::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    { const juce::ScopedLock sl (lock); pending.push_back (m); }
    triggerAsyncUpdate();
}

void ControllerHub::handleAsyncUpdate()
{
    std::vector<juce::MidiMessage> msgs;
    { const juce::ScopedLock sl (lock); msgs.swap (pending); }
    for (auto& m : msgs)
    {
        const auto text = surface.describe (m);       // describe before handling so zone context is right for releases too
        surface.handleIncoming (m);
        if (text != "ping reply") log.add (juce::Time::getCurrentTime().toString (false, true, true, true) + "   " + text
                                           + "      [" + juce::String::toHexString (m.getRawData(), m.getRawDataSize()) + "]");
    }
    while (log.size() > 300) log.remove (0);
    if (onLog && ! msgs.empty()) onLog();
}

void ControllerHub::timerCallback()
{
    if (! isConnected()) { if (++ticks % 60 == 0) autoConnect(); return; }      // hot-plug: look again every 3 s
    surface.updateFeedback();
    if (++ticks % 20 == 0) surface.sendPing();
}

//==============================================================================
ControllerPanel::ControllerPanel (ControllerHub& h) : hub (h)
{
    for (auto* b : { &inBox, &outBox }) addAndMakeVisible (b);
    connectBtn.setButtonText ("CONNECT"); autoBtn.setButtonText ("AUTO-DETECT"); clearBtn.setButtonText ("CLEAR LOG");
    connectBtn.onClick = [this] { hub.connect (inBox.getText() == "(none)" ? juce::String() : inBox.getText(), outBox.getText() == "(none)" ? juce::String() : outBox.getText(), true); refresh(); };
    autoBtn.onClick = [this] { hub.autoConnect (true); fillLists(); refresh(); if (! hub.isConnected()) { hub.log.add ("No FLkey / HUI DAW port found - is it plugged in?"); refresh(); } };
    clearBtn.onClick = [this] { hub.log.clear(); lastLogSize = -1; refresh(); };
    for (auto* b : { &connectBtn, &autoBtn, &clearBtn }) addAndMakeVisible (b);
    monitor.setMultiLine (true); monitor.setReadOnly (true); monitor.setFont (VegaFonts::mono (12.0f));
    monitor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff111111)); monitor.setColour (juce::TextEditor::textColourId, VegaColours::accent);
    addAndMakeVisible (monitor);
    status.setFont (VegaFonts::ui (13.0f, true)); addAndMakeVisible (status);
    help.setFont (VegaFonts::ui (12.0f)); help.setColour (juce::Label::textColourId, VegaColours::textDim);
    help.setJustificationType (juce::Justification::topLeft);
    help.setText ("Novation FLkey 2 49: plug it in and press AUTO-DETECT (or pick the DAW port: \"MIDIIN2 (FLkey 2 49)\" + \"MIDIOUT2 (FLkey 2 49)\" on Windows, "
                  "\"FLkey 2 49 DAW Out\" + \"FLkey 2 49 DAW In\" on Mac). The keys and drum pads keep working through the normal MIDI port and play the selected instrument track.\n"
                  "Faders = track volumes (8 at a time), encoders = pan (or Sends A-E), fader buttons = select, pads = mute/solo, transport = play/stop/record/loop, "
                  "bank buttons move to the next 8 tracks. Everything the controller sends shows in the monitor below.", juce::dontSendNotification);
    addAndMakeVisible (help);
    hub.onLog = [this] { refresh(); };
    fillLists(); refresh();
    setSize (720, 560);
    startTimerHz (4);
}

void ControllerPanel::fillLists()
{
    inBox.clear (juce::dontSendNotification); outBox.clear (juce::dontSendNotification);
    inBox.addItem ("(none)", 1); outBox.addItem ("(none)", 1);
    int id = 2; for (auto& d : juce::MidiInput::getAvailableDevices()) inBox.addItem (d.name, id++);
    id = 2; for (auto& d : juce::MidiOutput::getAvailableDevices()) outBox.addItem (d.name, id++);
    inBox.setText (hub.inputName.isEmpty() ? "(none)" : hub.inputName, juce::dontSendNotification);
    outBox.setText (hub.outputName.isEmpty() ? "(none)" : hub.outputName, juce::dontSendNotification);
}

void ControllerPanel::refresh()
{
    status.setText (hub.isConnected() ? "CONNECTED (HUI):  " + hub.inputName : juce::String ("NOT CONNECTED"), juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, hub.isConnected() ? VegaColours::accent : VegaColours::gold);
    if (hub.log.size() != lastLogSize) { lastLogSize = hub.log.size(); monitor.setText (hub.log.joinIntoString ("\n")); monitor.moveCaretToEnd(); }
}

void ControllerPanel::paint (juce::Graphics& g)
{
    g.fillAll (VegaColours::background);
    g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f, true));
    g.drawText ("CONTROLLER INPUT (DAW PORT)", 16, 112, 300, 14, juce::Justification::left);
    g.drawText ("FEEDBACK OUTPUT (LIGHTS, FADERS, SCREEN)", 16, 166, 320, 14, juce::Justification::left);
    g.drawText ("MIDI MONITOR", 16, 222, 200, 14, juce::Justification::left);
}

void ControllerPanel::resized()
{
    help.setBounds (12, 8, getWidth() - 24, 100);
    inBox.setBounds (16, 128, getWidth() - 300, 26); outBox.setBounds (16, 182, getWidth() - 300, 26);
    autoBtn.setBounds (getWidth() - 274, 128, 124, 26); connectBtn.setBounds (getWidth() - 140, 128, 124, 26);
    status.setBounds (getWidth() - 274, 182, 258, 26);
    clearBtn.setBounds (getWidth() - 116, 216, 100, 22);
    monitor.setBounds (16, 242, getWidth() - 32, getHeight() - 258);
}

//==============================================================================
ApcHub::ApcHub (te::Engine& e, DawSession& s) : surface (s), engine (e)
{
    surface.send = [this] (const juce::MidiMessage& m) { if (output) output->sendMessageNow (m); };
    startTimer (50);
}
ApcHub::~ApcHub() { stopTimer(); surface.allOff(); connect ({}, {}); }

juce::File ApcHub::settingsFile() const { return engine.getPropertyStorage().getAppPrefsFolder().getChildFile ("VegaApc.xml"); }

bool ApcHub::isApcControlPort (const juce::String& n)
{
    if (! n.containsIgnoreCase ("APC Key 25")) return false;
    return n.containsIgnoreCase ("MIDIIN2") || n.containsIgnoreCase ("MIDIOUT2") || n.containsIgnoreCase ("Control")
        || n.containsIgnoreCase ("Port 2") || n.trimEnd().endsWithIgnoreCase (" C");
}

bool ApcHub::connect (const juce::String& inName, const juce::String& outName, bool userChoice)
{
    if (output) surface.allOff();
    if (input) input->stop();
    input.reset(); output.reset();
    inputName = outputName = {};
    surface.reset();
    if (inName.isEmpty())
    {
        if (userChoice) { juce::XmlElement x ("VegaApc"); x.setAttribute ("off", 1); x.writeTo (settingsFile()); }
        return false;
    }
    // The APC's first port ("APC Key 25 mk2") is its keys. Pads, buttons and knobs only talk on the second
    // (control) port, and taking the first port here would also silence the keys. So never use it: swap to
    // the control port when there is one, otherwise leave the keys alone and say so.
    auto isKeysPort = [] (const juce::String& n) { return n.containsIgnoreCase ("APC Key 25") && ! isApcControlPort (n); };
    juce::String in = inName, out = outName;
    if (isKeysPort (in))
    {
        in = {};
        for (auto& d : juce::MidiInput::getAvailableDevices()) if (isApcControlPort (d.name)) { in = d.name; break; }
        if (in.isEmpty())
        {
            log.add ("\"" + inName + "\" is the APC's keys port - its pads use the second (control) port, which Windows isn't showing. Unplug the APC and plug it back in.");
            lastError = "\"" + inName + "\" is the APC's keys port. The pads and knobs use its second port (\"MIDIIN2 (APC Key 25 mk2)\"), which isn't showing right now - "
                        "unplug the APC, plug it back in and press Auto-detect. Close any other app that uses the APC (Ableton, a browser MIDI page) first.";
            return false;
        }
        log.add ("Switched from the keys port to the control port: " + in);
    }
    if (out.isEmpty() || isKeysPort (out))
    {
        out = {};
        const auto want = in.replace ("MIDIIN2", "MIDIOUT2");
        for (auto& d : juce::MidiOutput::getAvailableDevices()) if (d.name == want) out = d.name;
        if (out.isEmpty()) for (auto& d : juce::MidiOutput::getAvailableDevices()) if (isApcControlPort (d.name)) { out = d.name; break; }
    }
    lastError = {};
    // Windows lets only one connection use a MIDI port: keep the engine off the control port
    auto& dm = engine.getDeviceManager();
    for (auto& d : dm.getMidiInDevices()) if (d->getName() == in) d->setEnabled (false);
    for (int i = 0; i < dm.getNumMidiOutDevices(); ++i) if (auto* d = dm.getMidiOutDevice (i)) if (d->getName() == out) d->setEnabled (false);
    for (auto& d : juce::MidiInput::getAvailableDevices()) if (d.name == in) input = juce::MidiInput::openDevice (d.identifier, this);
    for (auto& d : juce::MidiOutput::getAvailableDevices()) if (d.name == out) output = juce::MidiOutput::openDevice (d.identifier);
    if (input == nullptr) { log.add ("Couldn't open " + in); lastError = "Couldn't open \"" + in + "\" - is another app (Ableton, a browser MIDI page) using the APC?"; return false; }
    input->start();
    inputName = in; outputName = output ? out : juce::String();
    log.add ("Connected: " + in + (output ? " / " + out : juce::String (" (no lights: output not found)")));
    if (userChoice) { juce::XmlElement x ("VegaApc"); x.setAttribute ("in", in); x.setAttribute ("out", out); x.writeTo (settingsFile()); }
    if (output) output->sendMessageNow (ApcSurface::introduction());
    surface.allOff();
    surface.updateFeedback (true);
    return true;
}

void ApcHub::autoConnect (bool forceAuto)
{
    juce::String in, out;
    if (auto x = juce::XmlDocument::parse (settingsFile()))
    {
        if (x->getBoolAttribute ("off") && ! forceAuto) return;
        in = x->getStringAttribute ("in"); out = x->getStringAttribute ("out");
    }
    auto ins = juce::MidiInput::getAvailableDevices(); auto outs = juce::MidiOutput::getAvailableDevices();
    bool hasIn = false; for (auto& d : ins) if (d.name == in && isApcControlPort (d.name)) hasIn = true;   // a saved keys port is a mistake: find the control port
    if (! hasIn) { in = {}; for (auto& d : ins) if (isApcControlPort (d.name)) { in = d.name; break; } }
    if (in.isEmpty()) return;
    bool hasOut = false; for (auto& d : outs) if (d.name == out) hasOut = true;
    if (! hasOut)
    {
        out = {};
        const auto want = in.replace ("MIDIIN2", "MIDIOUT2");
        for (auto& d : outs) if (d.name == want) out = d.name;
        if (out.isEmpty()) for (auto& d : outs) if (isApcControlPort (d.name)) { out = d.name; break; }
    }
    connect (in, out);
}

void ApcHub::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    { const juce::ScopedLock sl (lock); pending.push_back (m); }
    triggerAsyncUpdate();
}

void ApcHub::handleAsyncUpdate()
{
    std::vector<juce::MidiMessage> msgs;
    { const juce::ScopedLock sl (lock); msgs.swap (pending); }
    for (auto& m : msgs)
    {
        log.add (surface.describe (m));
        surface.handleIncoming (m);
    }
    while (log.size() > 200) log.remove (0);
}

void ApcHub::timerCallback()
{
    if (! isConnected()) { if (++ticks % 60 == 0) autoConnect(); return; }      // plugged in later: look every 3 s
    surface.updateFeedback();
}
