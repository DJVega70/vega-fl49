#pragma once
#include "HuiSurface.h"
#include "ApcSurface.h"
#include "VegaUI.h"

/** Owns the hardware connection for the HUI surface (e.g. Novation FLkey 2 "DAW" port), a ping timer and a MIDI monitor. */
class ControllerHub : private juce::MidiInputCallback, private juce::AsyncUpdater, private juce::Timer
{
public:
    ControllerHub (te::Engine&, DawSession&);
    ~ControllerHub() override;

    HuiSurface surface;
    bool connect (const juce::String& inName, const juce::String& outName, bool userChoice = false);   // empty = disconnect
    void autoConnect (bool forceAuto = false);                              // finds an FLkey / HUI "DAW" port
    bool isConnected() const { return input != nullptr; }
    juce::String inputName, outputName;
    static bool looksLikeDawPort (const juce::String& name);

    // monitor (message thread)
    juce::StringArray log; std::function<void()> onLog;
    std::function<void (te::AudioTrack*)> onSelectTrack;
    std::function<void()> onSave;
    void editChanged() { surface.reset(); surface.bankStart = 0; surface.updateFeedback (true); }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    juce::File settingsFile() const;
    void releaseFromEngine (const juce::String& inName, const juce::String& outName);
    te::Engine& engine; DawSession& session;
    std::unique_ptr<juce::MidiInput> input; std::unique_ptr<juce::MidiOutput> output;
    juce::CriticalSection lock; std::vector<juce::MidiMessage> pending;
    int ticks = 0;
};

/** Options > Controllers dialog content */
class ControllerPanel : public juce::Component, private juce::Timer
{
public:
    explicit ControllerPanel (ControllerHub&);
    ~ControllerPanel() override { hub.onLog = nullptr; }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { refresh(); }
    void refresh();
    void fillLists();
    ControllerHub& hub;
    juce::ComboBox inBox, outBox; juce::TextButton connectBtn, autoBtn, clearBtn; juce::TextEditor monitor; juce::Label status, help;
    int lastLogSize = -1;
};

/** Owns the connection to an Akai APC Key 25 mk2's control port (pads, buttons, knobs, LEDs).
    Its keys port is left as a normal MIDI input, so it plays the selected instrument track. */
class ApcHub : private juce::MidiInputCallback, private juce::AsyncUpdater, private juce::Timer
{
public:
    ApcHub (te::Engine&, DawSession&);
    ~ApcHub() override;
    ApcSurface surface;
    bool connect (const juce::String& inName, const juce::String& outName, bool userChoice = false);
    void autoConnect (bool forceAuto = false);
    bool isConnected() const { return input != nullptr; }
    juce::String inputName, outputName, lastError;
    static bool isApcControlPort (const juce::String& name);   // "MIDIIN2 (APC Key 25 mk2)" (Windows), "APC Key 25 mk2 Control" (Mac)
    void editChanged() { surface.trackOffset = surface.sceneOffset = 0; surface.updateFeedback (true); }
    juce::StringArray log;
private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    juce::File settingsFile() const;
    te::Engine& engine;
    std::unique_ptr<juce::MidiInput> input; std::unique_ptr<juce::MidiOutput> output;
    juce::CriticalSection lock; std::vector<juce::MidiMessage> pending;
    int ticks = 0;
};
