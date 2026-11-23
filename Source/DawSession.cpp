#include <map>
#include <set>
#include "DawSession.h"

static const juce::Identifier kindProp ("vegaKind");
static const juce::Identifier colourProp ("vegaColour");
static const juce::Identifier busNumProp ("vegaBusNumber");

DawSession::DawSession (te::Engine& e) : engine (e)
{
    static std::set<te::Engine*> registered;   // built-in types must be registered once per engine, before any edit loads
    // control surfaces (Mackie MCU...) see the DAW's audio / instrument / bus tracks, in order
    e.getExternalControllerManager().isVisibleOnControlSurface = [] (const te::Track& t) { return dynamic_cast<const te::AudioTrack*> (&t) != nullptr; };
    if (registered.insert (&e).second) { e.getPluginManager().createBuiltInType<VegaDJEQPlugin>(); e.getPluginManager().createBuiltInType<VegaMasterStripPlugin>(); e.getPluginManager().createBuiltInType<VegaTapeMachinePlugin>(); }
    newProject();
}

VegaMasterStripPlugin* DawSession::masterStrip (bool create)
{
    auto& list = editPtr->getMasterPluginList();
    for (auto* p : list.getPlugins()) if (auto* m = dynamic_cast<VegaMasterStripPlugin*> (p)) return m;
    if (! create) return nullptr;
    auto p = editPtr->getPluginCache().createNewPlugin (VegaMasterStripPlugin::xmlTypeName, {});
    if (p == nullptr) return nullptr;
    list.insertPlugin (p, 0, nullptr);
    return dynamic_cast<VegaMasterStripPlugin*> (p.get());
}

VegaTapeMachinePlugin* DawSession::masterTape (bool create)
{
    auto& list = editPtr->getMasterPluginList();
    for (auto* p : list.getPlugins()) if (auto* m = dynamic_cast<VegaTapeMachinePlugin*> (p)) return m;
    if (! create) return nullptr;
    auto p = editPtr->getPluginCache().createNewPlugin (VegaTapeMachinePlugin::xmlTypeName, {});
    if (p == nullptr) return nullptr;
    p->setEnabled (false);                      // starts bypassed: switching it on is your call
    list.insertPlugin (p, 0, nullptr);          // tape first, then the master strip's EQ / dynamics
    return dynamic_cast<VegaTapeMachinePlugin*> (p.get());
}

VegaDJEQPlugin* DawSession::djEq (te::AudioTrack& t, bool create)
{
    for (auto* p : t.pluginList.getPlugins()) if (auto* d = dynamic_cast<VegaDJEQPlugin*> (p)) return d;
    if (! create) return nullptr;
    auto p = editPtr->getPluginCache().createNewPlugin (VegaDJEQPlugin::xmlTypeName, {});
    if (p == nullptr) return nullptr;
    int index = t.pluginList.size();
    for (int i = 0; i < t.pluginList.size(); ++i)
        if (dynamic_cast<te::VolumeAndPanPlugin*> (t.pluginList[i]) != nullptr) { index = i; break; }
    t.pluginList.insertPlugin (p, index, nullptr);
    return dynamic_cast<VegaDJEQPlugin*> (p.get());
}
DawSession::~DawSession()
{
    if (editPtr) editPtr->getTransport().stop (false, true);
    editPtr.reset();
}

void DawSession::attachEdit (std::unique_ptr<te::Edit> e)
{
    if (editPtr) { if (onBeforeEditChange) onBeforeEditChange(); editPtr->getTransport().stop (false, true); }
    editPtr = std::move (e);
    vegaFocusedEdit = editPtr.get();
    editPtr->playInStopEnabled = true;
    for (auto* t : te::getAudioTracks (*editPtr)) djEq (*t);    // older projects get the DJ section too
    masterStrip();
    masterTape();
    for (auto* t : te::getAudioTracks (*editPtr))          // elastic clips play in real time (the proxy setting isn't saved)
        for (auto* c : t->getClips())
            if (auto* ac = dynamic_cast<te::WaveAudioClip*> (c); ac != nullptr && ac->getWarpTime()) ac->setUsesProxy (false);
    editPtr->getTransport().ensureContextAllocated();
    if (onEditChanged) onEditChanged();
}

void DawSession::newProject()
{
    projectFile = juce::File();
    auto tmp = engine.getTemporaryFileManager().getTempDirectory().getChildFile ("Untitled.vegaproj");
    tmp.deleteFile();
    auto e = te::createEmptyEdit (engine, tmp);
    e->tempoSequence.getTempos()[0]->setBpm (124.0);
    // a fresh session: one audio track, one MIDI track with the built-in synth
    attachEdit (std::move (e));
    // createEmptyEdit may already contain tracks; normalise to our starting layout
    for (auto* t : te::getAudioTracks (*editPtr)) editPtr->deleteTrack (t);
    addTrack (TrackKind::audio, "Audio 1");
    addTrack (TrackKind::midi, "Synth 1");
    editPtr->resetChangedStatus();
}

bool DawSession::openProject (const juce::File& f)
{
    if (! f.existsAsFile()) return false;
    auto e = te::loadEditFromFile (engine, f);
    if (e == nullptr) return false;
    projectFile = f;
    attachEdit (std::move (e));
    return true;
}

bool DawSession::saveProjectAs (const juce::File& f)
{
    auto target = f.withFileExtension ("vegaproj");
    te::EditFileOperations ops (*editPtr);
    if (! ops.writeToFile (target, false)) return false;
    editPtr->editFileRetriever = [target] { return target; };
    projectFile = target;
    editPtr->resetChangedStatus();
    return true;
}
bool DawSession::saveProject() { return projectFile != juce::File() && saveProjectAs (projectFile); }
bool DawSession::hasUnsavedChanges() const { return editPtr && editPtr->hasChangedSinceSaved(); }

//==============================================================================
DawSession::TrackKind DawSession::kindOf (const te::Track& t)
{
    const auto k = t.state.getProperty (kindProp).toString();
    return k == "bus" ? TrackKind::bus : k == "midi" ? TrackKind::midi : TrackKind::audio;
}

juce::Colour DawSession::trackColour (const te::Track& t)
{
    if (t.state.hasProperty (colourProp)) return juce::Colour::fromString (t.state.getProperty (colourProp).toString());
    return juce::Colour (0xff48e1e8);
}

te::AudioTrack* DawSession::addTrack (TrackKind kind, const juce::String& name)
{
    auto& ed = *editPtr;
    auto existing = te::getAudioTracks (ed);
    te::Track* last = existing.isEmpty() ? nullptr : existing.getLast();
    auto t = ed.insertNewAudioTrack (te::TrackInsertPoint (nullptr, last), nullptr);
    if (t == nullptr) return nullptr;
    static const juce::uint32 palette[] { 0xff48e1e8, 0xffe8b448, 0xff7ee87a, 0xffe8708a, 0xffa58bff, 0xff6fb7ff, 0xffffa25e, 0xff5ee8b6 };
    int count = 0; for (auto* other : getTracks (true)) if (kindOf (*other) == kind) ++count;
    const juce::String kindName = kind == TrackKind::bus ? "Bus" : kind == TrackKind::midi ? "Synth" : "Audio";
    t->setName (name.isNotEmpty() ? name : kindName + " " + juce::String (count));
    t->state.setProperty (kindProp, kind == TrackKind::bus ? "bus" : kind == TrackKind::midi ? "midi" : "audio", nullptr);
    t->state.setProperty (colourProp, juce::Colour (palette[(size_t) (existing.size() % 8)]).toString(), nullptr);
    djEq (*t);
    if (kind == TrackKind::midi) { setInstrument (*t, te::FourOscPlugin::xmlTypeName); if (auto vp = t->getVolumePlugin()) vp->setVolumeDb (-10.0f); }
    if (kind == TrackKind::bus)
    {
        t->state.setProperty (busNumProp, -1, nullptr);
        busNumberFor (*t);
    }
    return t.get();
}

juce::Array<te::AudioTrack*> DawSession::getTracks (bool includeBuses) const
{
    juce::Array<te::AudioTrack*> r;
    for (auto* t : te::getAudioTracks (*editPtr)) if (includeBuses || kindOf (*t) != TrackKind::bus) r.add (t);
    return r;
}
juce::Array<te::AudioTrack*> DawSession::getBuses() const
{
    juce::Array<te::AudioTrack*> r;
    for (auto* t : te::getAudioTracks (*editPtr)) if (kindOf (*t) == TrackKind::bus) r.add (t);
    return r;
}

void DawSession::deleteTrack (te::Track& t)
{
    if (auto* at = dynamic_cast<te::AudioTrack*> (&t); at != nullptr && isFolder (*at))
        for (auto* m : folderMembers (*at)) m->state.removeProperty ("vegaFolderOf", nullptr);
    if (auto* at = dynamic_cast<te::AudioTrack*> (&t); at != nullptr && kindOf (*at) == TrackKind::bus)
        for (auto* other : getTracks (true))
        {
            if (getOutputBus (*other) == at) routeToBus (*other, nullptr);
            removeSend (*other, *at);
        }
    editPtr->deleteTrack (&t);
}

void DawSession::moveTrack (te::AudioTrack& t, int newIndex)
{
    auto tracks = getTracks (true);
    auto members = isFolder (t) ? folderMembers (t) : juce::Array<te::AudioTrack*>();
    tracks.removeFirstMatchingValue (&t);
    for (auto* m : members) tracks.removeFirstMatchingValue (m);
    newIndex = juce::jlimit (0, tracks.size(), newIndex);
    if (newIndex > 0) editPtr->moveTrack (&t, te::TrackInsertPoint (nullptr, tracks[newIndex - 1]));
    else if (! tracks.isEmpty() && t.state.getParent() == tracks[0]->state.getParent())
    {   // before the first track (TrackInsertPoint can't express "first" reliably)
        auto parent = t.state.getParent();
        const int from = parent.indexOf (t.state), to = parent.indexOf (tracks[0]->state);
        parent.moveChild (from, from < to ? to - 1 : to, &editPtr->getUndoManager());
    }
    te::Track* prev = &t;
    for (auto* m : members) { editPtr->moveTrack (m, te::TrackInsertPoint (nullptr, prev)); prev = m; }
}

//==============================================================================
void DawSession::routeToBus (te::AudioTrack& source, te::AudioTrack* bus)
{
    if (bus == &source) return;
    if (bus != nullptr) source.getOutput().setOutputToTrack (bus);
    else source.getOutput().setOutputToDefaultDevice (false);
}

te::AudioTrack* DawSession::getOutputBus (te::AudioTrack& t) const { return t.getOutput().getDestinationTrack(); }

int DawSession::busNumberFor (te::AudioTrack& bus)
{
    int n = bus.state.getProperty (busNumProp, -1);
    if (n >= 0) return n;
    juce::Array<int> used; for (auto* b : getBuses()) used.add ((int) b->state.getProperty (busNumProp, -1));
    n = 0; while (used.contains (n)) ++n;
    bus.state.setProperty (busNumProp, n, nullptr);
    // the return that receives sends lives first on the bus
    bool hasReturn = false;
    for (auto* p : bus.pluginList.getPlugins()) if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (p)) { r->busNumber = n; hasReturn = true; }
    if (! hasReturn)
        if (auto p = editPtr->getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}))
        {
            if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (p.get())) r->busNumber = n;
            bus.pluginList.insertPlugin (p, 0, nullptr);
        }
    return n;
}

te::AuxSendPlugin* DawSession::findSend (te::AudioTrack& source, te::AudioTrack& bus) const
{
    const int n = bus.state.getProperty (busNumProp, -1);
    for (auto* p : source.pluginList.getPlugins()) if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p)) if (s->getBusNumber() == n) return s;
    return nullptr;
}

void DawSession::setSend (te::AudioTrack& source, te::AudioTrack& bus, float gainDb)
{
    if (&source == &bus) return;
    const int n = busNumberFor (bus);
    auto* s = findSend (source, bus);
    if (s == nullptr)
    {
        auto p = editPtr->getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
        if (p == nullptr) return;
        // post-fader: place just before the volume plugin's successor (end of list, before the meter)
        int idx = source.pluginList.size();
        for (int i = 0; i < source.pluginList.size(); ++i) if (dynamic_cast<te::LevelMeterPlugin*> (source.pluginList[i])) { idx = i; break; }
        source.pluginList.insertPlugin (p, idx, nullptr);
        s = dynamic_cast<te::AuxSendPlugin*> (p.get());
        if (s == nullptr) return;
        s->busNumber = n;
    }
    s->setGainDb (gainDb);
}

void DawSession::removeSend (te::AudioTrack& source, te::AudioTrack& bus)
{
    if (auto* s = findSend (source, bus)) s->removeFromParent();
}

//==============================================================================
te::Plugin::Ptr DawSession::addPlugin (te::AudioTrack& t, const juce::String& type, const juce::PluginDescription* desc, int index)
{
    auto p = desc != nullptr ? editPtr->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, *desc)
                             : editPtr->getPluginCache().createNewPlugin (type, {});
    if (p == nullptr) return {};
    if (index < 0)
    {   // inserts go before the volume/pan fader
        index = t.pluginList.size();
        for (int i = 0; i < t.pluginList.size(); ++i)
            if (dynamic_cast<te::VolumeAndPanPlugin*> (t.pluginList[i]) != nullptr || dynamic_cast<VegaDJEQPlugin*> (t.pluginList[i]) != nullptr) { index = i; break; }
    }
    t.pluginList.insertPlugin (p, index, nullptr);
    return p;
}

te::Plugin::Ptr DawSession::setInstrument (te::AudioTrack& t, const juce::String& type, const juce::PluginDescription* desc)
{
    // remove an existing synth in slot 0
    if (auto* first = t.pluginList[0]; first != nullptr && first->isSynth()) first->removeFromParent();
    return addPlugin (t, type, desc, 0);
}

juce::Array<te::Plugin*> DawSession::getInsertPlugins (te::AudioTrack& t) const
{
    juce::Array<te::Plugin*> r;
    for (auto* p : t.pluginList.getPlugins())
    {
        if (dynamic_cast<te::VolumeAndPanPlugin*> (p) || dynamic_cast<te::LevelMeterPlugin*> (p)
            || dynamic_cast<te::AuxSendPlugin*> (p) || dynamic_cast<te::AuxReturnPlugin*> (p) || dynamic_cast<VegaDJEQPlugin*> (p)) continue;
        r.add (p);
    }
    return r;
}

juce::StringArray DawSession::builtInEffectTypes()
{
    return { te::EqualiserPlugin::xmlTypeName, te::CompressorPlugin::xmlTypeName, te::ReverbPlugin::xmlTypeName, te::DelayPlugin::xmlTypeName,
             te::ChorusPlugin::xmlTypeName, te::PhaserPlugin::xmlTypeName, te::LowPassPlugin::xmlTypeName, te::PitchShiftPlugin::xmlTypeName };
}
juce::String DawSession::builtInDisplayName (const juce::String& type)
{
    if (type == te::FourOscPlugin::xmlTypeName) return "Vega 4-Osc Synth";
    if (type == te::SamplerPlugin::xmlTypeName) return "Vega Basic Sampler";
    if (type == te::EqualiserPlugin::xmlTypeName) return "4-Band EQ";
    if (type == te::CompressorPlugin::xmlTypeName) return "Compressor";
    if (type == te::ReverbPlugin::xmlTypeName) return "Reverb";
    if (type == te::DelayPlugin::xmlTypeName) return "Delay";
    if (type == te::ChorusPlugin::xmlTypeName) return "Chorus";
    if (type == te::PhaserPlugin::xmlTypeName) return "Phaser";
    if (type == te::LowPassPlugin::xmlTypeName) return "Filter";
    if (type == te::PitchShiftPlugin::xmlTypeName) return "Pitch Shift";
    return type;
}

//==============================================================================
te::WaveAudioClip::Ptr DawSession::insertAudioFile (te::AudioTrack& t, const juce::File& f, te::TimePosition at)
{
    te::AudioFile af (engine, f);
    if (! af.isValid()) return {};
    return t.insertWaveClip (f.getFileNameWithoutExtension(), f, { { at, te::TimeDuration::fromSeconds (af.getLength()) }, {} }, false);
}

te::MidiClip::Ptr DawSession::insertMidiClip (te::AudioTrack& t, te::TimePosition start, te::TimeDuration length)
{
    return t.insertMIDIClip ({ start, start + length }, nullptr);
}

te::MidiClip::Ptr DawSession::importMidiFile (te::AudioTrack& t, const juce::File& f, te::TimePosition at)
{
    juce::FileInputStream in (f); juce::MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) return {};
    mf.convertTimestampTicksToSeconds();
    juce::MidiMessageSequence all;
    for (int i = 0; i < mf.getNumTracks(); ++i) all.addSequence (*mf.getTrack (i), 0);
    all.updateMatchedPairs();
    const double len = std::max (all.getEndTime(), 0.5);
    auto clip = insertMidiClip (t, at, te::TimeDuration::fromSeconds (std::ceil (len / barLength().inSeconds()) * barLength().inSeconds()));
    if (clip == nullptr) return {};
    auto& ts = editPtr->tempoSequence;
    for (int i = 0; i < all.getNumEvents(); ++i)
    {
        auto* ev = all.getEventPointer (i);
        if (! ev->message.isNoteOn() || ev->noteOffObject == nullptr) continue;
        const auto startB = ts.toBeats (at + te::TimeDuration::fromSeconds (ev->message.getTimeStamp())) - ts.toBeats (at);
        const auto endB = ts.toBeats (at + te::TimeDuration::fromSeconds (ev->noteOffObject->message.getTimeStamp())) - ts.toBeats (at);
        clip->getSequence().addNote (ev->message.getNoteNumber(), te::BeatPosition::fromBeats (startB.inBeats()), te::BeatDuration::fromBeats (std::max (0.01, endB.inBeats() - startB.inBeats())),
                                     ev->message.getVelocity(), 0, nullptr);
    }
    return clip;
}

void DawSession::splitClip (te::Clip& c, te::TimePosition t)
{
    if (auto* track = dynamic_cast<te::ClipTrack*> (c.getTrack()))
        if (c.getPosition().time.contains (t) && t > c.getPosition().getStart()) track->splitClip (c, t);
}

int DawSession::sliceAtTransients (te::WaveAudioClip& clip, float sens)
{
    std::unique_ptr<juce::AudioFormatReader> r (engine.getAudioFileFormatManager().readFormatManager.createReaderFor (clip.getOriginalFile()));
    if (r == nullptr) return 0;
    const double ratio = clip.getSpeedRatio();
    const auto pos = clip.getPosition();
    const double srcStart = pos.getOffset().inSeconds() * ratio, srcLen = pos.getLength().inSeconds() * ratio;
    const auto first = (juce::int64) (srcStart * r->sampleRate), count = (juce::int64) (srcLen * r->sampleRate);
    if (count < 2048) return 0;
    juce::AudioBuffer<float> b ((int) std::min<juce::uint32> (2, r->numChannels), (int) count);
    r->read (&b, 0, (int) count, first, true, true);
    const int hop = 256, win = 512;
    std::vector<float> e;
    for (int s = 0; s + win <= b.getNumSamples(); s += hop) { double a = 0; for (int i = 0; i < win; ++i) { const float x = b.getSample (0, s + i); a += x * x; } e.push_back ((float) (a / win)); }
    float peak = 0; for (float v : e) peak = std::max (peak, v);
    const float ratioThr = 1.5f + (1.0f - sens) * 18.0f, floorE = peak * std::pow (10.0f, -(20.0f + sens * 30.0f) / 10.0f);
    std::vector<double> cuts; double last = 0;
    for (size_t k = 4; k < e.size(); ++k)
    {
        const float prev = (e[k - 2] + e[k - 3] + e[k - 4]) / 3.0f + 1.0e-10f;
        if (e[k] > floorE && e[k] / prev > ratioThr && e[k] >= e[k - 1])
        {
            const double t = (double) (k * hop - hop / 2) / r->sampleRate;
            if (t - last > 0.07 && t > 0.03) { cuts.push_back (t); last = t; }
        }
    }
    // split from the end so earlier positions stay valid
    te::Clip* current = &clip; int made = 0;
    auto* track = dynamic_cast<te::ClipTrack*> (clip.getTrack());
    if (track == nullptr) return 0;
    edit().getUndoManager().beginNewTransaction ("Slice at transients");
    for (auto it = cuts.rbegin(); it != cuts.rend(); ++it)
    {
        const auto at = pos.getStart() + te::TimeDuration::fromSeconds (*it / ratio);
        if (current->getPosition().time.contains (at)) { track->splitClip (*current, at); ++made; }
    }
    return made;
}

void DawSession::setClipFades (te::AudioClipBase& c, te::TimeDuration in, te::TimeDuration out)
{
    c.setFadeIn (in); c.setFadeOut (out);
}

void DawSession::setClipWarp (te::WaveAudioClip& c, bool followTempo, float semitones)
{
    c.setTimeStretchMode (te::TimeStretcher::soundtouchBetter);
    c.setAutoTempo (followTempo);
    c.setPitchChange (semitones);
}

//==============================================================================
void DawSession::setTempo (double bpm) { editPtr->tempoSequence.getTempos()[0]->setBpm (juce::jlimit (20.0, 400.0, bpm)); }
double DawSession::getTempo() const { return editPtr->tempoSequence.getTempos()[0]->getBpm(); }
void DawSession::setTimeSig (int num, int den)
{
    auto* ts = editPtr->tempoSequence.getTimeSig (0);
    ts->numerator = juce::jlimit (1, 16, num); ts->denominator = den;
}
te::TimeDuration DawSession::barLength() const
{
    auto* ts = editPtr->tempoSequence.getTimeSig (0);
    return te::TimeDuration::fromSeconds (60.0 / getTempo() * ts->numerator * 4.0 / ts->denominator);
}
te::TimePosition DawSession::snap (te::TimePosition t, int div) const
{
    if (div <= 0) return t;
    auto& ts = editPtr->tempoSequence;
    const double b = ts.toBeats (t).inBeats();
    const double snapped = std::round (b * div) / div;
    return ts.toTime (te::BeatPosition::fromBeats (std::max (0.0, snapped)));
}

te::TimeRange DawSession::contentRange() const
{
    te::TimePosition end;
    for (auto* t : te::getAudioTracks (*editPtr)) for (auto* c : t->getClips()) end = std::max (end, c->getPosition().getEnd());
    return { te::TimePosition(), end };
}

//============================================================================== export / bounce
juce::String DawSession::extensionFor (ExportOptions::Format f)
{
    switch (f)
    {
        case ExportOptions::Format::aiff: return "aif";
        case ExportOptions::Format::flac: return "flac";
        case ExportOptions::Format::ogg:  return "ogg";
        case ExportOptions::Format::mp3:  return "mp3";
        default:                          return "wav";
    }
}

juce::File& DawSession::lameExecutable() { static juce::File f; return f; }

juce::File DawSession::findLame()
{
    if (lameExecutable().existsAsFile()) return lameExecutable();
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (auto& f : { exeDir.getChildFile ("lame.exe"), exeDir.getChildFile ("lame"),
                     juce::File ("/opt/homebrew/bin/lame"), juce::File ("/usr/local/bin/lame"), juce::File ("/usr/bin/lame") })
        if (f.existsAsFile()) return lameExecutable() = f;
    return {};
}

juce::File DawSession::mediaFolder (const juce::String& sub) const
{
    auto base = projectFile != juce::File() ? projectFile.getParentDirectory()
                                            : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Productions");
    auto d = base.getChildFile (sub);
    d.createDirectory();
    return d;
}

namespace
{
    /** Fill in format / quality / normalising from the export options. Returns false if the format isn't available. */
    bool applyFormat (te::Engine& engine, te::Renderer::Parameters& p, const DawSession::ExportOptions& o, std::unique_ptr<juce::AudioFormat>& owned)
    {
        using F = DawSession::ExportOptions::Format;
        auto& afm = engine.getAudioFileFormatManager();
        p.bitDepth = o.bitDepth; p.quality = 0;
        switch (o.format)
        {
            case F::aiff: p.audioFormat = afm.getAiffFormat(); break;
            case F::flac: p.audioFormat = afm.getFlacFormat(); p.bitDepth = std::min (24, o.bitDepth); break;
            case F::ogg:
            {
                p.audioFormat = afm.getOggFormat(); p.bitDepth = 16;
                const auto q = p.audioFormat->getQualityOptions();
                p.quality = std::max (0, q.size() - 3);          // high VBR quality (~256 kbps)
                break;
            }
            case F::mp3:      // rendered to WAV, then encoded by lame (see encodeMp3)
                juce::ignoreUnused (owned);
                p.audioFormat = afm.getWavFormat(); p.bitDepth = 24;
                break;
            default: p.audioFormat = afm.getWavFormat(); break;
        }
        p.sampleRateForAudio = o.sampleRate;
        p.blockSizeForAudio = 512;
        p.ditheringEnabled = p.bitDepth < 24;
        p.shouldNormalise = o.normalise == DawSession::ExportOptions::Normalise::peak;
        p.shouldNormaliseByLUFS = o.normalise == DawSession::ExportOptions::Normalise::lufs;
        p.normaliseToLevelDb = p.shouldNormaliseByLUFS ? o.lufs : o.peakDb;
        p.limitTruePeak = true; p.truePeakCeilingDb = -1.0f;
        p.canRenderInMono = false;
        p.checkNodesForAudio = false;
        p.usePlugins = true;
        return p.audioFormat != nullptr;
    }

    int trackIndex (te::Edit& ed, te::Track& t) { return te::getAllTracks (ed).indexOf (&t); }

    /** 320 kbps MP3 from a rendered WAV, using the lame encoder. The WAV is deleted afterwards. */
    bool encodeMp3 (const juce::File& wav, const juce::File& mp3)
    {
        const auto lame = DawSession::findLame();
        if (! lame.existsAsFile()) { wav.deleteFile(); return false; }
        mp3.deleteFile();
        juce::ChildProcess cp;
        juce::StringArray args { lame.getFullPathName(), "--silent", "-b", "320", "-q", "2", wav.getFullPathName(), mp3.getFullPathName() };
        const bool ok = cp.start (args, 0) && cp.waitForProcessToFinish (30 * 60 * 1000) && cp.getExitCode() == 0 && mp3.existsAsFile();
        wav.deleteFile();
        return ok;
    }
}

bool DawSession::exportMix (const juce::File& f, const ExportOptions& o)
{
    auto r = o.range.value_or (contentRange());
    if (r.getLength() <= 0s) return false;
    r = r.withEnd (r.getEnd() + te::TimeDuration::fromSeconds (o.tailSeconds));
    juce::BigInteger tracksToDo; tracksToDo.setRange (0, te::getAllTracks (*editPtr).size(), true);
    te::Renderer::Parameters p (*editPtr);
    std::unique_ptr<juce::AudioFormat> owned;
    if (! applyFormat (engine, p, o, owned)) return false;
    const auto dest = f.withFileExtension (extensionFor (o.format));
    p.destFile = o.format == ExportOptions::Format::mp3 ? dest.withFileExtension ("render.wav") : dest;
    p.time = r; p.tracksToDo = tracksToDo; p.useMasterPlugins = o.includeMaster;
    p.destFile.deleteFile();
    if (! te::Renderer::renderToFile ("Export", p).existsAsFile()) return false;
    return o.format != ExportOptions::Format::mp3 || encodeMp3 (p.destFile, dest);
}

bool DawSession::exportWav (const juce::File& f, std::optional<te::TimeRange> range)
{
    ExportOptions o; o.range = range;
    return exportMix (f, o);
}

juce::Array<juce::File> DawSession::exportStems (const juce::File& folder, const ExportOptions& o, std::function<bool (float, const juce::String&)> progress)
{
    juce::Array<juce::File> done;
    auto r = o.range.value_or (contentRange());
    if (r.getLength() <= 0s) return done;
    r = r.withEnd (r.getEnd() + te::TimeDuration::fromSeconds (o.tailSeconds));
    folder.createDirectory();
    auto tracks = getTracks (true);
    for (int i = 0; i < tracks.size(); ++i)
    {
        auto* t = tracks[i];
        if (progress && ! progress ((float) i / (float) tracks.size(), t->getName())) break;
        te::Renderer::Parameters p (*editPtr);
        std::unique_ptr<juce::AudioFormat> owned;
        if (! applyFormat (engine, p, o, owned)) break;
        juce::BigInteger todo;
        if (kindOf (*t) == TrackKind::bus)
        {   // a bus / folder stem is only what comes out of it: everything feeding it runs silently
            todo.setBit (trackIndex (*editPtr, *t));
            for (auto* src : tracks)
                if (src != t && (getOutputBus (*src) == t || findSend (*src, *t) != nullptr))
                    p.tracksToProcessWhileMuted.add (src->itemID);
        }
        else
            todo.setBit (trackIndex (*editPtr, *t));
        // a track (or bus) routed into a bus is heard through that bus
        for (auto* b = getOutputBus (*t); b != nullptr && ! todo[trackIndex (*editPtr, *b)]; b = getOutputBus (*b))
            todo.setBit (trackIndex (*editPtr, *b));
        juce::String name = juce::String (i + 1).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (t->getName());
        const auto dest = folder.getChildFile (name + "." + extensionFor (o.format));
        p.destFile = o.format == ExportOptions::Format::mp3 ? dest.withFileExtension ("render.wav") : dest;
        p.time = r; p.tracksToDo = todo; p.useMasterPlugins = o.includeMaster;
        p.destFile.deleteFile();
        if (te::Renderer::renderToFile ("Stem: " + t->getName(), p).existsAsFile()
             && (o.format != ExportOptions::Format::mp3 || encodeMp3 (p.destFile, dest)))
            done.add (dest);
    }
    if (progress) progress (1.0f, {});
    return done;
}

te::AudioTrack* DawSession::bounceToNewTrack (te::AudioTrack& t, te::TimeRange range, bool withPlugins)
{
    if (range.getLength() <= 0s) return nullptr;
    auto file = mediaFolder ("Bounces").getNonexistentChildFile (juce::File::createLegalFileName (t.getName()) + " bounce", ".wav", false);
    te::Renderer::Parameters p (*editPtr);
    p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 24; p.sampleRateForAudio = 48000.0; p.blockSizeForAudio = 512;
    p.destFile = file; p.time = range; p.usePlugins = withPlugins; p.useMasterPlugins = false;
    p.checkNodesForAudio = false; p.canRenderInMono = false;
    p.tracksToDo.setBit (trackIndex (*editPtr, t));
    if (! te::Renderer::renderToFile ("Bounce", p).existsAsFile()) return nullptr;
    editPtr->getUndoManager().beginNewTransaction ("Bounce in place");
    auto* nt = addTrack (TrackKind::audio, t.getName() + " (bounce)");
    if (nt == nullptr) return nullptr;
    moveTrack (*nt, getTracks (true).indexOf (&t) + 1);
    nt->state.setProperty ("vegaColour", t.state.getProperty ("vegaColour"), nullptr);
    if (withPlugins) routeToBus (*nt, getOutputBus (t));
    else if (auto* f = folderOf (t)) moveToFolder (*nt, f);
    insertAudioFile (*nt, file, range.getStart());
    t.setMute (true);
    return nt;
}

te::Clip* DawSession::consolidate (te::AudioTrack& t, te::TimeRange range)
{
    // grow the range to whole clips it touches
    te::Clip::Array hit;
    for (auto* c : t.getClips())
        if (c->getPosition().time.overlaps (range)) { hit.add (c); range = range.getUnionWith (c->getPosition().time); }
    if (hit.isEmpty()) return nullptr;
    auto* um = &editPtr->getUndoManager();
    um->beginNewTransaction ("Consolidate");
    bool allMidi = true;
    for (auto* c : hit) if (dynamic_cast<te::MidiClip*> (c) == nullptr) allMidi = false;
    if (allMidi)
    {
        struct N { int note, vel; double start, len; };
        std::vector<N> notes;
        auto& ts = editPtr->tempoSequence;
        const double base = ts.toBeats (range.getStart()).inBeats();
        for (auto* c : hit)
        {
            auto* mc = static_cast<te::MidiClip*> (c);
            const double clipStartBeat = ts.toBeats (mc->getPosition().getStart()).inBeats() - mc->getOffsetInBeats().inBeats();
            const double s0 = ts.toBeats (mc->getPosition().getStart()).inBeats(), s1 = ts.toBeats (mc->getPosition().getEnd()).inBeats();
            for (auto* n : mc->getSequence().getNotes())
            {
                const double ns = clipStartBeat + n->getStartBeat().inBeats();
                if (ns < s0 - 1e-6 || ns >= s1 || mc->isMuted()) continue;
                notes.push_back ({ n->getNoteNumber(), n->getVelocity(), ns - base, std::min (n->getLengthBeats().inBeats(), s1 - ns) });
            }
        }
        const auto name = hit.getFirst()->getName();
        for (auto* c : hit) c->removeFromParent();
        auto mc = t.insertMIDIClip (name, range, nullptr);
        if (mc == nullptr) return nullptr;
        for (auto& n : notes) mc->getSequence().addNote (n.note, te::BeatPosition::fromBeats (n.start), te::BeatDuration::fromBeats (std::max (0.01, n.len)), n.vel, 0, um);
        return mc.get();
    }
    auto file = mediaFolder ("Bounces").getNonexistentChildFile (juce::File::createLegalFileName (t.getName()) + " consolidated", ".wav", false);
    te::Renderer::Parameters p (*editPtr);
    p.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
    p.bitDepth = 24; p.sampleRateForAudio = 48000.0; p.blockSizeForAudio = 512;
    p.destFile = file; p.time = range; p.usePlugins = false; p.useMasterPlugins = false;
    p.checkNodesForAudio = false; p.canRenderInMono = false;
    p.tracksToDo.setBit (trackIndex (*editPtr, t));
    for (auto* c : hit) p.allowedClips.add (c);
    if (! te::Renderer::renderToFile ("Consolidate", p).existsAsFile()) return nullptr;
    const auto name = hit.getFirst()->getName();
    for (auto* c : hit) c->removeFromParent();
    auto nc = t.insertWaveClip (name, file, { range, {} }, false);
    return nc.get();
}

//============================================================================== takes / comping
juce::Array<juce::File> DawSession::takeFiles (te::WaveAudioClip& c)
{
    juce::Array<juce::File> r;
    auto takes = c.state.getChildWithName (te::IDs::TAKES);
    for (auto take : takes)
    {
        if (take.getProperty (te::IDs::isComp, false)) continue;
        r.add (te::SourceFileReference::findFileFromString (c.edit, take.getProperty (te::IDs::source).toString()));
    }
    return r;
}

int DawSession::currentTake (te::WaveAudioClip& c)
{
    const auto cur = c.getSourceFileReference().getFile();
    auto files = takeFiles (c);
    for (int i = 0; i < files.size(); ++i) if (files[i] == cur) return i;
    return -1;
}

void DawSession::setTake (te::WaveAudioClip& c, int index)
{
    auto files = takeFiles (c);
    if (! juce::isPositiveAndBelow (index, files.size()) || currentTake (c) == index) return;
    c.getSourceFileReference().setToFile (files[index], te::SourceFileReference::PathStyle::chooseBest, false);
}

void DawSession::addTakes (te::WaveAudioClip& c, const juce::Array<juce::File>& files)
{
    for (auto& f : files) c.addTake (f);
}

te::WaveAudioClip* DawSession::compTake (te::WaveAudioClip& clip, int index, te::TimeRange range)
{
    auto* track = dynamic_cast<te::ClipTrack*> (clip.getTrack());
    if (track == nullptr) return nullptr;
    const auto pos = clip.getPosition().time;
    range = range.getIntersectionWith (pos);
    if (range.getLength() < 0.005s) return nullptr;
    editPtr->getUndoManager().beginNewTransaction ("Comp");
    te::Clip* piece = &clip;
    const auto xf = 0.008s;      // tiny fades at the joins so the switch doesn't click
    if (range.getEnd() < pos.getEnd() - 0.005s)
        if (auto* right = track->splitClip (*piece, range.getEnd()))
            if (auto* rc = dynamic_cast<te::AudioClipBase*> (right)) rc->setFadeIn (xf);
    if (range.getStart() > pos.getStart() + 0.005s)
        if (auto* right = track->splitClip (*piece, range.getStart()))
        {
            if (auto* lc = dynamic_cast<te::AudioClipBase*> (piece)) lc->setFadeOut (xf);
            piece = right;
        }
    auto* w = dynamic_cast<te::WaveAudioClip*> (piece);
    if (w == nullptr) return nullptr;
    if (range.getStart() > pos.getStart() + 0.005s) w->setFadeIn (xf);
    if (range.getEnd() < pos.getEnd() - 0.005s) w->setFadeOut (xf);
    setTake (*w, index);
    return w;
}

void DawSession::keepCurrentTakeOnly (te::WaveAudioClip& c)
{
    auto takes = c.state.getChildWithName (te::IDs::TAKES);
    if (takes.isValid()) c.state.removeChild (takes, &c.edit.getUndoManager());
}

juce::Array<te::AudioTrack*> DawSession::unpackTakes (te::WaveAudioClip& c)
{
    juce::Array<te::AudioTrack*> made;
    auto* src = dynamic_cast<te::AudioTrack*> (c.getTrack());
    if (src == nullptr) return made;
    auto files = takeFiles (c);
    const auto pos = c.getPosition();
    int at = getTracks (true).indexOf (src) + 1;
    editPtr->getUndoManager().beginNewTransaction ("Takes to tracks");
    for (int i = 0; i < files.size(); ++i)
    {
        auto* t = addTrack (TrackKind::audio, src->getName() + " take " + juce::String (i + 1));
        if (t == nullptr) continue;
        moveTrack (*t, at++);
        t->state.setProperty ("vegaColour", src->state.getProperty ("vegaColour"), nullptr);
        if (auto nc = insertAudioFile (*t, files[i], pos.getStart())) nc->setPosition (pos);
        t->setMute (i != currentTake (c));
        made.add (t);
    }
    return made;
}

//============================================================================== clip launcher
int DawSession::numScenes() const { return editPtr->getSceneList().getNumScenes(); }
void DawSession::ensureScenes (int n) { editPtr->getSceneList().ensureNumberOfScenes (std::max (n, numScenes())); }
void DawSession::insertScene (int index) { editPtr->getSceneList().insertScene (juce::jlimit (0, numScenes(), index)); }
void DawSession::deleteScene (int index)
{
    if (auto* s = editPtr->getSceneList().getScenes()[index]) editPtr->getSceneList().deleteScene (*s);
}
te::ClipSlot* DawSession::slot (te::AudioTrack& t, int scene) { return t.getClipSlotList().getClipSlots()[scene]; }
te::Clip* DawSession::slotClip (te::AudioTrack& t, int scene) { auto* s = slot (t, scene); return s != nullptr ? s->getClip() : nullptr; }

te::MidiClip* DawSession::createSlotMidiClip (te::AudioTrack& t, int scene, int bars)
{
    ensureScenes (scene + 1);
    auto* s = slot (t, scene);
    if (s == nullptr) return nullptr;
    if (auto* old = s->getClip()) old->removeFromParent();
    const auto len = editPtr->tempoSequence.toTime (te::BeatPosition::fromBeats (bars * editPtr->tempoSequence.getTimeSig (0)->numerator.get()));
    auto mc = te::insertMIDIClip (*s, { te::TimePosition(), len });
    if (mc == nullptr) return nullptr;
    te::prepareClipForLauncher (*mc);
    return mc.get();
}

te::Clip* DawSession::insertSlotFile (te::AudioTrack& t, int scene, const juce::File& f)
{
    ensureScenes (scene + 1);
    auto* s = slot (t, scene);
    if (s == nullptr) return nullptr;
    if (auto* old = s->getClip()) old->removeFromParent();
    te::Clip* c = nullptr;
    if (f.hasFileExtension (te::midiFileExtensions))
        c = te::createClipFromFile (f, *s, false).get();
    else
    {
        te::AudioFile af (engine, f);
        if (! af.isValid()) return nullptr;
        c = te::insertWaveClip (*s, f.getFileNameWithoutExtension(), f, { { te::TimePosition(), te::TimeDuration::fromSeconds (af.getLength()) } }, te::DeleteExistingClips::yes).get();
    }
    if (c != nullptr) te::prepareClipForLauncher (*c);
    return c;
}

te::Clip* DawSession::copyToSlot (te::Clip& src, te::AudioTrack& t, int scene)
{
    ensureScenes (scene + 1);
    auto* s = slot (t, scene);
    if (s == nullptr) return nullptr;
    if (auto* old = s->getClip()) old->removeFromParent();
    auto state = src.state.createCopy();
    te::EditItemID::remapIDs (state, nullptr, *editPtr);
    auto* c = te::insertClipWithState (*s, state);
    if (c != nullptr) te::prepareClipForLauncher (*c);
    return c;
}

te::Clip* DawSession::copyToArrangement (te::Clip& sc, te::TimePosition at)
{
    auto* s = sc.getClipSlot();
    if (s == nullptr) return nullptr;
    auto* t = dynamic_cast<te::AudioTrack*> (&s->track);
    if (t == nullptr) return nullptr;
    auto state = sc.state.createCopy();
    te::EditItemID::remapIDs (state, nullptr, *editPtr);
    auto* c = t->insertClipWithState (state);
    if (c != nullptr) c->setStart (at, false, true);
    return c;
}

namespace
{
    te::MonotonicBeat launchPosition (te::Edit& e, te::Clip* c)
    {
        auto epc = e.getTransport().getCurrentPlaybackContext();
        if (! epc) return {};
        auto sp = epc->getSyncPoint();
        if (! sp) return {};
        te::LaunchQuantisation* lq = &e.getLaunchQuantisation();
        if (c != nullptr && ! c->usesGlobalLaunchQuatisation()) if (auto* clq = c->getLaunchQuantisation()) lq = clq;
        auto q = sp->beat;
        for (int guard = 0; guard < 4; ++guard) { q = lq->getNext (q); if (q >= te::BeatPosition()) break; q = q + te::BeatDuration::fromBeats (0.001); }
        return { sp->monotonicBeat.v + (q - sp->beat) };
    }
    void startTransportForLaunch (te::Edit& e)
    {
        auto& tc = e.getTransport();
        if (tc.isPlaying()) return;
        auto& ts = e.tempoSequence;
        const auto bb = ts.toBarsAndBeats (tc.getPosition());
        tc.setPosition (ts.toTime (te::tempo::BarsAndBeats { bb.bars, {} }));
        if (auto epc = tc.getCurrentPlaybackContext()) epc->blockUntilSyncPointChange();
    }
}

bool DawSession::isSlotPlaying (te::Clip& c)
{
    auto lh = c.getLaunchHandle();
    return lh != nullptr && lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing;
}
bool DawSession::isSlotQueued (te::Clip& c)
{
    auto lh = c.getLaunchHandle();
    return lh != nullptr && lh->getQueuedStatus().has_value();
}

void DawSession::launchClip (te::Clip& c)
{
    auto* s = c.getClipSlot();
    auto* t = s != nullptr ? dynamic_cast<te::AudioTrack*> (&s->track) : nullptr;
    if (t == nullptr) return;
    auto& tc = editPtr->getTransport();
    const bool wasPlaying = tc.isPlaying();
    tc.ensureContextAllocated();
    startTransportForLaunch (*editPtr);
    for (auto* other : t->getClipSlotList().getClipSlots())
        if (auto* oc = other != nullptr ? other->getClip() : nullptr; oc != nullptr && oc != &c)
            if (auto lh = oc->getLaunchHandle())
                lh->stop (wasPlaying ? std::make_optional (launchPosition (*editPtr, oc)) : std::nullopt);
    if (auto lh = c.getLaunchHandle()) lh->play (wasPlaying ? std::make_optional (launchPosition (*editPtr, &c)) : std::nullopt);
    if (! wasPlaying) tc.play (false);
}

void DawSession::stopTrackSlots (te::AudioTrack& t)
{
    const bool playing = editPtr->getTransport().isPlaying();
    for (auto* s : t.getClipSlotList().getClipSlots())
        if (auto* c = s != nullptr ? s->getClip() : nullptr)
            if (auto lh = c->getLaunchHandle())
                lh->stop (playing ? std::make_optional (launchPosition (*editPtr, c)) : std::nullopt);
}

void DawSession::launchScene (int scene)
{
    for (auto* t : getTracks (true))
    {
        if (auto* c = slotClip (*t, scene)) launchClip (*c);
        else stopTrackSlots (*t);
    }
}

void DawSession::stopAllSlots() { for (auto* t : getTracks (true)) stopTrackSlots (*t); }

void DawSession::backToArrangement()
{
    for (auto* t : getTracks (true))
    {
        for (auto* s : t->getClipSlotList().getClipSlots())
            if (auto* c = s != nullptr ? s->getClip() : nullptr)
                if (auto lh = c->getLaunchHandle()) lh->stop (std::nullopt);
        t->playSlotClips = false;
    }
}

//============================================================================== tempo / markers
void DawSession::addTempoChange (te::TimePosition at, double bpm)
{
    auto& ts = editPtr->tempoSequence;
    if (at <= 0.001s) { setTempo (bpm); return; }
    if (auto t = ts.insertTempo (at)) t->setBpm (juce::jlimit (20.0, 400.0, bpm));
}

void DawSession::addTimeSigChange (te::TimePosition at, int num, int den)
{
    auto& ts = editPtr->tempoSequence;
    const auto bb = ts.toBarsAndBeats (at);
    const auto barStart = ts.toTime (te::tempo::BarsAndBeats { bb.bars + (bb.beats.inBeats() > 0.01 ? 1 : 0), {} });
    if (barStart <= 0.001s) { setTimeSig (num, den); return; }
    if (auto s = ts.insertTimeSig (barStart)) { s->numerator = juce::jlimit (1, 16, num); s->denominator = den; }
}

te::MarkerClip* DawSession::addMarker (te::TimePosition at, const juce::String& name)
{
    auto& mm = editPtr->getMarkerManager();
    int next = 1; for (auto& m : mm.getMarkers()) next = std::max (next, m->getMarkerID() + 1);
    auto m = mm.createMarker (next, at, barLength(), nullptr);
    if (m != nullptr && name.isNotEmpty()) m->setName (name);
    return dynamic_cast<te::MarkerClip*> (m.get());
}

//============================================================================== folders
static const juce::Identifier folderProp ("vegaFolder"), folderOfProp ("vegaFolderOf");

bool DawSession::isFolder (const te::Track& t) { return (bool) t.state.getProperty (folderProp, false); }

te::AudioTrack* DawSession::addFolder (const juce::String& name)
{
    int count = 1; for (auto* t : getTracks (true)) if (isFolder (*t)) ++count;
    auto* f = addTrack (TrackKind::bus, name.isNotEmpty() ? name : "Folder " + juce::String (count));
    if (f != nullptr) f->state.setProperty (folderProp, true, nullptr);
    return f;
}

te::AudioTrack* DawSession::folderOf (const te::AudioTrack& t) const
{
    const auto id = t.state.getProperty (folderOfProp).toString();
    if (id.isEmpty()) return nullptr;
    for (auto* f : getTracks (true)) if (isFolder (*f) && f->itemID.toString() == id) return f;
    return nullptr;
}

juce::Array<te::AudioTrack*> DawSession::folderMembers (const te::AudioTrack& folder) const
{
    juce::Array<te::AudioTrack*> r;
    for (auto* t : getTracks (true)) if (t != &folder && folderOf (*t) == &folder) r.add (t);
    return r;
}

bool DawSession::isHiddenByFolder (const te::AudioTrack& t) const
{
    auto* f = folderOf (t);
    return f != nullptr && isCollapsed (*f);
}

void DawSession::moveToFolder (te::AudioTrack& t, te::AudioTrack* folder)
{
    if (isFolder (t) || folder == &t) return;
    auto* current = folderOf (t);
    if (current == folder) return;
    auto* um = &editPtr->getUndoManager();
    if (folder == nullptr)
    {
        auto members = folderMembers (*current);
        t.state.removeProperty (folderOfProp, um);
        if (getOutputBus (t) == current) routeToBus (t, nullptr);
        // put it just after the folder's last member
        auto tracks = getTracks (true);
        tracks.removeFirstMatchingValue (&t);
        int idx = tracks.indexOf (current);
        for (auto* m : members) if (m != &t) idx = std::max (idx, tracks.indexOf (m));
        moveTrack (t, idx + 1);
        return;
    }
    auto members = folderMembers (*folder);
    t.state.setProperty (folderOfProp, folder->itemID.toString(), um);
    routeToBus (t, folder);
    auto tracks = getTracks (true);
    tracks.removeFirstMatchingValue (&t);
    int idx = tracks.indexOf (folder);
    for (auto* m : members) if (m != &t) idx = std::max (idx, tracks.indexOf (m));
    moveTrack (t, idx + 1);
}

te::AudioTrack* DawSession::duplicateTrack (te::AudioTrack& t)
{
    if (isFolder (t)) return nullptr;
    auto state = t.state.createCopy();
    te::EditItemID::remapIDs (state, nullptr, *editPtr);
    auto nt = editPtr->insertTrack (te::TrackInsertPoint (nullptr, &t), state, nullptr);
    auto* at = dynamic_cast<te::AudioTrack*> (nt.get());
    if (at == nullptr) return nullptr;
    at->setName (t.getName() + " copy");
    if (kindOf (t) == TrackKind::bus) { at->state.setProperty (busNumProp, -1, nullptr); busNumberFor (*at); }
    return at;
}

juce::Array<te::AudioTrack*> DawSession::placeStems (const juce::Array<juce::File>& stems, const juce::StringArray& names, const juce::String& baseName,
                                                     te::WaveAudioClip* original, te::TimePosition at)
{
    static const std::map<juce::String, juce::uint32> stemColour {
        { "Vocals", 0xffe8708a }, { "Drums", 0xffffa25e }, { "Bass", 0xff7ee87a }, { "Other", 0xff6fb7ff }, { "Guitar", 0xffe8b448 }, { "Piano", 0xffa58bff } };
    juce::Array<te::AudioTrack*> made;
    // put the stems right after the original clip's track (or at the end)
    for (int i = 0; i < stems.size(); ++i)
    {
        auto* t = addTrack (TrackKind::audio, baseName + " " + names[i]);
        if (t == nullptr) continue;
        if (auto it = stemColour.find (names[i]); it != stemColour.end()) t->state.setProperty ("vegaColour", juce::Colour (it->second).toString(), nullptr);
        auto c = insertAudioFile (*t, stems[i], original != nullptr ? original->getPosition().getStart() : at);
        if (c != nullptr && original != nullptr)
        {
            c->setPosition (original->getPosition());          // same start, length and offset into the file
            if (original->getAutoTempo() || std::abs (original->getPitchChange()) > 0.01f) setClipWarp (*c, original->getAutoTempo(), original->getPitchChange());
            c->setGainDB (original->getGainDB());
        }
        made.add (t);
    }
    if (original != nullptr && ! made.isEmpty()) original->setMuted (true);
    return made;
}

void DawSession::setElastic (te::WaveAudioClip& ac, bool on)
{
    if (! on) { ac.setWarpTime (false); ac.setUsesProxy (true); return; }
    ac.setUsesProxy (false);      // warp markers are applied by real-time playback, not the pre-rendered proxy
    auto& li = ac.getLoopInfo();
    if (li.getNumBeats() <= 0 || li.getBpm (ac.getWaveInfo()) <= 0)
    {
        auto pos = te::createPosition (ac.edit.tempoSequence);
        pos.set (ac.getPosition().getStart());
        li.setBpm (pos.getTempo(), ac.getWaveInfo());
    }
    setClipWarp (ac, true, ac.getPitchChange());
    ac.setWarpTime (true);
    auto& wtm = ac.getWarpTimeManager();     // Tracktion creates the start / end markers itself
    if (wtm.getMarkers().isEmpty())
    {
        const auto len = wtm.getSourceLength();
        wtm.insertMarker (te::WarpMarker (te::TimePosition(), te::TimePosition()));
        wtm.insertMarker (te::WarpMarker (te::TimePosition() + len, te::TimePosition() + len));
    }
}

//============================================================================== phone sketches
const juce::PluginDescription* DawSession::findPlugin (const juce::String& part) const
{
    static juce::PluginDescription found;
    for (auto& d : engine.getPluginManager().knownPluginList.getTypes())
        if (d.name.containsIgnoreCase (part) && d.isInstrument) { found = d; return &found; }
    return nullptr;
}

DawSession::SketchResult DawSession::importSketch (const juce::File& file, const SketchImport& opt)
{
    SketchResult res;
    // unpack next to the project (or in Documents/Vega Productions)
    juce::File dir;
    if (file.isDirectory()) dir = file;
    else
    {
        juce::ZipFile zip (file);
        if (zip.getNumEntries() == 0) { res.message = "That isn't a Vega Sketch file."; return res; }
        dir = mediaFolder ("Phone sketches").getNonexistentChildFile (juce::File::createLegalFileName (file.getFileNameWithoutExtension()), {}, false);
        dir.createDirectory();
        if (zip.uncompressTo (dir, true).failed()) { res.message = "Couldn't unpack the sketch."; return res; }
    }
    auto json = juce::JSON::parse (dir.getChildFile ("sketch.json"));
    if (json.getProperty ("format", {}).toString() != "vega-sketch") { res.message = "sketch.json is missing - is this a Vega Sketch export?"; return res; }
    res.name = json.getProperty ("name", "Phone sketch").toString();
    const double bpm = (double) json.getProperty ("bpm", 120.0);
    const int bpb = juce::jlimit (2, 12, (int) json.getProperty ("beatsPerBar", 4));
    auto& um = editPtr->getUndoManager();
    um.beginNewTransaction ("Import phone sketch");
    if (opt.adoptTempo) { setTempo (bpm); setTimeSig (bpb, 4); }

    // scenes
    auto* scenesArr = json.getProperty ("scenes", {}).getArray();
    const int sceneCount = scenesArr != nullptr ? scenesArr->size() : 8;
    const int sceneBase = [this] { int used = 0; for (auto* t : getTracks (true)) for (int i = 0; i < numScenes(); ++i) if (slotClip (*t, i) != nullptr) used = std::max (used, i + 1); return used; }();
    ensureScenes (sceneBase + std::max (8, sceneCount));
    if (scenesArr != nullptr)
        for (int i = 0; i < scenesArr->size(); ++i)
            if (auto* sc = editPtr->getSceneList().getScenes()[sceneBase + i]) sc->name = (*scenesArr)[i].toString();

    struct Placed { te::Clip* clip; int scene; double beats; };
    std::vector<Placed> placed;
    auto* tracksArr = json.getProperty ("tracks", {}).getArray();
    if (tracksArr == nullptr || tracksArr->isEmpty()) { res.message = "The sketch has no tracks."; return res; }
    auto colourOf = [] (const juce::var& t) { return juce::Colour::fromString ("ff" + t.getProperty ("colour", "#6fb7ff").toString().removeCharacters ("#")); };
    auto gainDb = [] (const juce::var& t) { return juce::Decibels::gainToDecibels ((float) (double) t.getProperty ("volume", 0.8), -60.0f); };

    for (auto& tv : *tracksArr)
    {
        const auto type = tv.getProperty ("type", "keys").toString();
        const auto name = tv.getProperty ("name", "Track").toString();
        auto* clips = tv.getProperty ("clips", {}).getArray();
        const bool audioTrack = type == "audio";
        const bool wantMidi = ! audioTrack && opt.sounds != SketchImport::Sounds::phone;
        const bool wantPrint = ! audioTrack && opt.sounds != SketchImport::Sounds::desktop;
        auto setup = [&] (te::AudioTrack* t, bool muted)
        {
            t->state.setProperty ("vegaColour", (juce::int64) colourOf (tv).getARGB(), nullptr);
            if (auto vp = t->getVolumePlugin()) { vp->setVolumeDb (gainDb (tv) - (audioTrack ? 0.0f : 6.0f)); vp->setPan ((float) (double) tv.getProperty ("pan", 0.0)); }
            t->setMute (muted || (bool) tv.getProperty ("mute", false));
            ++res.tracks;
        };
        if (audioTrack || wantPrint)
        {
            auto* t = addTrack (TrackKind::audio, audioTrack ? name : name + " (phone)");
            if (t == nullptr) continue;
            setup (t, wantMidi && wantPrint);
            if (clips != nullptr)
                for (auto& cv : *clips)
                {
                    const auto rel = (audioTrack ? cv.getProperty ("audio", {}) : cv.getProperty ("render", {})).toString();
                    const auto f = dir.getChildFile (rel);
                    if (rel.isEmpty() || ! f.existsAsFile()) continue;
                    const int sc = sceneBase + (int) cv.getProperty ("scene", 0);
                    if (auto* c = insertSlotFile (*t, sc, f))
                    {
                        c->setName (cv.getProperty ("name", "Clip").toString());
                        const double beats = (double) cv.getProperty ("lengthBeats", 4.0);
                        c->setLoopRange ({ te::TimePosition(), editPtr->tempoSequence.toTime (te::BeatPosition::fromBeats (beats)) });
                        placed.push_back ({ c, sc, beats }); ++res.clips;
                    }
                }
        }
        if (wantMidi)
        {
            auto* t = addTrack (TrackKind::midi, name);
            if (t == nullptr) continue;
            const bool drums = type == "drums";
            const auto kit = tv.getProperty ("kit", {});
            bool kitLoaded = false;
            if (drums && kit.isObject())
            {
                // the phone's sample kit (808, LinnDrum, ... or the user's own) travels in the export:
                // load the same one-shots into a sampler, one pad per note, playing to their end like a drum machine
                if (auto p = setInstrument (*t, te::SamplerPlugin::xmlTypeName))
                    if (auto* sp = dynamic_cast<te::SamplerPlugin*> (p.get()))
                        if (auto* pads = kit.getProperty ("pads", {}).getArray())
                            for (auto& pv : *pads)
                            {
                                const auto f = dir.getChildFile (pv.getProperty ("file", {}).toString());
                                const int note = juce::jlimit (0, 127, (int) pv.getProperty ("note", 36));
                                if (! f.existsAsFile() || sp->addSound (f.getFullPathName(), pv.getProperty ("name", "Pad").toString(), 0.0, 0.0, 0.0f).isNotEmpty()) continue;
                                const int i = sp->getNumSounds() - 1;
                                sp->setSoundParams (i, note, note, note);
                                sp->setSoundOpenEnded (i, true);
                                kitLoaded = true;
                            }
                if (kitLoaded) ++res.kits;
            }
            if (! kitLoaded)
                if (auto* d = findPlugin (drums ? "Vega Productions Drums" : "Vega Productions Multisynth")) setInstrument (*t, te::ExternalPlugin::xmlTypeName, d);
            setup (t, false);
            if (clips != nullptr)
                for (auto& cv : *clips)
                {
                    const int sc = sceneBase + (int) cv.getProperty ("scene", 0);
                    const int bars = std::max (1, (int) cv.getProperty ("bars", 1));
                    auto* mc = createSlotMidiClip (*t, sc, bars);
                    if (mc == nullptr) continue;
                    mc->setName (cv.getProperty ("name", "Clip").toString());
                    const double beats = bars * (double) bpb;
                    mc->setLoopRangeBeats ({ te::BeatPosition(), te::BeatDuration::fromBeats (beats) });
                    if (auto* notes = cv.getProperty ("notes", {}).getArray())
                        for (auto& n : *notes)
                            if (auto* a = n.getArray(); a != nullptr && a->size() >= 4)
                                mc->getSequence().addNote ((int) (*a)[0], te::BeatPosition::fromBeats ((double) (*a)[1]), te::BeatDuration::fromBeats (std::max (0.02, (double) (*a)[2])),
                                                           juce::jlimit (1, 127, (int) (*a)[3]), 0, nullptr);
                    placed.push_back ({ mc, sc, beats }); ++res.clips;
                }
        }
    }

    // the arrangement: scene after scene, each as long as its longest clip, with a marker for each
    if (opt.toArrangement)
    {
        auto& ts = editPtr->tempoSequence;
        double at = 0;
        for (auto* t : getTracks (true)) for (auto* c : t->getClips()) at = std::max (at, ts.toBeats (c->getPosition().getEnd()).inBeats());
        at = std::ceil (at / bpb) * bpb;
        for (int sc = sceneBase; sc < sceneBase + sceneCount; ++sc)
        {
            double len = 0;
            for (auto& p : placed) if (p.scene == sc) len = std::max (len, p.beats);
            if (len <= 0) continue;
            juce::String sname = "Scene " + juce::String (sc - sceneBase + 1);
            if (auto* s2 = editPtr->getSceneList().getScenes()[sc]) if (s2->name.get().isNotEmpty()) sname = s2->name.get();
            addMarker (ts.toTime (te::BeatPosition::fromBeats (at)), sname);
            for (auto& p : placed)
                if (p.scene == sc)
                    if (auto* c = copyToArrangement (*p.clip, ts.toTime (te::BeatPosition::fromBeats (at))))
                    {
                        if (dynamic_cast<te::MidiClip*> (c) != nullptr) c->setLoopRangeBeats ({ te::BeatPosition(), te::BeatDuration::fromBeats (p.beats) });
                        else c->setLoopRange ({ te::TimePosition(), ts.toTime (te::BeatPosition::fromBeats (p.beats)) });
                        c->setPosition ({ { ts.toTime (te::BeatPosition::fromBeats (at)), ts.toTime (te::BeatPosition::fromBeats (at + len)) }, te::TimeDuration() });
                    }
            at += len;
        }
    }
    res.ok = res.clips > 0 || res.tracks > 0;
    res.message = "Imported \"" + res.name + "\": " + juce::String (res.tracks) + " tracks, " + juce::String (res.clips) + " clips."
                  + (res.kits > 0 ? " Drum kits loaded into the Sampler: " + juce::String (res.kits) + "." : juce::String());
    return res;
}

int DawSession::fixMultiOutPlugins()
{
    int fixed = 0;
    for (auto* p : te::getAllPlugins (edit(), false))
    {
        auto* ext = dynamic_cast<te::ExternalPlugin*> (p);
        if (ext == nullptr) continue;
        auto* pi = ext->getAudioPluginInstance();
        if (pi == nullptr || pi->getBusCount (false) < 2) continue;
        auto layout = pi->getBusesLayout();
        bool extraOn = false;
        for (int i = 1; i < layout.outputBuses.size(); ++i)
            if (! layout.outputBuses.getReference (i).isDisabled()) { extraOn = true; layout.outputBuses.getReference (i) = juce::AudioChannelSet::disabled(); }
        if (! extraOn || layout.outputBuses.getReference (0).isDisabled() || ! pi->checkBusesLayoutSupported (layout)) continue;   // a plug-in that refuses is left alone (no retry every tick)
        if (ext->setBusesLayout (layout)) ++fixed;
    }
    return fixed;
}
