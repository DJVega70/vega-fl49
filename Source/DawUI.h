#pragma once
#include "DawSession.h"
#include "VegaUI.h"

/** The DAW's look: the plug-ins' look-and-feel, with text buttons drawn as the mixer's console keys
    (dark bezel, grey face, lit in the button's "on" colour) so every view matches the mixer. */
struct DawLookAndFeel : public VegaLookAndFeel
{
    static juce::Colour litFor (juce::Button& b)
    {
        if (! b.isColourSpecified (juce::TextButton::buttonOnColourId)) return VegaColours::accent;
        const auto c = b.findColour (juce::TextButton::buttonOnColourId);
        return c == VegaColours::accentDim ? VegaColours::accent : c;
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        if (b.getProperties()["flat"]) { VegaLookAndFeel::drawButtonBackground (g, b, bg, over, down); return; }
        vegapaint::key (g, b.getLocalBounds().toFloat().reduced (1.0f), b.getToggleState(), over, down, litFor (b));
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        auto ink = on ? vegapaint::keyInk (true) : b.isColourSpecified (juce::TextButton::textColourOffId) ? b.findColour (juce::TextButton::textColourOffId) : VegaColours::text;
        if (! b.isEnabled()) ink = ink.withAlpha (0.4f);
        g.setColour (ink);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1, 0.7f);
    }
};

/** Shared view state for the arrange window, piano roll and mixer */
struct ViewState
{
    explicit ViewState (DawSession& s) : session (s) {}
    DawSession& session;
    double pixelsPerBeat = 28.0, scrollBeats = 0.0;
    int scrollY = 0;
    /** Snap like FL Studio's playlist: "Line" follows the visible grid (it gets finer as you zoom in),
        or a fixed Bar / Beat / 1/8 / 1/16, or Off. Hold Shift (or Alt in FL) to move freely. */
    enum class Snap { line, bar, beat, eighth, sixteenth, off };
    Snap snapMode = Snap::line;
    /** Divisions per beat of the visible grid: 4 = 1/16, 2 = 1/8, 1 = beats, 0 = bars only */
    int lineDiv() const { return pixelsPerBeat >= 48 ? 4 : pixelsPerBeat >= 24 ? 2 : pixelsPerBeat >= 12 ? 1 : 0; }
    /** Divisions per beat the snap uses right now (0 = whole bars or off) */
    int snapDivisions() const
    {
        switch (snapMode)
        {
            case Snap::line: return lineDiv();
            case Snap::beat: return 1;
            case Snap::eighth: return 2;
            case Snap::sixteenth: return 4;
            default: return 0;
        }
    }
    /** Grid step in beats for nudging / quantising (a bar counts as 4 beats here) */
    double gridBeats() const { const int d = snapDivisions(); return d > 0 ? 1.0 / d : 4.0; }
    te::EditItemID selectedClip, selectedTrack;
    bool followPlayhead = true;
    std::function<void()> onSelectionChanged, onOpenPianoRoll, onShowSession, onShowSongMap;
    std::function<bool (const juce::var& dragDescription, te::AudioTrack* target)> onPluginDropped;
    std::function<juce::Rectangle<int>()> controllerWindow;       // tracks x scenes a launcher controller shows (empty = none)
    std::function<void (te::WaveAudioClip*)> onSeparateClip;                                          // AI stem separation of an audio clip   // plug-in dragged from the browser

    te::Edit& edit() { return session.edit(); }
    te::TempoSequence& tempo() { return session.edit().tempoSequence; }
    double timeToBeats (te::TimePosition t) { return tempo().toBeats (t).inBeats(); }
    te::TimePosition beatsToTime (double b) { return tempo().toTime (te::BeatPosition::fromBeats (b)); }
    float beatToX (double beat, int x0) const { return (float) (x0 + (beat - scrollBeats) * pixelsPerBeat); }
    double xToBeat (float x, int x0) const { return scrollBeats + (x - x0) / pixelsPerBeat; }
    double snapBeat (double b, bool bypass = false)
    {
        if (bypass || snapMode == Snap::off) return b;
        const int d = snapDivisions();
        if (d > 0) return std::round (b * d) / d;
        // whole bars (following time-signature changes)
        auto& ts = tempo();
        const auto bb = ts.toBarsAndBeats (beatsToTime (std::max (0.0, b)));
        const double s = ts.toBeats (tracktion::tempo::BarsAndBeats { bb.bars, {} }).inBeats();
        const double e = ts.toBeats (tracktion::tempo::BarsAndBeats { bb.bars + 1, {} }).inBeats();
        return b - s <= e - b ? s : e;
    }
    /** Bar lines follow time-signature changes: is this (whole) beat the first beat of a bar? */
    bool isBarStart (double beat) { return tempo().toBarsAndBeats (beatsToTime (beat + 1.0e-4)).beats.inBeats() < 2.0e-4 + 1.0e-6; }
    int barNumber (double beat) { return tempo().toBarsAndBeats (beatsToTime (beat + 1.0e-4)).bars + 1; }
    int beatsPerBarAt (double beat) { return tempo().getTimeSigAt (beatsToTime (beat)).numerator.get(); }

    te::Clip* getSelectedClip()
    {
        if (! selectedClip.isValid()) return nullptr;
        for (auto* t : te::getAudioTracks (edit()))
        {
            for (auto* c : t->getClips()) if (c->itemID == selectedClip) return c;
            for (auto* s : t->getClipSlotList().getClipSlots()) if (auto* c = s != nullptr ? s->getClip() : nullptr; c != nullptr && c->itemID == selectedClip) return c;
        }
        return nullptr;
    }
    te::AudioTrack* getSelectedTrack()
    {
        for (auto* t : te::getAudioTracks (edit())) if (t->itemID == selectedTrack) return t;
        return nullptr;
    }
    void selectClip (te::Clip* c)
    {
        selectedClip = c != nullptr ? c->itemID : te::EditItemID();
        if (c != nullptr) if (auto* t = dynamic_cast<te::AudioTrack*> (c->getTrack())) selectedTrack = t->itemID;
        if (c != nullptr) if (auto* sl = c->getClipSlot()) if (auto* t = dynamic_cast<te::AudioTrack*> (&sl->track)) selectedTrack = t->itemID;
        if (onSelectionChanged) onSelectionChanged();
    }
    void selectTrack (te::AudioTrack* t) { selectedTrack = t != nullptr ? t->itemID : te::EditItemID(); if (onSelectionChanged) onSelectionChanged(); }
};

namespace dawui
{
inline juce::Colour clipColour (const te::Clip& c)
{
    if (auto* t = c.getTrack()) return DawSession::trackColour (*t);
    return VegaColours::accent;
}

/** Popup menu listing built-in effects/instruments and scanned plugins. Result is delivered asynchronously. */
inline void choosePlugin (te::Engine& engine, bool instruments, juce::Component* target,
                          std::function<void (juce::String builtInType, std::optional<juce::PluginDescription>)> done)
{
    juce::PopupMenu m, vega, other;
    auto types = engine.getPluginManager().knownPluginList.getTypes();
    std::sort (types.begin(), types.end(), [] (auto& a, auto& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    juce::StringArray builtIns;
    if (instruments) builtIns = { te::FourOscPlugin::xmlTypeName, te::SamplerPlugin::xmlTypeName };
    else builtIns = DawSession::builtInEffectTypes();
    juce::PopupMenu bi;
    for (int i = 0; i < builtIns.size(); ++i) bi.addItem (1 + i, DawSession::builtInDisplayName (builtIns[i]));
    m.addSubMenu ("Built-in", bi);
    for (int i = 0; i < types.size(); ++i)
    {
        if (types.getReference (i).isInstrument != instruments) continue;
        auto& menu = types.getReference (i).manufacturerName.containsIgnoreCase ("Vega") ? vega : other;
        menu.addItem (1000 + i, types.getReference (i).name + "   (" + types.getReference (i).pluginFormatName + ")");
    }
    if (vega.getNumItems() > 0) m.addSubMenu ("Vega Productions", vega);
    if (other.getNumItems() > 0) m.addSubMenu ("Plugins", other);
    if (types.isEmpty()) m.addItem (-1, "No plugins scanned yet - Options > Plugins...", false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [types, builtIns, done] (int r)
    {
        if (r >= 1000) done ({}, types[r - 1000]);
        else if (r >= 1) done (builtIns[r - 1], std::nullopt);
    });
}

/** Automation: parameters a lane can show, and the one currently chosen for a track's lane */
inline juce::String paramKey (te::AutomatableParameter& p) { return (p.getPlugin() != nullptr ? p.getPlugin()->itemID.toString() : juce::String()) + "|" + p.paramID; }
inline juce::Array<te::AutomatableParameter*> automatableParams (te::AudioTrack& t)
{
    juce::Array<te::AutomatableParameter*> r;
    for (auto* p : t.getAllAutomatableParams())
    {
        auto* pl = p->getPlugin();
        if (pl == nullptr || dynamic_cast<te::LevelMeterPlugin*> (pl) || dynamic_cast<te::AuxReturnPlugin*> (pl)) continue;
        r.add (p);
    }
    return r;
}
inline juce::String paramDisplayName (te::AutomatableParameter& p)
{
    auto* pl = p.getPlugin();
    if (dynamic_cast<te::VolumeAndPanPlugin*> (pl)) return p.getParameterName();          // "Volume", "Pan"
    return (pl != nullptr ? pl->getName() + ": " : juce::String()) + p.getParameterName();
}
inline te::AutomatableParameter* laneParam (te::AudioTrack& t)
{
    const auto key = t.state.getProperty ("vegaAutoParam").toString();
    auto params = automatableParams (t);
    for (auto* p : params) if (paramKey (*p) == key) return p;
    if (auto vp = t.getVolumePlugin()) return vp->volParam.get();
    return params.isEmpty() ? nullptr : params[0];
}
/** Wrap a UI edit so automation write mode can record it */
inline void gesture (te::AutomatableParameter* p, bool begin) { if (p != nullptr) { if (begin) p->parameterChangeGestureBegin(); else p->parameterChangeGestureEnd(); } }

/** Record arm: true when an input feeding this track is armed */
inline bool isArmed (te::AudioTrack& t)
{
    for (auto* in : t.edit.getAllInputDevices())
        if (te::isOnTargetTrack (*in, t, 0) && in->isRecordingEnabled (t.itemID)) return true;
    return false;
}
/** Toggle record arm, giving the track a default input first if it has none */
inline void toggleArm (te::AudioTrack& t)
{
    bool has = false;
    for (auto* in : t.edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, t, 0)) has = true;
    if (! has)
    {
        const bool midi = DawSession::kindOf (t) == DawSession::TrackKind::midi;
        for (auto* in : t.edit.getAllInputDevices())
        {
            const auto type = in->getInputDevice().getDeviceType();
            // MIDI: keyboards, plus VEGA FL49's processed FLkey keys (never the FLkey's raw port, which would double every note)
            const auto name = in->getInputDevice().getName();
            const bool rawFlkey = name.containsIgnoreCase ("FLkey") && type == te::InputDevice::physicalMidiDevice;
            if (midi ? ((type == te::InputDevice::physicalMidiDevice && ! rawFlkey) || (type == te::InputDevice::virtualMidiDevice && name == "VEGA FL49 Keys")) : type == te::InputDevice::waveDevice)
            {
                [[maybe_unused]] auto r = in->setTarget (t.itemID, ! midi, &t.edit.getUndoManager(), 0);
                if (! midi) break;
            }
        }
    }
    const bool now = ! isArmed (t);
    for (auto* in : t.edit.getAllInputDevices())
        if (te::isOnTargetTrack (*in, t, 0)) in->setRecordingEnabled (t.itemID, now);
}

/** Open a plug-in's window and bring it to the front */
inline void openPlugin (te::Plugin* p)
{
    if (p == nullptr) return;
    p->showWindowExplicitly();
    if (auto* w = p->windowState != nullptr ? p->windowState->pluginWindow.get() : nullptr) { w->setVisible (true); w->toFront (true); }
}

/** Menu of everything on a track: open the instrument or any effect, change the instrument, add effects */
inline void pluginChainMenu (DawSession& session, te::AudioTrack& track, juce::Component* target, std::function<void()> changed = nullptr)
{
    juce::PopupMenu m;
    auto plugins = session.getInsertPlugins (track);
    const bool inst = DawSession::kindOf (track) == DawSession::TrackKind::midi;
    m.addSectionHeader ("Click to open");
    for (int i = 0; i < plugins.size(); ++i)
        m.addItem (1 + i, (plugins[i]->isSynth() ? juce::String ("Instrument:  ") : juce::String ("Effect:  ")) + plugins[i]->getName() + (plugins[i]->isEnabled() ? "" : "  (bypassed)"));
    if (plugins.isEmpty()) m.addItem (-1, "No plug-ins on this track yet", false);
    m.addSeparator();
    if (inst) m.addItem (900, "Change instrument...");
    m.addItem (901, "Add effect...");
    te::AudioTrack::Ptr t (&track);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [&session, t, plugins, target, changed] (int r)
    {
        if (t == nullptr) return;
        if (r >= 1 && r <= plugins.size()) openPlugin (plugins[r - 1]);
        else if (r == 900 || r == 901)
            choosePlugin (session.edit().engine, r == 900, target, [&session, t, r, changed] (juce::String type, std::optional<juce::PluginDescription> d)
            {
                te::Plugin::Ptr p;
                if (r == 900) p = d ? session.setInstrument (*t, {}, &*d) : session.setInstrument (*t, type);
                else p = d ? session.addPlugin (*t, {}, &*d) : session.addPlugin (*t, type);
                openPlugin (p.get());
                if (changed) changed();
            });
    });
}

/** Simple slider-per-parameter editor for built-in plugins (which have no GUI of their own) */
class GenericEditor : public te::Plugin::EditorComponent, private juce::Timer
{
public:
    explicit GenericEditor (te::Plugin& p) : plugin (p)
    {
        for (auto* param : plugin.getAutomatableParameters())
        {
            auto* row = rows.add (new Row (*param));
            inner.addAndMakeVisible (row->slider); inner.addAndMakeVisible (row->label);
        }
        view.setViewedComponent (&inner, false); view.setScrollBarsShown (true, false);
        addAndMakeVisible (view);
        const int full = std::max (60, rows.size() * 34 + 20);
        inner.setSize (420, full);
        setSize (420 + (full > 640 ? view.getScrollBarThickness() : 0), std::min (full, 640));   // long lists scroll
        startTimerHz (15);
    }
    bool allowWindowResizing() override { return false; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }
    void paint (juce::Graphics& g) override { g.fillAll (VegaColours::background); }
    void resized() override
    {
        view.setBounds (getLocalBounds());
        int y = 10;
        for (auto* r : rows) { r->label.setBounds (10, y, 140, 26); r->slider.setBounds (150, y, inner.getWidth() - 160, 26); y += 34; }
    }
private:
    struct Row
    {
        explicit Row (te::AutomatableParameter& p) : param (p)
        {
            label.setText (p.getParameterName(), juce::dontSendNotification);
            label.setColour (juce::Label::textColourId, VegaColours::text);
            const auto r = p.getValueRange();
            slider.setSliderStyle (juce::Slider::LinearBar);
            slider.setRange (r.getStart(), r.getEnd(), 0.0);
            slider.setValue (p.getCurrentValue(), juce::dontSendNotification);
            slider.textFromValueFunction = [&p] (double v) { return p.valueToString ((float) v) + " " + p.getLabel(); };
            slider.onDragStart = [&p] { p.parameterChangeGestureBegin(); };
            slider.onDragEnd = [&p] { p.parameterChangeGestureEnd(); };
            slider.onValueChange = [this] { param.setParameter ((float) slider.getValue(), juce::sendNotification); };
            slider.updateText();
        }
        te::AutomatableParameter& param; juce::Slider slider; juce::Label label;
    };
    void timerCallback() override
    {
        for (auto* r : rows) if (! r->slider.isMouseButtonDown()) r->slider.setValue (r->param.getCurrentValue(), juce::dontSendNotification);
    }
    te::Plugin& plugin; juce::OwnedArray<Row> rows;
    juce::Viewport view; juce::Component inner;
};

inline juce::String formatBarsBeats (te::Edit& edit, te::TimePosition t)
{
    auto bb = edit.tempoSequence.toBarsAndBeats (t);
    return juce::String (bb.bars + 1) + "." + juce::String (bb.getWholeBeats() + 1) + "." + juce::String ((int) (bb.getFractionalBeats().inBeats() * 4) + 1);
}
} // namespace dawui
