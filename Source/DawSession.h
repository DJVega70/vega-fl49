#pragma once
#include <tracktion_engine/tracktion_engine.h>
#include "DJEQPlugin.h"
#include "MasterStripPlugin.h"
#include "TapeMachinePlugin.h"

namespace te = tracktion;
using namespace std::literals;

/** Runs engine tasks (rendering, etc.) to completion on the calling thread. */
inline void runEngineTaskBlocking (te::ThreadPoolJobWithProgress& job)
{
    while (job.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain) {}
}
/** The edit that hardware controllers / MIDI learn talk to (set whenever the session's edit changes) */
inline te::Edit* vegaFocusedEdit = nullptr;

struct HeadlessUIBehaviour : public te::UIBehaviour
{
    te::Edit* getCurrentlyFocusedEdit() override { return vegaFocusedEdit; }
    te::Edit* getLastFocusedEdit() override { return vegaFocusedEdit; }
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& j) override { runEngineTaskBlocking (j); }
};

/** Everything the DAW does to the engine, kept free of UI so it can be tested headless. */
class DawSession
{
public:
    enum class TrackKind { audio, midi, bus };

    explicit DawSession (te::Engine&);
    ~DawSession();

    te::Engine& engine;
    te::Edit& edit() { return *editPtr; }
    std::function<void()> onEditChanged, onBeforeEditChange;            // called after new / open (the Edit object was replaced)

    // ---- project files (.vegaproj = Tracktion edit XML)
    void newProject();
    bool openProject (const juce::File&);
    bool saveProject();                               // to the current file (false if untitled)
    bool saveProjectAs (const juce::File&);
    juce::File getProjectFile() const { return projectFile; }
    void setProjectFile (const juce::File& f) { projectFile = f; editPtr->editFileRetriever = [f] { return f; }; }   // after recovering an autosave
    bool hasUnsavedChanges() const;

    // ---- tracks
    te::AudioTrack* addTrack (TrackKind, const juce::String& name = {});
    static TrackKind kindOf (const te::Track&);
    juce::Array<te::AudioTrack*> getTracks (bool includeBuses = true) const;
    juce::Array<te::AudioTrack*> getBuses() const;                      // includes folders (they're buses too)
    void deleteTrack (te::Track&);
    void moveTrack (te::AudioTrack&, int newIndex);
    static juce::Colour trackColour (const te::Track&);

    // ---- routing
    void routeToBus (te::AudioTrack& source, te::AudioTrack* bus);     // nullptr = master
    te::AudioTrack* getOutputBus (te::AudioTrack&) const;
    void setSend (te::AudioTrack& source, te::AudioTrack& bus, float gainDb);   // creates send/return pair if needed
    void removeSend (te::AudioTrack& source, te::AudioTrack& bus);
    te::AuxSendPlugin* findSend (te::AudioTrack& source, te::AudioTrack& bus) const;
    int busNumberFor (te::AudioTrack& bus);

    // ---- plugins
    te::Plugin::Ptr addPlugin (te::AudioTrack&, const juce::String& type, const juce::PluginDescription* desc = nullptr, int index = -1);
    te::Plugin::Ptr setInstrument (te::AudioTrack&, const juce::String& type, const juce::PluginDescription* desc = nullptr);
    juce::Array<te::Plugin*> getInsertPlugins (te::AudioTrack&) const;
    VegaDJEQPlugin* djEq (te::AudioTrack&, bool create = true);
    VegaMasterStripPlugin* masterStrip (bool create = true);
    VegaTapeMachinePlugin* masterTape (bool create = true);                // the reel-to-reel on the master bus (before the master strip)               // EQ / dynamics on the master bus            // the channel's DJ EQ / filter (always present, sits before the fader)      // user plugins, excludes volume/meter/aux
    static juce::StringArray builtInEffectTypes();
    static juce::String builtInDisplayName (const juce::String& type);

    // ---- clips
    te::WaveAudioClip::Ptr insertAudioFile (te::AudioTrack&, const juce::File&, te::TimePosition at);
    te::MidiClip::Ptr insertMidiClip (te::AudioTrack&, te::TimePosition start, te::TimeDuration length);
    te::MidiClip::Ptr importMidiFile (te::AudioTrack&, const juce::File&, te::TimePosition at);
    void splitClip (te::Clip&, te::TimePosition);
    int sliceAtTransients (te::WaveAudioClip&, float sensitivity = 0.5f);    // splits the clip at detected hits, returns number of new clips
    static void setClipFades (te::AudioClipBase&, te::TimeDuration fadeIn, te::TimeDuration fadeOut);
    static void setClipWarp (te::WaveAudioClip&, bool followTempo, float semitones);   // time-stretch + pitch (SoundTouch)
    /** Elastic audio: warp markers (start / end markers are created). Turns on tempo-follow, since warp maps
        are applied by the tempo-synced playback path; an untagged file is treated as being at the project tempo. */
    static void setElastic (te::WaveAudioClip&, bool on);
    /** Put separated stems on new audio tracks, lined up with the clip they came from (or at 'at' for a whole file).
        Returns the new tracks. The original clip (if any) is muted. */
    juce::Array<te::AudioTrack*> placeStems (const juce::Array<juce::File>& stems, const juce::StringArray& names, const juce::String& baseName,
                                             te::WaveAudioClip* original, te::TimePosition at = {});

    // ---- transport / timing
    void setTempo (double bpm);
    double getTempo() const;
    void setTimeSig (int num, int den);
    te::TimeDuration barLength() const;
    te::TimePosition snap (te::TimePosition, int gridDivisionsPerBeat) const;

    void addTempoChange (te::TimePosition, double bpm);                     // tempo changes later in the song
    void addTimeSigChange (te::TimePosition, int num, int den);             // snapped to the bar
    te::MarkerClip* addMarker (te::TimePosition, const juce::String& name = {});

    // ---- folders: a folder is a bus with its member tracks routed into it, shown nested (and collapsible)
    te::AudioTrack* addFolder (const juce::String& name = {});
    static bool isFolder (const te::Track&);
    te::AudioTrack* folderOf (const te::AudioTrack&) const;
    juce::Array<te::AudioTrack*> folderMembers (const te::AudioTrack& folder) const;
    void moveToFolder (te::AudioTrack&, te::AudioTrack* folder);            // nullptr = take it out of its folder
    static bool isCollapsed (const te::Track& t) { return (bool) t.state.getProperty ("vegaCollapsed", false); }
    static void setCollapsed (te::Track& t, bool c) { t.state.setProperty ("vegaCollapsed", c, nullptr); }
    bool isHiddenByFolder (const te::AudioTrack&) const;                    // inside a collapsed folder
    te::AudioTrack* duplicateTrack (te::AudioTrack&);

    // ---- export
    struct ExportOptions
    {
        enum class Format { wav, aiff, flac, ogg, mp3 };
        enum class Normalise { none, peak, lufs };
        Format format = Format::wav;
        int bitDepth = 24;
        double sampleRate = 48000.0;
        Normalise normalise = Normalise::none;
        float peakDb = -0.3f, lufs = -14.0f;
        std::optional<te::TimeRange> range;            // default: the whole song
        double tailSeconds = 2.0;                      // let reverbs / delays ring out
        bool includeMaster = true;                     // master-bus processing (stems: off by default)
    };
    static juce::String extensionFor (ExportOptions::Format);
    /** Path of lame.exe / lame for MP3 export (MP3 encoding isn't built in). Empty if not found. */
    static juce::File& lameExecutable();
    static juce::File findLame();
    bool exportMix (const juce::File&, const ExportOptions&);
    bool exportWav (const juce::File&, std::optional<te::TimeRange> range = std::nullopt);
    /** One file per track (buses and folders included). Returns the files written. */
    juce::Array<juce::File> exportStems (const juce::File& folder, const ExportOptions&, std::function<bool (float progress, const juce::String& name)> progress = nullptr);
    /** Render a track (with its plug-ins) over the range to audio on a new track under it. The original is muted. */
    te::AudioTrack* bounceToNewTrack (te::AudioTrack&, te::TimeRange, bool withPlugins = true);
    /** Join every clip on the track that overlaps the range into one clip (audio is rendered without plug-ins, MIDI is merged). */
    te::Clip* consolidate (te::AudioTrack&, te::TimeRange);
    juce::File mediaFolder (const juce::String& sub) const;                  // next to the project, or in Documents
    te::TimeRange contentRange() const;

    // ---- takes / comping (loop recording puts every pass in one clip as takes)
    static juce::Array<juce::File> takeFiles (te::WaveAudioClip&);
    static int currentTake (te::WaveAudioClip&);
    static void setTake (te::WaveAudioClip&, int index);
    static void addTakes (te::WaveAudioClip&, const juce::Array<juce::File>&);
    /** Use take 'index' for the part of the clip inside 'range' (the clip is split at the range edges). Returns that piece. */
    te::WaveAudioClip* compTake (te::WaveAudioClip&, int index, te::TimeRange range);
    static void keepCurrentTakeOnly (te::WaveAudioClip&);
    juce::Array<te::AudioTrack*> unpackTakes (te::WaveAudioClip&);          // each take on its own new track

    // ---- phone sketches (Vega Sketch for Android): a .vegasketch zip with sketch.json, audio/ and renders/
    struct SketchImport
    {
        enum class Sounds { desktop, phone, both };
        Sounds sounds = Sounds::desktop;     // desktop instruments (MIDI) / the phone's own audio prints / both (prints muted)
        bool toArrangement = true;           // also lay the scenes out one after another in the arrangement
        bool adoptTempo = true;
    };
    struct SketchResult { bool ok = false; juce::String message, name; int tracks = 0, clips = 0, kits = 0; };
    SketchResult importSketch (const juce::File& sketchFile, const SketchImport&);
    /** A plug-in the scanner has found, by (part of) its name: e.g. "Vega Productions Drums" */
    const juce::PluginDescription* findPlugin (const juce::String& nameContains) const;

    // ---- clip launcher (session view)
    int numScenes() const;
    void ensureScenes (int);                                                // also gives every track that many slots
    void insertScene (int index);
    void deleteScene (int index);
    static te::ClipSlot* slot (te::AudioTrack&, int scene);
    static te::Clip* slotClip (te::AudioTrack&, int scene);
    te::MidiClip* createSlotMidiClip (te::AudioTrack&, int scene, int bars = 4);
    te::Clip* insertSlotFile (te::AudioTrack&, int scene, const juce::File&);
    te::Clip* copyToSlot (te::Clip&, te::AudioTrack&, int scene);
    te::Clip* copyToArrangement (te::Clip& slotClip, te::TimePosition at);
    void launchClip (te::Clip&);
    void stopTrackSlots (te::AudioTrack&);
    void launchScene (int scene);
    void stopAllSlots();
    void backToArrangement();
    static bool isSlotPlaying (te::Clip&);
    static bool isSlotQueued (te::Clip&);

private:
    void attachEdit (std::unique_ptr<te::Edit>);
    std::unique_ptr<te::Edit> editPtr;
    juce::File projectFile;
};
