#pragma once
#include "DawSession.h"
#include "Fl49Keys.h"

/**
    VEGA FL49: everything the FLkey 2 49's *keys* port does (the HUI "DAW" port is ControllerHub's job).

    The FLkey's MIDI port is listened to directly. Its notes go through the scale / chord brain and
    then into a virtual input, "VEGA FL49 Keys", which is what the selected instrument track listens
    to - so recordings contain exactly what you heard (snapped notes, full chords).
    The 16 pads are recognised by their channel + note (TEACH learns them) and, depending on the pad
    mode, play drums, launch clips or launch scenes.
*/
class Fl49Hub : private te::PhysicalMidiInputDevice::Listener, private juce::AsyncUpdater, private juce::Timer
{
public:
    enum class PadMode { drums, clips, scenes };
    static constexpr const char* virtualName = "VEGA FL49 Keys";

    Fl49Hub (te::Engine&, DawSession&);
    ~Fl49Hub() override;

    // ---- wiring (set by the main window)
    std::function<te::AudioTrack*()> getSelectedTrack;
    std::function<int()> getBankStart;                 // first track the faders / pads show (follows the HUI bank)
    std::function<void()> onChange;                     // something the panel shows changed

    // ---- state
    bool isConnected() const { return attached != nullptr; }
    juce::String keysPortName() const { return attachedName; }
    static bool isKeysPort (const juce::String& name);  // "FLkey 2 49 MIDI" (Windows), "FLkey 2 49 MIDI Out" (Mac); never the DAW port
    te::MidiInputDevice* virtualInput() const;

    PadMode padMode() const { return (PadMode) padModeValue.load(); }
    void setPadMode (PadMode);
    int sceneOffset = 0;
    void moveScenes (int delta);

    fl49::Settings keySettings() const { return processor.getSettings(); }
    void setKeySettings (const fl49::Settings&);       // saved in the project
    void loadKeySettingsFromEdit();
    void panic();
    /** Play a message into "VEGA FL49 Keys" as if it came from the keyboard (on-screen wheels, pads) */
    void sendToKeys (const juce::MidiMessage& m) { send (m); }

    // ---- pads
    void startTeach();                                  // the next 16 pad hits become pads 1-16
    void cancelTeach();
    bool isTeaching() const { return teachIndex.load() >= 0; }
    int teachStep() const { return teachIndex.load(); }
    void resetPads();
    fl49::PadMap pads() const { const juce::SpinLock::ScopedLockType sl (padLock); return padMap; }
    void pressPad (int pad, int velocity);              // a pad hit, from the hardware or the screen
    std::array<juce::uint32, 16> padHitTime {};         // message thread, for the panel's flash
    std::array<int, 16> padVelocity {};

    // ---- knobs = plug-in macros
    int macroPage = 0;                                  // 8 parameters per page
    int macroSlot = 0;                                  // 0 = the instrument (else the first effect), 1.. = effect n
    te::Plugin* macroPlugin();
    juce::Array<te::Plugin*> macroCandidates();
    te::AutomatableParameter* macroParam (int knob);    // knob 0-7 on the current page
    int macroParamCount();

    void editChanged() { loadKeySettingsFromEdit(); sceneOffset = 0; if (onChange) onChange(); }
    juce::StringArray log;                              // shown in the panel's info line

private:
    void handleIncomingMidiMessage (const juce::MidiMessage&) override;   // MIDI thread
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void attach();
    void detach();
    void send (const juce::MidiMessage&);
    void saveSettings();
    void loadSettings();
    juce::File settingsFile() const;

    te::Engine& engine; DawSession& session;
    std::shared_ptr<te::MidiInputDevice> attached;      // the FLkey keys port we listen to
    juce::String attachedName;
    std::shared_ptr<te::MidiInputDevice> virtualDev;
    fl49::NoteProcessor processor; juce::SpinLock processorLock;     // MIDI thread + panic / settings changes
    std::atomic<te::VirtualMidiInputDevice*> virtualRaw { nullptr };   // what the MIDI thread sends to
    fl49::PadMap padMap; juce::SpinLock padLock;
    std::atomic<int> padModeValue { (int) PadMode::drums }, teachIndex { -1 };
    struct Event { int pad, velocity; bool teach; int channel, note; };
    juce::AbstractFifo fifo { 256 }; std::array<Event, 256> events {};
};
