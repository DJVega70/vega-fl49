#pragma once
#include "DawSession.h"

/**
    Akai APC Key 25 mk2 controller script (its second, "control" MIDI port; the keys port plays the
    selected instrument track like any keyboard). Pure logic: feed it MIDI on the message thread and it
    calls `send` with LED feedback. Laid out like the Ableton script, but driving the DAW's clip launcher.

      5 x 8 pads           clip launcher: launch a clip. An empty slot on an instrument (or armed) track records a new clip;
                           press it again to stop recording and play it. Other empty slots stop that track.
                           Shift + pad selects the clip (opens it in the editor with a double tap).
      Scene buttons        launch the scene row. Shift + scene button = soft-key mode for the 8 track
                           buttons: CLIP STOP / SOLO / MUTE / REC ARM / SELECT.
      Track buttons 1-8    the soft-key mode's action on the 8 tracks shown.
                           Shift + arrows (buttons 1-4) moves the 8 x 5 window,
                           Shift + VOLUME / PAN / SEND / DEVICE sets what the knobs do.
      Knobs K1-K8          volume / pan / send to the first bus, for the 8 tracks shown,
                           or (DEVICE) the first 8 parameters of the selected track's plug-in.
      STOP ALL CLIPS       stops every launcher clip. Shift + STOP ALL = back to arrangement.
      Play / Record        transport. Shift + Play = back to the start.
      Shift + Record       drum pads on / off: the 40 pads play the selected instrument track
                           (GM drum notes from 36 up, bottom-left first), so they play Vega Productions Drums.
*/
class ApcSurface
{
public:
    explicit ApcSurface (DawSession&);
    std::function<void (const juce::MidiMessage&)> send;
    std::function<void (te::AudioTrack*)> onSelectTrack;
    std::function<void (te::Clip*)> onSelectClip, onOpenClip;
    // empty slot on an instrument (or armed) track: record a new clip into it; press it again to stop and play it
    std::function<bool (te::AudioTrack&, int scene)> onRecordSlot, isRecordingSlot;
    std::function<void()> onStopSlotRecording;
    std::function<void()> onWindowMoved;                     // the 8 x 5 box moved (the session view draws it)

    void handleIncoming (const juce::MidiMessage&);
    void updateFeedback (bool forceAll = false);             // call ~20x/sec
    void reset() { lastPad.fill (-1); lastLed.clear(); }
    void allOff();
    static juce::MidiMessage introduction();                 // Ableton-mode handshake sysex

    enum class SoftMode { clipStop, solo, mute, recArm, select };
    enum class KnobMode { volume, pan, send, device };
    SoftMode softMode = SoftMode::clipStop;
    KnobMode knobMode = KnobMode::volume;
    int trackOffset = 0, sceneOffset = 0;
    bool shift = false;
    bool drumPads = false;
    static constexpr int drumBaseNote = 36;
    te::EditItemID selectedID;                               // keep in sync with the UI selection

    juce::Array<te::AudioTrack*> columns() const;            // the tracks the launcher shows
    te::AudioTrack* trackForColumn (int col) const;
    static int nearestPaletteColour (juce::Colour);
    juce::String describe (const juce::MidiMessage&) const;

    static constexpr int padRows = 5, padCols = 8;
    static constexpr int trackButton0 = 0x40, sceneButton0 = 0x52, stopAllNote = 0x51, playNote = 0x5b, recordNote = 0x5d, shiftNote = 0x62, knobCc0 = 0x30;

private:
    void pad (int row, int col);
    void drumPad (int index, bool down);
    te::AudioTrack* instrumentTrack() const;
    void trackButton (int i);
    void sceneButton (int i);
    void knob (int i, int delta);
    void moveWindow (int dTracks, int dScenes);
    te::AutomatableParameter* knobParam (int i);
    void touch (te::AutomatableParameter*);
    void endGestures (bool all);
    DawSession& s;
    std::array<int, 40> lastPad {};
    std::array<bool, 40> padDown {};                          // (behaviour channel << 8) | colour, -1 = unknown
    std::map<int, int> lastLed;
    int blink = 0;
    struct Gesture { te::AutomatableParameter::Ptr param; juce::uint32 last = 0; };
    std::vector<Gesture> gestures;
    juce::uint32 lastPadTap = 0; int lastPadTapped = -1;
};
