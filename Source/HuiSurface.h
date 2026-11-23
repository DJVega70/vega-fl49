#pragma once
#include "DawSession.h"

/**
    Mackie HUI control-surface script (used by the Novation FLkey 2 series on its "DAW" port,
    and by many other controllers). Pure logic: feed it incoming MIDI on the message thread,
    and it calls `send` with the feedback (faders, LEDs, rings, names, ping).

    Mapping
      faders 1-8           -> volume of the 8 tracks in the current bank
      encoders (V-pots)    -> pan, or send A-E level when a send mode is chosen
      strip buttons        -> select / mute / solo / record-arm
      bank & channel <- -> -> move the 8-track window
      transport            -> rewind / fast-forward (1 bar), stop, play, record, loop, return to zero
      undo / save / click  -> undo, save, metronome
*/
class HuiSurface
{
public:
    explicit HuiSurface (DawSession&);
    std::function<void (const juce::MidiMessage&)> send;      // to the controller
    std::function<void (te::AudioTrack*)> onSelectTrack;       // UI selection
    std::function<void()> onSave;

    void handleIncoming (const juce::MidiMessage&);           // message thread
    void updateFeedback (bool forceAll = false);              // call ~10-20x/sec
    void sendPing() { if (send) send (juce::MidiMessage::noteOn (1, 0, (juce::uint8) 0)); }
    void reset() { last = {}; }

    int bankStart = 0;
    te::EditItemID selectedID;                                // keep in sync with the UI selection
    enum class EncMode { pan, sendA, sendB, sendC, sendD, sendE, plugin } encMode = EncMode::pan;
    /** plugin mode (VEGA FL49): the encoders turn these 8 parameters (the selected track's plug-in macros) */
    std::function<te::AutomatableParameter* (int knob)> macroParam;
    std::function<void (int knob)> onKnobTouched;            // for the on-screen FLkey panel
    te::AudioTrack* trackForStrip (int strip) const;
    juce::String describe (const juce::MidiMessage&) const;  // human readable, for the monitor

    static float faderPosFromDb (float db) { return te::decibelsToVolumeFaderPosition (db); }
    static float dbFromFaderPos (float p)   { return te::volumeFaderPositionToDB (p); }

private:
    void pressed (int zone, int port, bool down);
    void switchLed (int zone, int port, bool on);
    void moveBank (int delta);
    te::AuxSendPlugin* sendFor (te::AudioTrack&, int sendIndex) const;
    DawSession& s;
    int zone = -1;
    std::array<int, 8> faderMsb {};                          // waiting for LSB
    struct Fb { std::array<int, 8> fader { -1,-1,-1,-1,-1,-1,-1,-1 }, ring { -1,-1,-1,-1,-1,-1,-1,-1 };
                std::map<int, bool> leds; std::array<juce::String, 8> names; } last;
    juce::uint32 lastTouch[8] {};
    struct Gesture { te::AutomatableParameter::Ptr param; juce::uint32 last; };
    std::vector<Gesture> gestures;                            // plug-in knob turns, one gesture each (automation write)
    void touch (te::AutomatableParameter*);
    void endGestures (bool all = false);
};
