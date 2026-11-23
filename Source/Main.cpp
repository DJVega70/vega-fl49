#include "DawSession.h"
#include "DawUI.h"
#include "PluginWindow.h"
#include "ArrangeView.h"
#include "MixerView.h"
#include "PianoRoll.h"
#include "Controllers.h"
#include "PluginBrowser.h"
#include "StemSeparator.h"
#include "StemClient.h"
#include "RoutingMatrix.h"
#include "SessionView.h"
#include "SongMap.h"
#include "ExportDialog.h"
#include "Edition.h"
#if VEGA_FL49
 #include "Fl49Panel.h"
 #include "HardwareStrip.h"
#endif

//==============================================================================
class TransportBar : public juce::Component, private juce::Timer
{
public:
    explicit TransportBar (ViewState& v) : vs (v)
    {
        auto btn = [this] (juce::TextButton& b, const juce::String& t, juce::Colour on = VegaColours::accent)
        {
            b.setButtonText (t); b.setColour (juce::TextButton::buttonOnColourId, on); addAndMakeVisible (b);
        };
        btn (home, juce::String::fromUTF8 ("\xe2\x8f\xae")); btn (play, juce::String::fromUTF8 ("\xe2\x96\xb6")); btn (stop, juce::String::fromUTF8 ("\xe2\x96\xa0"));
        btn (rec, juce::String::fromUTF8 ("\xe2\x97\x8f"), juce::Colour (0xffe04848)); btn (loop, "LOOP"); btn (click, "CLICK"); btn (autoWrite, "WRITE", juce::Colour (0xffe8a23a));
        autoWrite.setTooltip ("Automation write: while playing, moving faders, pans, EQ and plug-in knobs records automation");
        autoWrite.onClick = [this] { auto& arm = vs.edit().getAutomationRecordManager(); arm.setWritingAutomation (! arm.isWritingAutomation()); };
        btn (learn, "LEARN", juce::Colour (0xffb183f2));
        learn.setTooltip ("MIDI learn: turn on, move a control on screen, then move a knob / fader on your controller. Turn off when done.");
        learn.onClick = [this] { auto& ls = vs.edit().engine.getMidiLearnState(); ls.setActive (! ls.isActive()); repaint(); };
        btn (view, "MIXER"); btn (sessionBtn, "SESSION"); btn (browse, "PLUGINS"); btn (addAudio, "+ AUDIO"); btn (addInst, "+ INST"); btn (addBus, "+ BUS");
        sessionBtn.setTooltip ("Clip launcher (Shift+Tab)"); addInst.setTooltip ("Add an instrument track");
        home.onClick = [this] { vs.edit().getTransport().setPosition (0s); };
        play.onClick = [this] { auto& t = vs.edit().getTransport(); if (t.isPlaying()) t.stop (false, false); else t.play (false); };
        stop.onClick = [this] { auto& t = vs.edit().getTransport(); if (t.isPlaying()) t.stop (false, false); else t.setPosition (0s); };
        rec.onClick = [this] { auto& t = vs.edit().getTransport(); if (t.isRecording()) t.stop (true, false); else t.record (false); };
        loop.onClick = [this] { auto& t = vs.edit().getTransport(); t.looping = ! t.looping.get(); if (t.looping && t.getLoopRange().getLength() <= 0s) t.setLoopRange ({ 0s, vs.session.barLength() * 4 }); };
        click.onClick = [this] { vs.edit().clickTrackEnabled = ! vs.edit().clickTrackEnabled.get(); };
        addAudio.onClick = [this] { vs.selectTrack (vs.session.addTrack (DawSession::TrackKind::audio)); };
        addInst.onClick = [this] { vs.selectTrack (vs.session.addTrack (DawSession::TrackKind::midi)); };
        addBus.onClick = [this] { vs.selectTrack (vs.session.addTrack (DawSession::TrackKind::bus)); };
        tempo.setSliderStyle (juce::Slider::LinearBar); tempo.setRange (40.0, 240.0, 0.1); tempo.setTextValueSuffix (" BPM");
        tempo.setColour (juce::Slider::trackColourId, VegaColours::accentDim);
        tempo.onValueChange = [this] { vs.session.setTempo (tempo.getValue()); };
        addAndMakeVisible (tempo);
        sig.addItemList ({ "4/4", "3/4", "6/8", "7/8", "5/4" }, 1);
        sig.onChange = [this] { const int n[] { 4, 3, 6, 7, 5 }, d[] { 4, 4, 8, 8, 4 }; const int i = sig.getSelectedItemIndex(); if (i >= 0) vs.session.setTimeSig (n[i], d[i]); };
        addAndMakeVisible (sig);
        snap.addItemList ({ "Snap: Line", "Snap: Bar", "Snap: Beat", "Snap: 1/8", "Snap: 1/16", "Snap: Off" }, 1);
        snap.setSelectedItemIndex (0, juce::dontSendNotification);
        snap.setTooltip ("Line follows the grid you see (finer as you zoom in), like FL Studio. Hold Shift while dragging to move freely.");
        snap.onChange = [this] { const int i = snap.getSelectedItemIndex(); if (i >= 0) vs.snapMode = (ViewState::Snap) i; };
        addAndMakeVisible (snap);
        startTimerHz (20);
    }
    std::function<void()> onToggleView, onToggleBrowser, onToggleSession;
    void setBrowserShown (bool b) { browse.setToggleState (b, juce::dontSendNotification); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1c1c1c));
        auto r = display.toFloat();
        vegaui::drawScreen (g, r);
        auto& tr = vs.edit().getTransport();
        g.setFont (VegaFonts::mono (19.0f)); g.setColour (VegaColours::accent);
        g.drawText (dawui::formatBarsBeats (vs.edit(), tr.getPosition()), r.removeFromLeft (r.getWidth() * 0.5f).toNearestInt(), juce::Justification::centred);
        const double secs = tr.getPosition().inSeconds();
        g.setFont (VegaFonts::mono (13.0f)); g.setColour (VegaColours::textDim);
        g.drawText (juce::String::formatted ("%d:%02d.%03d", (int) secs / 60, (int) secs % 60, (int) (std::fmod (secs, 1.0) * 1000)), r.toNearestInt(), juce::Justification::centred);
        if (vs.edit().engine.getMidiLearnState().isActive())
        {
            auto d = display.toFloat();
            g.setColour (juce::Colour (0xee1a1424)); g.fillRoundedRectangle (d, 3.0f);
            g.setColour (juce::Colour (0xffb183f2)); g.setFont (VegaFonts::ui (11.0f, true));
            g.drawFittedText ("MIDI LEARN: move a control here,\nthen a knob on your controller", d.toNearestInt().reduced (6, 2), juce::Justification::centred, 2);
        }
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.5f, true));
        const float cpu = (float) vs.edit().engine.getDeviceManager().getCpuUsage();
        g.drawText ("CPU", cpuArea.withHeight (12), juce::Justification::centredLeft);
        auto bar = cpuArea.withTrimmedTop (14).toFloat();
        g.setColour (juce::Colour (0xff111111)); g.fillRect (bar);
        g.setColour (cpu > 0.8f ? juce::Colour (0xffe04848) : VegaColours::accent); g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, cpu)));
        g.setColour (VegaColours::line); g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
    }
    void resized() override
    {
        int x = 8; const int y = 8, h = getHeight() - 16;
        for (auto* b : { &home, &play, &stop, &rec }) { b->setBounds (x, y, 40, h); x += 44; }
        loop.setBounds (x + 6, y, 58, h); x += 70; click.setBounds (x, y, 60, h); x += 66; autoWrite.setBounds (x, y, 62, h); x += 66; learn.setBounds (x, y, 60, h); x += 70;
        display = { x, y - 2, 260, h + 4 }; x += 270;
        tempo.setBounds (x, y, 110, h); x += 116; sig.setBounds (x, y, 70, h); x += 76; snap.setBounds (x, y, 112, h); x += 120;
        cpuArea = { x + 4, y, 60, h }; x += 74;
        view.setBounds (getWidth() - 88, y, 80, h); sessionBtn.setBounds (getWidth() - 172, y, 80, h);
        addBus.setBounds (getWidth() - 238, y, 62, h); addInst.setBounds (getWidth() - 312, y, 70, h); addAudio.setBounds (getWidth() - 386, y, 70, h);
        browse.setBounds (getWidth() - 472, y, 82, h);
        browse.onClick = [this] { if (onToggleBrowser) onToggleBrowser(); };
        view.onClick = [this] { if (onToggleView) onToggleView(); };
        sessionBtn.onClick = [this] { if (onToggleSession) onToggleSession(); };
    }
    void setViewLabel (bool mixerShowing, bool sessionShowing = false)
    {
        view.setButtonText (mixerShowing ? "ARRANGE" : "MIXER"); sessionBtn.setButtonText (sessionShowing ? "ARRANGE" : "SESSION");
        sessionBtn.setToggleState (sessionShowing, juce::dontSendNotification);
    }
    void refreshFromEdit()
    {
        tempo.setValue (vs.session.getTempo(), juce::dontSendNotification);
        auto* ts = vs.edit().tempoSequence.getTimeSig (0);
        const juce::String s = juce::String (ts->numerator.get()) + "/" + juce::String (ts->denominator.get());
        for (int i = 0; i < sig.getNumItems(); ++i) if (sig.getItemText (i) == s) sig.setSelectedItemIndex (i, juce::dontSendNotification);
    }
private:
    void timerCallback() override
    {
        auto& tr = vs.edit().getTransport();
        play.setToggleState (tr.isPlaying(), juce::dontSendNotification);
        rec.setToggleState (tr.isRecording(), juce::dontSendNotification);
        loop.setToggleState (tr.looping.get(), juce::dontSendNotification);
        click.setToggleState (vs.edit().clickTrackEnabled.get(), juce::dontSendNotification);
        autoWrite.setToggleState (vs.edit().getAutomationRecordManager().isWritingAutomation(), juce::dontSendNotification);
        const bool learning = vs.edit().engine.getMidiLearnState().isActive();
        if (learn.getToggleState() != learning) { learn.setToggleState (learning, juce::dontSendNotification); repaint(); }
        if (! tempo.isMouseButtonDown() && ! tempo.hasKeyboardFocus (true)) tempo.setValue (vs.session.getTempo(), juce::dontSendNotification);
        repaint (display); repaint (cpuArea);
    }
    ViewState& vs;
    juce::TextButton home, play, stop, rec, loop, click, view, sessionBtn, addAudio, addInst, addBus, browse, autoWrite, learn;
    juce::Slider tempo; juce::ComboBox sig, snap;
    juce::Rectangle<int> display, cpuArea;
};

//==============================================================================
class MainComponent : public juce::Component, public juce::MenuBarModel, public juce::DragAndDropContainer, private juce::Timer
{
public:
    MainComponent (te::Engine& e) : engine (e), session (e), vs (session), hub (e, session), apc (e, session), transport (vs), arrange (vs), mixer (vs), launcher (vs), pianoRoll (vs), browser (vs)
    {
        setLookAndFeel (&lnf);
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);     // menus, dialogs and floating windows get the plug-ins' look too
        session.edit().getAutomationRecordManager().setWritingAutomation (false);
        menuBar.setModel (this);
        addAndMakeVisible (menuBar); addAndMakeVisible (transport); addAndMakeVisible (arrange);
        addChildComponent (mixer); addChildComponent (launcher); addChildComponent (pianoRoll); addChildComponent (divider);
        transport.onToggleView = [this] { showMixer (! mixerShown()); };
        transport.onToggleSession = [this] { showSession (! launcher.isVisible()); };
        vs.onShowSession = [this] { showSession (true); };
        vs.onShowSongMap = [this] { showSongMap(); };
        arrange.onRenderTrack = [this] (te::AudioTrack& t, bool bounce) { renderTrack (t, bounce); };
        addAndMakeVisible (browser); transport.setBrowserShown (true);
        transport.onToggleBrowser = [this] { showBrowser (! browser.isVisible()); };
        browser.onLoad = [this] (const PluginBrowser::Item& it, te::AudioTrack* t) { loadFromBrowser (it, t); };
        vs.onPluginDropped = [this] (const juce::var& d, te::AudioTrack* t)
        {
            if (auto* it = browser.itemForDragDescription (d)) { loadFromBrowser (*it, t, true); return true; }
            return false;
        };
        vs.onSeparateClip = [this] (te::WaveAudioClip* c) { if (c != nullptr) startSeparation (c->getOriginalFile(), c->getName(), te::WaveAudioClip::Ptr (c)); };
        // pick up newly installed plug-ins in the background; first run does a full visible scan
        juce::Timer::callAfterDelay (1500, [this, sp = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (sp != nullptr) browser.startScan (engine.getPluginManager().knownPluginList.getNumTypes() > 0);
        });
        vs.onOpenPianoRoll = [this] { showPianoRoll (true); };
        vs.onSelectionChanged = [this] { hub.surface.selectedID = vs.selectedTrack; apc.surface.selectedID = vs.selectedTrack; routeMidiToSelectedTrack(); arrange.repaint();
                                    #if VEGA_FL49
                                     fl49.macroSlot = 0; fl49.macroPage = 0; fl49Panel.refresh();
                                    #endif
                                  };
        apc.surface.onSelectTrack = [this] (te::AudioTrack* t) { vs.selectTrack (t); };
        apc.surface.onSelectClip = [this] (te::Clip* c) { vs.selectClip (c); };
        apc.surface.onRecordSlot = [this] (te::AudioTrack& t, int sc) { vs.selectTrack (&t); return launcher.startSlotRecording (t, sc); };
        apc.surface.isRecordingSlot = [this] (te::AudioTrack& t, int sc) { return launcher.isRecordingSlot (t, sc); };
        apc.surface.onStopSlotRecording = [this] { launcher.stopSlotRecording(); };
        apc.surface.onOpenClip = [this] (te::Clip* c) { vs.selectClip (c); if (dynamic_cast<te::MidiClip*> (c) != nullptr || dynamic_cast<te::StepClip*> (c) != nullptr) showPianoRoll (true); };
        vs.controllerWindow = [this]
        {
            return apc.isConnected() ? juce::Rectangle<int> (apc.surface.trackOffset, apc.surface.sceneOffset, ApcSurface::padCols, ApcSurface::padRows) : juce::Rectangle<int>();
        };
        apc.autoConnect();
        hub.onSelectTrack = [this] (te::AudioTrack* t) { vs.selectTrack (t); };
        hub.onSave = [this] { save (false); };
        hub.autoConnect();
       #if VEGA_FL49
        addAndMakeVisible (fl49Panel);
        fl49.getSelectedTrack = [this] { return vs.getSelectedTrack(); };
        fl49.getBankStart = [this] { return hub.surface.bankStart; };
        hub.surface.macroParam = [this] (int k) { return fl49.macroParam (k); };
        hub.surface.onKnobTouched = [this] (int k) { fl49Panel.knobTouched (k); strip.knobTouched (k); };
        addChildComponent (strip);
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<MainComponent> (this)] { if (sp != nullptr) sp->showSession (true); });
       #endif
        juce::Timer::callAfterDelay (1200, [sp = juce::Component::SafePointer<MainComponent> (this)] { if (sp != nullptr) sp->autoDetectMcu(); });
        pianoRoll.onClose = [this] { showPianoRoll (false); };
        pianoRoll.onDetach = [this] { setPianoFloating (! pianoFloat); };
        mixer.onDetach = [this] { setMixerFloating (! mixerFloat); };
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<MainComponent> (this)] { if (sp != nullptr) sp->loadUi(); });
        session.onBeforeEditChange = [this] {
                                       #if VEGA_FL49
                                        strip.clearMeters();
                                       #endif
                                        engine.getExternalControllerManager().detachFromEdit (&session.edit()); arrange.clearAll(); mixer.clearAll(); };
        session.onEditChanged = [this] { arrange.editChanged(); mixer.rebuild(); launcher.editChanged(); hub.editChanged(); apc.editChanged();
                                  #if VEGA_FL49
                                   fl49.editChanged();
                                  #endif
                                   transport.refreshFromEdit(); setupInputs(); showPianoRoll (false); updateTitle(); };
        divider.onDrag = [this] (int dy) { pianoH = juce::jlimit (160, getHeight() - 240, pianoH - dy); resized(); };
        setupInputs();
        transport.refreshFromEdit();
        setWantsKeyboardFocus (true);
        setSize (1500, 900);
        startTimerHz (2);
        juce::Timer::callAfterDelay (900, [sp = juce::Component::SafePointer<MainComponent> (this)] { if (sp != nullptr) sp->offerRecovery(); });
    }
    ~MainComponent() override { saveUi(); mixerWin.reset(); pianoWin.reset(); engine.getExternalControllerManager().detachFromEdit (&session.edit()); menuBar.setModel (nullptr); mixer.clearAll(); setLookAndFeel (nullptr); juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

    DawSession& getSession() { return session; }
    std::function<void (juce::String)> onTitle;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (VegaColours::background);
        auto top = getLocalBounds().removeFromTop (34).toFloat();
        g.setColour (juce::Colour (0xff222222)); g.fillRect (top);
        g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (15.0f, true));
       #if VEGA_FL49
        g.setFont (VegaFonts::ui (17.0f, true));
        g.drawText ("VEGA", top.withTrimmedLeft (12).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (VegaColours::accent); g.drawText ("FL49", top.withTrimmedLeft (64).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, true));
        g.drawText ("by Vega Productions", top.withTrimmedLeft (112).toNearestInt(), juce::Justification::centredLeft);
        {   // status, like the hardware's screen: key, keyboard, launch quantise
            auto st = top.toNearestInt().withTrimmedLeft (640).reduced (12, 6);
            auto field = [&] (const juce::String& label, const juce::String& value, juce::Colour vc, int w)
            {
                auto f = st.removeFromRight (w); st.removeFromRight (14);
                g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.5f, true));
                g.drawText (label, f.removeFromLeft (f.getWidth() / 2 - 6), juce::Justification::centredRight);
                f.removeFromLeft (6);
                g.setColour (vc); g.setFont (VegaFonts::ui (12.0f, true));
                g.drawText (value, f, juce::Justification::centredLeft);
            };
            field ("SCENE LAUNCH:", juce::String (launchQuantName()), VegaColours::text, 220);
            const bool keys = fl49.isConnected(), daw = hub.isConnected();
            field ("HARDWARE:", keys && daw ? "FLkey 2 49  SYNC OK" : keys || daw ? "FLkey 2 49  PARTIAL" : "NO FLkey", keys && daw ? juce::Colour (0xff3ddc84) : keys || daw ? juce::Colour (0xffffc23a) : VegaColours::textDim, 260);
            const auto ks = fl49.keySettings();
            const juce::String key = juce::String (fl49::noteName (ks.root)) + " " + juce::String (fl49::scales()[(size_t) ks.scale].name).toUpperCase();
            field (ks.keys != fl49::KeyMode::off ? "SCALE LOCK ON:" : "KEY:", key, ks.keys != fl49::KeyMode::off ? VegaColours::accent : VegaColours::text, 250);
        }
       #else
        g.drawText ("Vega Productions", top.withTrimmedLeft (12).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, true));
        g.drawText ("DAW  VP-20", top.withTrimmedLeft (150).toNearestInt(), juce::Justification::centredLeft);
       #endif
        g.setColour (VegaColours::line); g.drawHorizontalLine (33, 0, (float) getWidth());
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (34);
        menuBar.setBounds (top.withTrimmedLeft (240).withWidth (360).reduced (0, 5));
        transport.setBounds (r.removeFromTop (52));
       #if VEGA_FL49
        strip.setVisible (stripShown);
        if (stripShown) strip.setBounds (r.removeFromBottom (HardwareStrip::preferredHeight));
       #endif
        if (! pianoFloat && pianoRoll.isVisible())
        {
            pianoRoll.setBounds (r.removeFromBottom (pianoH));
            divider.setBounds (r.removeFromBottom (6));
        }
       #if VEGA_FL49
        fl49Panel.setVisible (fl49Shown);
        if (fl49Shown) fl49Panel.setBounds (r.removeFromRight (Fl49Panel::preferredWidth));
       #endif
        if (browser.isVisible()) browser.setBounds (r.removeFromLeft (browserW));
        arrange.setBounds (r); launcher.setBounds (r); if (! mixerFloat) mixer.setBounds (r);
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        auto& tr = session.edit().getTransport();
        const auto cmd = juce::ModifierKeys::commandModifier;
        if (k == juce::KeyPress::spaceKey) { if (tr.isPlaying()) tr.stop (false, false); else tr.play (false); return true; }
        if (k.getKeyCode() == 'R' && k.getModifiers().isAnyModifierKeyDown() == false) { if (tr.isRecording()) tr.stop (true, false); else tr.record (false); return true; }
        if (k.getKeyCode() == 'L' && ! k.getModifiers().isAnyModifierKeyDown()) { tr.looping = ! tr.looping.get(); return true; }
        if (k == juce::KeyPress (juce::KeyPress::tabKey, juce::ModifierKeys::shiftModifier, 0)) { showSession (! launcher.isVisible()); return true; }
        if (k == juce::KeyPress (juce::KeyPress::tabKey)) { showMixer (! mixerShown()); return true; }
        if (k.getKeyCode() == 'M' && ! k.getModifiers().isAnyModifierKeyDown()) { session.edit().getUndoManager().beginNewTransaction ("Add marker"); session.addMarker (tr.getPosition()); return true; }
        if (k.getKeyCode() == 'B' && ! k.getModifiers().isAnyModifierKeyDown()) { showBrowser (! browser.isVisible()); return true; }
       #if VEGA_FL49
        if (k.getKeyCode() == 'F' && ! k.getModifiers().isAnyModifierKeyDown()) { fl49Shown = ! fl49Shown; resized(); return true; }
        if (k.getKeyCode() == 'H' && ! k.getModifiers().isAnyModifierKeyDown()) { stripShown = ! stripShown; resized(); return true; }
       #endif
        if (k == juce::KeyPress ('z', cmd, 0)) { session.edit().getUndoManager().undo(); return true; }
        if (k == juce::KeyPress ('z', cmd | juce::ModifierKeys::shiftModifier, 0) || k == juce::KeyPress ('y', cmd, 0)) { session.edit().getUndoManager().redo(); return true; }
        if (k == juce::KeyPress ('s', cmd, 0)) { save (false); return true; }
        if (k == juce::KeyPress ('e', cmd | juce::ModifierKeys::shiftModifier, 0)) { showExport (false); return true; }
        if (k == juce::KeyPress ('o', cmd, 0)) { open(); return true; }
        if (k == juce::KeyPress (juce::KeyPress::homeKey)) { tr.setPosition (0s); return true; }
        return false;
    }

    // ---- menus
    juce::StringArray getMenuBarNames() override { return { "File", "Edit", "Track", "Options", "Help" }; }
    juce::PopupMenu getMenuForIndex (int idx, const juce::String&) override
    {
        juce::PopupMenu m;
        if (idx == 0)
        {
            m.addItem (1, "New project"); m.addItem (2, "Open...      Ctrl+O");
            juce::PopupMenu recent; auto files = recentFiles();
            for (int i = 0; i < files.size(); ++i) recent.addItem (700 + i, files[i].getFileNameWithoutExtension() + "   (" + files[i].getParentDirectory().getFileName() + ")");
            if (files.isEmpty()) recent.addItem (-1, "No recent projects", false);
            m.addSubMenu ("Open recent", recent);
            m.addItem (3, "Save          Ctrl+S"); m.addItem (4, "Save as...");
            m.addSeparator(); m.addItem (5, "Import audio / MIDI..."); m.addItem (10, "Import phone sketch (Vega Sketch)..."); m.addItem (7, "Separate stems from a song (AI)...");
            m.addSeparator(); m.addItem (6, "Export mixdown...   Ctrl+Shift+E"); m.addItem (8, "Export stems...");
            m.addSeparator(); m.addItem (9, "Quit");
        }
        else if (idx == 1)
        {
            m.addItem (20, "Undo   Ctrl+Z", session.edit().getUndoManager().canUndo()); m.addItem (21, "Redo   Ctrl+Shift+Z", session.edit().getUndoManager().canRedo());
            m.addSeparator(); m.addItem (22, "Split clip at playhead   Ctrl+E"); m.addItem (23, "Duplicate clip   Ctrl+D"); m.addItem (24, "Delete clip   Del");
            m.addSeparator(); m.addItem (25, "Add marker at playhead   M"); m.addItem (26, "Markers & tempo list...");
        }
        else if (idx == 2)
        {
            m.addItem (30, "Add audio track"); m.addItem (31, "Add instrument track"); m.addItem (32, "Add bus"); m.addItem (29, "Add folder");
            m.addSeparator(); m.addItem (28, "Duplicate selected track", vs.getSelectedTrack() != nullptr && ! DawSession::isFolder (*vs.getSelectedTrack()));
            m.addItem (33, "Delete selected track", vs.getSelectedTrack() != nullptr);
            m.addItem (27, "Bounce selected track in place", vs.getSelectedTrack() != nullptr && DawSession::kindOf (*vs.getSelectedTrack()) != DawSession::TrackKind::bus);
            m.addItem (19, "Consolidate clips on selected track", vs.getSelectedTrack() != nullptr && DawSession::kindOf (*vs.getSelectedTrack()) != DawSession::TrackKind::bus);
            m.addSeparator();
            m.addItem (34, "Show mixer   Tab", true, mixerShown()); m.addItem (48, "Show clip launcher   Shift+Tab", true, launcher.isVisible());
            m.addItem (35, "Show piano roll", true, pianoShown());
            m.addItem (136, "Float the mixer in its own window", true, mixerFloat);
            m.addItem (137, "Float the piano roll in its own window", true, pianoFloat);
            m.addItem (36, "Show plug-in browser   B", true, browser.isVisible());
           #if VEGA_FL49
            m.addItem (138, "Show FLkey side panel   F", true, fl49Shown);
            m.addItem (139, "Show FLkey hardware strip   H", true, stripShown);
           #endif
            m.addItem (39, "Routing matrix...");
            m.addSeparator();
            m.addItem (37, "Open instrument of selected track", vs.getSelectedTrack() != nullptr);
            m.addItem (38, "Plug-ins on selected track...", vs.getSelectedTrack() != nullptr);
        }
        else if (idx == 3)
        {
            m.addItem (40, "Audio / MIDI settings..."); m.addItem (41, "Scan for new plug-ins"); m.addItem (45, "Advanced plug-in list...");
            m.addItem (46, "MIDI learn mappings...");
            m.addItem (47, juce::String ("Mackie Control (MCU)...") + (mcuEnabled() ? "   - on" : ""));
            m.addItem (44, juce::String ("Controllers (FLkey 2 / HUI)...") + (hub.isConnected() ? "   - connected" : ""));
            m.addItem (49, juce::String ("Akai APC Key 25 mk2...") + (apc.isConnected() ? "   - connected" : ""));
            m.addItem (42, "Metronome", true, session.edit().clickTrackEnabled.get());
            m.addItem (43, "Follow playhead", true, vs.followPlayhead);
        }
        else
        {
            m.addItem (50, "Keyboard shortcuts...");
            m.addItem (51, "About " VEGA_APP_NAME);
        }
        return m;
    }
    void menuItemSelected (int id, int) override
    {
        switch (id)
        {
            case 1: confirmDiscard ([this] { session.newProject(); }); break;
            case 2: open(); break;
            case 3: save (false); break;
            case 4: save (true); break;
            case 5: importFiles(); break;
            case 6: showExport (false); break;
            case 10: importPhoneSketch(); break;
            case 8: showExport (true); break;
            case 25: session.edit().getUndoManager().beginNewTransaction ("Add marker"); session.addMarker (session.edit().getTransport().getPosition()); break;
            case 26: showSongMap(); break;
            case 29: { session.edit().getUndoManager().beginNewTransaction ("Add folder"); vs.selectTrack (session.addFolder()); arrange.rebuildHeaders(); break; }
            case 28: if (auto* t = vs.getSelectedTrack()) { if (auto* d = session.duplicateTrack (*t)) vs.selectTrack (d); arrange.rebuildHeaders(); mixer.rebuild(); } break;
            case 27: if (auto* t = vs.getSelectedTrack()) renderTrack (*t, true); break;
            case 19: if (auto* t = vs.getSelectedTrack()) renderTrack (*t, false); break;
            case 48: showSession (! launcher.isVisible()); break;
            case 49: showApcSettings(); break;
            case 7: separateFile(); break;
            case 9: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 700: case 701: case 702: case 703: case 704: case 705: case 706: case 707: case 708: case 709:
            {
                const auto f = recentFiles()[id - 700];
                confirmDiscard ([this, f] { if (! session.openProject (f)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open", "Couldn't open " + f.getFullPathName()); else addRecent (f); });
                break;
            }
            case 20: session.edit().getUndoManager().undo(); break;
            case 21: session.edit().getUndoManager().redo(); break;
            case 22: arrange.splitSelectedAtPlayhead(); break;
            case 23: arrange.duplicateSelectedClip(); break;
            case 24: arrange.deleteSelectedClip(); break;
            case 30: session.addTrack (DawSession::TrackKind::audio); break;
            case 31: session.addTrack (DawSession::TrackKind::midi); break;
            case 32: session.addTrack (DawSession::TrackKind::bus); break;
            case 33: if (auto* t = vs.getSelectedTrack()) session.deleteTrack (*t); break;
            case 34: showMixer (! mixerShown()); break;
            case 35: showPianoRoll (! pianoShown()); break;
            case 136: setMixerFloating (! mixerFloat); break;
            case 137: setPianoFloating (! pianoFloat); break;
           #if VEGA_FL49
            case 138: fl49Shown = ! fl49Shown; resized(); break;
            case 139: stripShown = ! stripShown; resized(); break;
           #endif
            case 40: showAudioSettings(); break;
            case 41: showBrowser (true); browser.startScan (false); break;
            case 45: showPluginList(); break;
            case 46: showMappings(); break;
            case 47: showMcuSettings(); break;
            case 36: showBrowser (! browser.isVisible()); break;
            case 39:
            {
                juce::DialogWindow::LaunchOptions o;
                o.dialogTitle = "Routing matrix"; o.content.setOwned (new RoutingMatrix (session));
                o.dialogBackgroundColour = juce::Colour (0xff1b1b1d); o.resizable = true; o.useNativeTitleBar = true;
                o.launchAsync();
                break;
            }
            case 37: if (auto* t = vs.getSelectedTrack()) { auto* f = t->pluginList[0]; if (f != nullptr && f->isSynth()) dawui::openPlugin (f); else dawui::pluginChainMenu (session, *t, &transport); } break;
            case 38: if (auto* t = vs.getSelectedTrack()) dawui::pluginChainMenu (session, *t, &transport, [this] { mixer.rebuild(); }); break;
            case 44:
            {
                juce::DialogWindow::LaunchOptions o;
                o.dialogTitle = "Controllers"; o.content.setOwned (new ControllerPanel (hub));
                o.dialogBackgroundColour = juce::Colour (0xff1e1e1e); o.resizable = true; o.useNativeTitleBar = true;
                o.launchAsync();
                break;
            }
            case 42: session.edit().clickTrackEnabled = ! session.edit().clickTrackEnabled.get(); break;
            case 43: vs.followPlayhead = ! vs.followPlayhead; break;
            case 50: juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Shortcuts",
                        "Space  play / stop\nR  record (with LOOP on, every pass becomes a take)\nL  loop on/off\nM  add a marker\nHome  go to start\nTab  arrange / mixer\nShift+Tab  clip launcher\nB  plug-in browser\nCtrl+Shift+E  export\nCtrl+Z / Ctrl+Shift+Z  undo / redo\nCtrl+S  save\nCtrl+E  split clip at playhead\nCtrl+D  duplicate clip\nDel  delete clip\n\n"
                        "Arrange: drag clips to move, drag edges to trim, drag the top corners of audio clips for fades, Alt-drag to copy, Shift for no snap.\n"
                        "Ctrl+wheel zoom, Shift+wheel scroll. Double-click an instrument track to create a MIDI clip.\n"
                        "Ruler: click to move the playhead, drag the top strip (or Shift-drag) to set the loop.\n"
                        "Piano roll: click to add notes, drag to move, drag the right edge to resize, right-click or double-click to delete, drag in the velocity lane."); break;
            case 51: juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, VEGA_APP_NAME,
                        "Version " JUCE_APPLICATION_VERSION_STRING "\n\nBuilt on Tracktion Engine (GPLv3) and JUCE.\n32-bit floating-point, multi-core audio engine."); break;
            default: break;
        }
    }

    void confirmDiscard (std::function<void()> then)
    {
        if (! session.hasUnsavedChanges()) { then(); return; }
        auto opts = juce::MessageBoxOptions().withTitle ("Unsaved changes").withMessage ("Save the current project first?")
                        .withButton ("Save", 1).withButton ("Don't save", 2).withButton ("Cancel", 0);
        juce::AlertWindow::showAsync (opts, [this, then] (int r)
        {
            if (r == 1) save (false, then);
            else if (r == 2) then();
        });
    }

private:
    void timerCallback() override
    {
        updateTitle();
        repaint (0, 0, getWidth(), 34);   // the status in the menu row
        if (++autosaveTicks >= 2 * 120) { autosaveTicks = 0; autosave(); }      // every 2 minutes
        // a keyboard / controller plugged in (or out) while running: enable its keys and point them at the selected instrument
        juce::StringArray names;
        for (auto& d : engine.getDeviceManager().getMidiInDevices()) names.add (d->getName());
        const auto sig = names.joinIntoString ("|");
        if (sig != midiInputsSeen) { const bool first = midiInputsSeen.isEmpty(); midiInputsSeen = sig; if (! first) setupInputs(); }
        // a keyboard that was switched on a moment ago only shows up in the edit once the engine has opened it:
        // keep checking, so its keys always end up on the selected instrument instead of "not connected to a track"
        session.fixMultiOutPlugins();   // per-voice drum outs etc. -> the track's main output
        if (++routeTicks >= 2) { routeTicks = 0; if (keysUnrouted()) routeMidiToSelectedTrack(); else stripRawFl49Targets(); }
    }
    int routeTicks = 0;
    bool keysUnrouted()
    {
        bool anyInstrument = false;
        for (auto* t : session.getTracks()) if (DawSession::kindOf (*t) == DawSession::TrackKind::midi) anyInstrument = true;
        if (! anyInstrument) return false;
        for (auto* in : session.edit().getAllInputDevices())
        {
            auto& dev = in->getInputDevice();
            if (! isKeysInput (*in) || ! dev.isEnabled() || isControllerPort (dev.getName())) continue;
            bool targeted = false;
            for (auto* t : session.getTracks()) if (te::isOnTargetTrack (*in, *t, 0)) targeted = true;
            if (! targeted) return true;
        }
        return false;
    }
    int autosaveTicks = 0;
    juce::String midiInputsSeen;
    void updateTitle()
    {
        const auto f = session.getProjectFile();
        if (onTitle) onTitle ((f == juce::File() ? juce::String ("Untitled") : f.getFileNameWithoutExtension()) + (session.hasUnsavedChanges() ? " *" : "") + "  -  " VEGA_APP_NAME);
    }
    void showMixer (bool m)
    {
        if (mixerFloat)
        {   // floating: Tab / MIXER show and hide its window
            if (mixerWin == nullptr) return;
            if (m) { mixerWin->setVisible (true); mixerWin->toFront (true); mixer.rebuild(); }
            else mixerWin->setVisible (false);
            transport.setViewLabel (false, launcher.isVisible());
            return;
        }
        mixer.setVisible (m); launcher.setVisible (false); arrange.setVisible (! m); transport.setViewLabel (m, false); if (m) mixer.rebuild();
    }
    void showSession (bool on)
    {
        launcher.setVisible (on); if (! mixerFloat) mixer.setVisible (false); arrange.setVisible (! on); transport.setViewLabel (false, on);
       #if VEGA_FL49
        // clip launcher: the hardware strip along the bottom; arrangement: the FLkey dock on the right
        stripShown = on; fl49Shown = ! on; resized();
       #endif
        if (on) launcher.grabKeyboardFocus();
    }
    const char* launchQuantName()
    {
        switch (session.edit().getLaunchQuantisation().type.get())
        {
            case te::LaunchQType::none: return "IMMEDIATE";
            case te::LaunchQType::eightBars: return "8 BARS";
            case te::LaunchQType::fourBars: return "4 BARS";
            case te::LaunchQType::twoBars: return "2 BARS";
            case te::LaunchQType::bar: return "BAR QUANTIZED";
            case te::LaunchQType::half: return "1/2 QUANTIZED";
            case te::LaunchQType::quarter: return "BEAT QUANTIZED";
            case te::LaunchQType::eighth: return "1/8 QUANTIZED";
            case te::LaunchQType::sixteenth: return "1/16 QUANTIZED";
            default: return "QUANTIZED";
        }
    }
    bool mixerShown() const { return mixerFloat ? (mixerWin != nullptr && mixerWin->isVisible()) : mixer.isVisible(); }
    bool pianoShown() const { return pianoFloat ? (pianoWin != nullptr && pianoWin->isVisible()) : pianoRoll.isVisible(); }

    // ---------------------------------------------------------------- floating mixer / piano roll (like FL Studio)
    /** A view in its own window: stays above the DAW while it's in use, passes keys (space, R, Tab...) to the DAW */
    struct FloatWindow : public juce::DocumentWindow, private juce::Timer
    {
        FloatWindow (const juce::String& title, juce::Component& c, juce::Rectangle<int> b, juce::Point<int> defaultSize, bool show)
            : DocumentWindow (title, VegaColours::background, DocumentWindow::closeButton | DocumentWindow::minimiseButton | DocumentWindow::maximiseButton)
        {
            setUsingNativeTitleBar (true);
            setContentNonOwned (&c, false);
            setResizable (true, false); setResizeLimits (520, 260, 10000, 10000);
            const auto screen = juce::Desktop::getInstance().getDisplays().getTotalBounds (true);
            if (! b.isEmpty() && screen.intersects (b)) setBounds (b); else centreWithSize (defaultSize.x, defaultSize.y);
            c.setVisible (true);
            setVisible (show);
            startTimer (300);
        }
        ~FloatWindow() override { clearContentComponent(); }
        std::function<void()> onCloseButton;
        std::function<bool (const juce::KeyPress&)> onKey;
        void closeButtonPressed() override { if (onCloseButton) onCloseButton(); }
        bool keyPressed (const juce::KeyPress& k) override { return onKey != nullptr && onKey (k); }
        void timerCallback() override
        {   // on top of the DAW while it's the app in use; not on top of other apps
            const bool fg = juce::Process::isForegroundProcess();
            if (isAlwaysOnTop() != fg) setAlwaysOnTop (fg);
        }
    };
    std::unique_ptr<FloatWindow> mixerWin, pianoWin;
    bool mixerFloat = false, pianoFloat = false;
    juce::Rectangle<int> mixerWinBounds, pianoWinBounds;
    static juce::File uiFile() { return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (VEGA_APP_NAME).getChildFile ("windows.xml"); }
    void saveUi()
    {
        if (mixerWin != nullptr) mixerWinBounds = mixerWin->getBounds();
        if (pianoWin != nullptr) pianoWinBounds = pianoWin->getBounds();
        juce::XmlElement x ("VegaWindows");
        x.setAttribute ("mixerFloat", mixerFloat); x.setAttribute ("pianoFloat", pianoFloat);
        x.setAttribute ("mixerBounds", mixerWinBounds.toString()); x.setAttribute ("pianoBounds", pianoWinBounds.toString());
        uiFile().getParentDirectory().createDirectory();
        x.writeTo (uiFile());
    }
    void loadUi()
    {
        if (auto x = juce::XmlDocument::parse (uiFile()))
        {
            mixerWinBounds = juce::Rectangle<int>::fromString (x->getStringAttribute ("mixerBounds"));
            pianoWinBounds = juce::Rectangle<int>::fromString (x->getStringAttribute ("pianoBounds"));
            if (x->getBoolAttribute ("pianoFloat")) setPianoFloating (true, false);
            if (x->getBoolAttribute ("mixerFloat")) setMixerFloating (true, false);
        }
    }
    void setMixerFloating (bool f, bool showNow = true)
    {
        if (f == mixerFloat) return;
        if (f)
        {
            const bool was = mixer.isVisible();
            mixer.setVisible (false); arrange.setVisible (! launcher.isVisible()); transport.setViewLabel (false, launcher.isVisible());
            removeChildComponent (&mixer);
            mixerFloat = true; mixer.setDetached (true);
            mixerWin = std::make_unique<FloatWindow> ("Vega Productions - Mixer", mixer, mixerWinBounds, juce::Point<int> (1500, 860), showNow || was);
            mixerWin->onCloseButton = [this] { showMixer (false); };
            mixerWin->onKey = [this] (const juce::KeyPress& k) { return keyPressed (k); };
            mixer.rebuild();
        }
        else
        {
            if (mixerWin != nullptr) { mixerWinBounds = mixerWin->getBounds(); mixerWin.reset(); }
            mixerFloat = false; mixer.setDetached (false);
            addChildComponent (mixer);
            resized();
            showMixer (showNow);
        }
        saveUi();
    }
    void setPianoFloating (bool f, bool showNow = true)
    {
        if (f == pianoFloat) return;
        const bool shown = pianoShown();
        if (f)
        {
            pianoRoll.setVisible (false); divider.setVisible (false);
            removeChildComponent (&pianoRoll);
            pianoFloat = true; pianoRoll.setDetached (true);
            pianoWin = std::make_unique<FloatWindow> ("Vega Productions - Piano Roll", pianoRoll, pianoWinBounds, juce::Point<int> (1200, 640), showNow && shown);
            pianoWin->onCloseButton = [this] { showPianoRoll (false); };
            pianoWin->onKey = [this] (const juce::KeyPress& k) { return keyPressed (k); };
            resized();
        }
        else
        {
            if (pianoWin != nullptr) { pianoWinBounds = pianoWin->getBounds(); pianoWin.reset(); }
            pianoFloat = false; pianoRoll.setDetached (false);
            addChildComponent (pianoRoll);
            showPianoRoll (showNow && shown);
        }
        saveUi();
    }

    // ---------------------------------------------------------------- export / bounce / markers
    void showExport (bool stems)
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = stems ? "Export stems" : "Export";
        o.content.setOwned (new ExportDialog (session, stems));
        o.dialogBackgroundColour = juce::Colour (0xff1b1b1d); o.resizable = false; o.useNativeTitleBar = true;
        o.launchAsync();
    }
    /** File > Import phone sketch: a .vegasketch sent from Vega Sketch on Android (Quick Share, Google Drive, USB) */
    void importPhoneSketch()
    {
        auto start = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
        chooser = std::make_unique<juce::FileChooser> ("Import a phone sketch", start.isDirectory() ? start : juce::File(), "*.vegasketch;*.zip");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            auto* w = new juce::AlertWindow ("Import phone sketch", f.getFileNameWithoutExtension()
                + "\n\nTracks, clips, scenes and tempo come across. Drum and keys tracks can use your desktop instruments "
                  "(Vega Productions Drums / Multisynth if installed) as MIDI you can edit, or the phone's own sound as audio.", juce::MessageBoxIconType::NoIcon);
            w->addComboBox ("sounds", { "Desktop instruments (MIDI, editable)", "Phone sounds (audio, exactly as on the phone)", "Both (phone audio muted underneath)" }, "Sounds");
            w->addComboBox ("where", { "Into a new project", "Into this project" }, "Where");
            w->addComboBox ("arr", { "Clip launcher + the scenes laid out in the arrangement", "Clip launcher only" }, "Layout");
            w->addButton ("Import", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, f] (int r)
            {
                std::unique_ptr<juce::AlertWindow> owner (w);
                if (r != 1) return;
                DawSession::SketchImport opt;
                const int snd = w->getComboBoxComponent ("sounds")->getSelectedItemIndex();
                opt.sounds = snd == 1 ? DawSession::SketchImport::Sounds::phone : snd == 2 ? DawSession::SketchImport::Sounds::both : DawSession::SketchImport::Sounds::desktop;
                opt.toArrangement = w->getComboBoxComponent ("arr")->getSelectedItemIndex() == 0;
                const bool fresh = w->getComboBoxComponent ("where")->getSelectedItemIndex() == 0;
                auto run = [this, f, opt, fresh]
                {
                    auto o = opt;
                    if (fresh) { session.newProject(); for (auto* t : session.getTracks()) session.deleteTrack (*t); }
                    else o.adoptTempo = false;
                    auto res = session.importSketch (f, o);
                    arrange.rebuildHeaders(); mixer.rebuild(); transport.refreshFromEdit(); setupInputs();
                    juce::AlertWindow::showMessageBoxAsync (res.ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "Import phone sketch", res.message);
                    if (res.ok) showSession (true);
                };
                if (fresh) confirmDiscard (run); else run();
            }), false);
        });
    }

    void showSongMap()
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Markers & tempo";
        o.content.setOwned (new SongMap (vs));
        o.dialogBackgroundColour = juce::Colour (0xff1b1b1d); o.resizable = true; o.useNativeTitleBar = true;
        o.launchAsync();
    }
    /** Bounce in place / consolidate over the loop range (when looping) or everything on the track */
    void renderTrack (te::AudioTrack& t, bool bounce)
    {
        auto& tr = session.edit().getTransport();
        te::TimeRange range;
        if (tr.looping && tr.getLoopRange().getLength() > 0s) range = tr.getLoopRange();
        else
        {
            bool any = false;
            for (auto* c : t.getClips()) { range = any ? range.getUnionWith (c->getPosition().time) : c->getPosition().time; any = true; }
            if (! any) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, bounce ? "Bounce" : "Consolidate", "There are no clips on this track."); return; }
        }
        if (bounce)
        {
            if (auto* nt = session.bounceToNewTrack (t, range)) vs.selectTrack (nt);
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Bounce", "The bounce didn't work.");
        }
        else if (auto* c = session.consolidate (t, range)) vs.selectClip (c);
        arrange.rebuildHeaders(); mixer.rebuild();
    }

    // ---------------------------------------------------------------- recent files + autosave
    juce::File prefsFile (const juce::String& name) { return engine.getPropertyStorage().getAppPrefsFolder().getChildFile (name); }
    juce::Array<juce::File> recentFiles()
    {
        juce::Array<juce::File> r;
        for (auto& l : juce::StringArray::fromLines (prefsFile ("VegaRecent.txt").loadFileAsString()))
            if (l.isNotEmpty() && juce::File (l).existsAsFile() && r.size() < 10) r.add (juce::File (l));
        return r;
    }
    void addRecent (const juce::File& f)
    {
        auto list = recentFiles(); list.removeFirstMatchingValue (f); list.insert (0, f);
        juce::StringArray lines; for (auto& x : list) lines.add (x.getFullPathName());
        prefsFile ("VegaRecent.txt").replaceWithText (lines.joinIntoString ("\n"));
    }
    juce::File autosaveFile()
    {
        const auto pf = session.getProjectFile();
        if (pf != juce::File()) return pf.getSiblingFile (pf.getFileNameWithoutExtension() + " (autosave).vegaproj");
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Productions").getChildFile ("Autosave");
        d.createDirectory();
        return d.getChildFile ("Untitled (autosave).vegaproj");
    }
    void autosave()
    {
        if (! session.hasUnsavedChanges() || session.edit().getTransport().isRecording()) return;
        const auto f = autosaveFile();
        te::EditFileOperations ops (session.edit());
        if (ops.writeToFile (f, false))
            prefsFile ("VegaAutosave.txt").replaceWithText (f.getFullPathName() + "\n" + session.getProjectFile().getFullPathName());
    }
public:
    /** Saved (or quit cleanly): the autosave isn't needed any more */
    void clearAutosave()
    {
        auto lines = juce::StringArray::fromLines (prefsFile ("VegaAutosave.txt").loadFileAsString());
        if (lines.size() > 0 && lines[0].isNotEmpty()) juce::File (lines[0]).deleteFile();
        prefsFile ("VegaAutosave.txt").deleteFile();
    }
private:
    void offerRecovery()
    {
        auto lines = juce::StringArray::fromLines (prefsFile ("VegaAutosave.txt").loadFileAsString());
        if (lines.size() < 1 || ! juce::File (lines[0]).existsAsFile()) return;
        const juce::File autosaved (lines[0]), original (lines[1]);
        auto opts = juce::MessageBoxOptions().withTitle ("Recover your work?").withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withMessage (VEGA_APP_NAME " didn't close normally last time. There's an autosave of "
                          + (original != juce::File() ? "\"" + original.getFileNameWithoutExtension() + "\"" : juce::String ("an unsaved project"))
                          + " from " + juce::File (lines[0]).getLastModificationTime().toString (true, true) + ".")
            .withButton ("Recover", 1).withButton ("Discard it", 0);
        juce::AlertWindow::showAsync (opts, [this, autosaved, original] (int r)
        {
            if (r != 1) { clearAutosave(); return; }
            if (session.openProject (autosaved))
            {
                if (original != juce::File()) session.setProjectFile (original);
                session.edit().markAsChanged();
                updateTitle();
            }
        });
    }

    // ---------------------------------------------------------------- AI stem separation
    void separateFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a song to separate", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) startSeparation (fc.getResult(), fc.getResult().getFileNameWithoutExtension(), nullptr);
        });
    }

    void startSeparation (const juce::File& source, const juce::String& baseName, te::WaveAudioClip::Ptr clip)
    {
        if (! source.existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Separate stems", "The audio file for this clip can't be found:\n" + source.getFullPathName());
            return;
        }
        auto opts = juce::MessageBoxOptions().withTitle ("Separate stems").withIconType (juce::MessageBoxIconType::NoIcon)
            .withMessage ("Split \"" + baseName + "\" into separate tracks with AI (Demucs v4).\n\n"
                          "4 stems: Vocals, Drums, Bass, Other\n6 stems: also Guitar and Piano (slightly lower quality)\n\n"
                          "It runs on your computer. Expect roughly 1-2x the song's length on a modern CPU.")
            .withButton ("4 stems", 1).withButton ("6 stems", 2).withButton ("Cancel", 0);
        juce::AlertWindow::showAsync (opts, [this, source, baseName, clip] (int r)
        {
            if (r == 0) return;
            const auto model = r == 2 ? StemSeparator::Model::sixStems : StemSeparator::Model::fourStems;
            ensureModel (model, [this, source, baseName, clip, model] { runSeparation (model, source, baseName, clip); });
        });
    }

    void ensureModel (StemSeparator::Model m, std::function<void()> then)
    {
        if (StemSeparator::hasModel (m)) { then(); return; }
        auto opts = juce::MessageBoxOptions().withTitle ("Download the separation model").withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withMessage ("Stem separation needs the Demucs v4 model (" + StemSeparator::modelSizeText (m) + "), downloaded once from Hugging Face "
                          "(huggingface.co/datasets/Retrobear/demucs.cpp) and kept in:\n" + StemSeparator::modelsFolder().getFullPathName() + "\n\n"
                          "The Demucs weights are released by Meta for research / non-commercial use.")
            .withButton ("Download", 1).withButton ("Cancel", 0);
        juce::AlertWindow::showAsync (opts, [this, m, then] (int r)
        {
            if (r != 1) return;
            struct Downloader : juce::ThreadWithProgressWindow
            {
                Downloader (StemSeparator::Model mm) : ThreadWithProgressWindow ("Downloading the separation model...", true, true), model (mm) {}
                void run() override
                {
                    StemSeparator::modelsFolder().createDirectory();
                    auto tmp = StemSeparator::modelFile (model).withFileExtension ("part");
                    tmp.deleteFile();
                    auto in = StemSeparator::modelUrl (model).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                                    .withConnectionTimeoutMs (20000).withNumRedirectsToFollow (8));
                    if (in == nullptr) { error = "Couldn't connect to Hugging Face. Check your internet connection."; return; }
                    std::unique_ptr<juce::FileOutputStream> out (tmp.createOutputStream());
                    if (out == nullptr) { error = "Couldn't write to " + tmp.getFullPathName(); return; }
                    const auto total = in->getTotalLength();
                    juce::HeapBlock<char> buf (1 << 16);
                    juce::int64 done = 0;
                    while (! in->isExhausted() && ! threadShouldExit())
                    {
                        const int n = in->read (buf.getData(), 1 << 16);
                        if (n <= 0) break;
                        out->write (buf.getData(), (size_t) n); done += n;
                        if (total > 0) setProgress ((double) done / (double) total);
                        setStatusMessage (juce::String (done / (1024 * 1024)) + " MB" + (total > 0 ? " of " + juce::String (total / (1024 * 1024)) + " MB" : juce::String()));
                    }
                    out.reset();
                    if (threadShouldExit()) { tmp.deleteFile(); error = "cancelled"; return; }
                    if (done < 10 * 1024 * 1024 || (total > 0 && done != total)) { tmp.deleteFile(); error = "The download didn't complete. Please try again."; return; }
                    tmp.moveFileTo (StemSeparator::modelFile (model));
                }
                StemSeparator::Model model; juce::String error;
            } dl (m);
            dl.runThread();
            if (dl.error == "cancelled") return;
            if (dl.error.isNotEmpty()) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Download", dl.error); return; }
            then();
        });
    }

    void runSeparation (StemSeparator::Model m, const juce::File& source, const juce::String& baseName, te::WaveAudioClip::Ptr clip)
    {
        auto folder = session.getProjectFile() != juce::File() ? session.getProjectFile().getParentDirectory().getChildFile ("Stems")
                                                             : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Productions").getChildFile ("Stems");
        auto outDir = folder.getChildFile (juce::File::createLegalFileName (baseName));
        struct Job : juce::ThreadWithProgressWindow
        {
            Job (te::Engine& e, StemSeparator::Model mm, juce::File src, juce::File out) : ThreadWithProgressWindow ("Separating stems...", true, true), engine (e), model (mm), source (src), outDir (out) {}
            void run() override
            {
                result = StemClient::separate (model, source, outDir,
                    [this] (float p, const juce::String& msg) { if (p >= 0) { setProgress (p); setStatusMessage (msg); } return ! threadShouldExit(); });
            }
            te::Engine& engine; StemSeparator::Model model; juce::File source, outDir; StemSeparator::Result result;
        } job (engine, m, source, outDir);
        const bool finished = job.runThread (juce::Thread::Priority::normal);
        if (! finished || (! job.result.ok && job.result.error.isEmpty())) return;    // cancelled
        if (! job.result.ok) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Separate stems", job.result.error); return; }
        auto tracks = session.placeStems (job.result.stems, StemSeparator::stemNames (m), baseName, clip.get(),
                                          clip == nullptr ? session.edit().getTransport().getPosition() : te::TimePosition());
        arrange.rebuildHeaders(); mixer.rebuild();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Separate stems",
            juce::String (tracks.size()) + " stem tracks added" + (clip != nullptr ? " (the original clip is muted)." : ".") + "\n\nThe WAV files are in:\n" + outDir.getFullPathName());
    }

    // ---------------------------------------------------------------- Mackie Control (Tracktion's built-in MCU driver)
    te::ExternalController* mcu()
    {
        for (auto* c : engine.getExternalControllerManager().getControllers())
            if (c->getControlSurfaceIfType<te::MackieMCU>() != nullptr) return c;
        return nullptr;
    }
    bool mcuEnabled() { auto* c = mcu(); return c != nullptr && c->isEnabled() && c->getMidiInputDevice (0).isNotEmpty() && c->getMidiInputDevice (0) != te::ExternalController::getNoDeviceSelectedMessage(); }
    bool isControllerPort (const juce::String& name)
    {
        for (auto* c : engine.getExternalControllerManager().getControllers())
            if (c->isEnabled() && c->getMidiInputDevice (0) == name) return true;
        return false;
    }
    /** First run: pick up an MCU-style surface by name */
    void autoDetectMcu()
    {
        auto* c = mcu();
        if (c == nullptr || mcuEnabled() || engine.getPropertyStorage().getAppPrefsFolder().getChildFile ("VegaMcuAsked").existsAsFile()) return;
        static const char* hints[] { "X-Touch", "MCU", "Mackie", "iCON", "Platform", "QCon", "FaderPort", "Nektar" };
        for (auto& in : c->getMidiInputPorts())
            for (auto* h : hints)
                if (in.containsIgnoreCase (h) && ! ControllerHub::looksLikeDawPort (in))
                {
                    juce::String out;
                    for (auto& o : c->getMidiOutputPorts()) if (o.containsIgnoreCase (h)) { out = o; break; }
                    c->setMidiInputDevice (0, in); if (out.isNotEmpty()) c->setBackChannelDevice (0, out); c->setEnabled (true);
                    setupInputs();
                    return;
                }
    }
    void showMcuSettings()
    {
        auto* c = mcu();
        if (c == nullptr) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Mackie Control", "Mackie Control support isn't available in this build."); return; }
        auto* w = new juce::AlertWindow ("Mackie Control (MCU)",
            "For control surfaces in Mackie Control mode: Behringer X-Touch / X-Touch One, iCON Platform, Mackie MCU Pro, "
            "QCon, FaderPort (MCU mode)... Motor faders, V-pots, transport, banking, track names and meters follow the mixer.", juce::MessageBoxIconType::NoIcon);
        auto ins = c->getMidiInputPorts(), outs = c->getMidiOutputPorts();
        ins.insert (0, "(off)"); outs.insert (0, "(none)");
        w->addComboBox ("in", ins, "MIDI input from the surface");
        w->addComboBox ("out", outs, "MIDI output to the surface (faders, lights, display)");
        if (auto* cb = w->getComboBoxComponent ("in")) { const int i = ins.indexOf (c->getMidiInputDevice (0)); cb->setSelectedItemIndex (c->isEnabled() && i > 0 ? i : 0); }
        if (auto* cb = w->getComboBoxComponent ("out")) { const int i = outs.indexOf (c->getBackChannelDevice (0)); cb->setSelectedItemIndex (i > 0 ? i : 0); }
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            std::unique_ptr<juce::AlertWindow> owner (w);
            engine.getPropertyStorage().getAppPrefsFolder().getChildFile ("VegaMcuAsked").replaceWithText ("1");
            if (r != 1) return;
            auto* c2 = mcu(); if (c2 == nullptr) return;
            const auto in = w->getComboBoxComponent ("in")->getText(), out = w->getComboBoxComponent ("out")->getText();
            if (w->getComboBoxComponent ("in")->getSelectedItemIndex() <= 0) { c2->setEnabled (false); setupInputs(); return; }
            c2->setMidiInputDevice (0, in);
            c2->setBackChannelDevice (0, w->getComboBoxComponent ("out")->getSelectedItemIndex() > 0 ? out : te::ExternalController::getNoDeviceSelectedMessage());
            c2->setEnabled (true);
            setupInputs();
        }), false);
    }

    void showApcSettings()
    {
        setupInputs();
        juce::String keys = "not found - check it's plugged in";
        for (auto& d : engine.getDeviceManager().getMidiInDevices())
            if (d->getName().containsIgnoreCase ("APC Key 25") && ! ApcHub::isApcControlPort (d->getName()))
            {
                juce::String target = "not connected to a track";
                for (auto* in : session.edit().getAllInputDevices())
                    if (&in->getInputDevice() == d.get())
                        for (auto* t : session.getTracks()) if (te::isOnTargetTrack (*in, *t, 0)) target = "playing \"" + t->getName() + "\"";
                keys = "\"" + d->getName() + "\" " + (d->isEnabled() ? "on, " + target : juce::String ("switched OFF in Audio / MIDI settings"));
            }
        auto* w = new juce::AlertWindow ("Akai APC Key 25 mk2",
            juce::String ("Pads / knobs: ") + (apc.isConnected() ? "connected to " + apc.inputName : juce::String ("not connected")) + "\n"
            "Keys: " + keys + "\n\n"
            "Pads launch the clip launcher's clips (the box in the SESSION view shows which 8 x 5). Scene buttons launch scenes; "
            "Shift + scene button picks what the 8 track buttons do (Clip Stop / Solo / Mute / Rec Arm / Select). "
            "Shift + arrows move the box, Shift + Volume / Pan / Send / Device pick what the knobs do. "
            "Stop All Clips stops the launcher (Shift: back to arrangement). The keys play the selected instrument track.\n\n"
            "Pick the APC's second (control) port - on Windows \"MIDIIN2 (APC Key 25 mk2)\" and \"MIDIOUT2 (APC Key 25 mk2)\".", juce::MessageBoxIconType::NoIcon);
        juce::StringArray ins { "(off)" }, outs { "(none)" };
        for (auto& d : juce::MidiInput::getAvailableDevices()) ins.add (d.name);
        for (auto& d : juce::MidiOutput::getAvailableDevices()) outs.add (d.name);
        w->addComboBox ("in", ins, "Control input");
        w->addComboBox ("out", outs, "Lights output");
        if (auto* cb = w->getComboBoxComponent ("in")) cb->setSelectedItemIndex (std::max (0, ins.indexOf (apc.inputName)));
        if (auto* cb = w->getComboBoxComponent ("out")) cb->setSelectedItemIndex (std::max (0, outs.indexOf (apc.outputName)));
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Auto-detect", 2); w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
        {
            std::unique_ptr<juce::AlertWindow> owner (w);
            if (r == 2) { apc.autoConnect (true); setupInputs(); if (! apc.isConnected()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "APC Key 25", apc.lastError.isNotEmpty() ? apc.lastError
                : juce::String ("The APC's control port (\"MIDIIN2 (APC Key 25 mk2)\") isn't showing. Unplug the APC, plug it back in, close any other app using it, then press Auto-detect again.")); return; }
            if (r != 1) return;
            auto* in = w->getComboBoxComponent ("in"); auto* out = w->getComboBoxComponent ("out");
            const bool ok = apc.connect (in->getSelectedItemIndex() > 0 ? in->getText() : juce::String(), out->getSelectedItemIndex() > 0 ? out->getText() : juce::String(), true);
            setupInputs();
            if (! ok && apc.lastError.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "APC Key 25", apc.lastError);
            else if (ok && apc.inputName != in->getText()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "APC Key 25",
                "\"" + in->getText() + "\" is the APC's keys port, so the pads and knobs were connected to \"" + apc.inputName + "\" instead. The keys keep playing your instrument.");
        }), false);
    }

    void showMappings()
    {
        auto& pcm = session.edit().getParameterControlMappings();
        juce::PopupMenu m;
        m.addSectionHeader ("MIDI learn mappings (click one to remove it)");
        int rows = 0;
        for (int i = 0; i < pcm.getNumControllerIDs(); ++i)
        {
            auto mp = pcm.getMappingForRow (i);
            if (mp.parameter == nullptr) continue;
            auto txt = pcm.getTextForRow (i);
            m.addItem (1 + i, txt.first + "   ->   " + mp.parameter->getFullName());
            ++rows;
        }
        if (rows == 0) m.addItem (-1, "No mappings yet - press LEARN in the transport bar", false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&transport), [this] (int r)
        {
            if (r >= 1) session.edit().getParameterControlMappings().removeMapping (r - 1);
        });
    }

    void showBrowser (bool show) { browser.setVisible (show); transport.setBrowserShown (show); resized(); }

    /** Load a browser item: instruments go on the target / selected instrument track (or a new one),
        effects go at the end of the target / selected track's chain. The plug-in window opens right away. */
    void loadFromBrowser (const PluginBrowser::Item& it, te::AudioTrack* target, bool dropped = false)
    {
        auto* t = target != nullptr ? target : (dropped ? nullptr : vs.getSelectedTrack());
        te::Plugin::Ptr p;
        if (it.instrument)
        {
            if (t == nullptr || DawSession::kindOf (*t) != DawSession::TrackKind::midi)
                t = session.addTrack (DawSession::TrackKind::midi, it.name);
            if (t == nullptr) return;
            p = it.desc ? session.setInstrument (*t, {}, &*it.desc) : session.setInstrument (*t, it.builtInType);
        }
        else
        {
            if (t == nullptr)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, it.name,
                    "Select a track first (click its name), or drag the effect onto a track or mixer strip.");
                return;
            }
            p = it.desc ? session.addPlugin (*t, {}, &*it.desc) : session.addPlugin (*t, it.builtInType);
        }
        vs.selectTrack (t);
        if (p == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, it.name, "Couldn't load this plug-in.");
            return;
        }
        arrange.rebuildHeaders(); mixer.rebuild();
        dawui::openPlugin (p.get());
    }

    void showPianoRoll (bool show)
    {
        if (pianoFloat)
        {
            if (pianoWin == nullptr) return;
            if (show) { pianoWin->setVisible (true); pianoWin->toFront (true); pianoRoll.grabKeyboardFocus(); }
            else pianoWin->setVisible (false);
            return;
        }
        pianoRoll.setVisible (show); divider.setVisible (show); resized(); if (show) pianoRoll.grabKeyboardFocus();
    }

    void setupInputs()
    {
        auto& dm = engine.getDeviceManager();
        for (auto& midiIn : dm.getMidiInDevices())
        {
            // controller "DAW" ports carry HUI control data, not notes - leave them to the controller script
            if (ControllerHub::looksLikeDawPort (midiIn->getName()) || ApcHub::isApcControlPort (midiIn->getName())
                 || (hub.inputName.isNotEmpty() && midiIn->getName() == hub.inputName)) { midiIn->setEnabled (false); continue; }
            if (isControllerPort (midiIn->getName())) continue;     // Mackie Control port: owned by the MCU driver, not a note input
            // always audible: the keys only ever feed the selected instrument track, armed or not
            midiIn->setMonitorMode (te::InputDevice::MonitorMode::on); midiIn->setEnabled (true);
        }
        session.edit().getTransport().ensureContextAllocated();
        engine.getExternalControllerManager().setCurrentEdit (&session.edit(), &selection);
        session.edit().getAutomationRecordManager().setWritingAutomation (false);   // automation write always starts off
        routeMidiToSelectedTrack();
    }
    // play your keyboard into whichever instrument track is selected
    void routeMidiToSelectedTrack()
    {
        auto* t = vs.getSelectedTrack();
        if (t == nullptr || DawSession::kindOf (*t) != DawSession::TrackKind::midi)
        {
            for (auto* tr : session.getTracks()) if (DawSession::kindOf (*tr) == DawSession::TrackKind::midi) { t = tr; break; }
            if (t == nullptr) return;
        }
        for (auto* in : session.edit().getAllInputDevices())
        {
            if (! isKeysInput (*in) || isControllerPort (in->getInputDevice().getName()) || te::isOnTargetTrack (*in, *t, 0)) continue;
            [[maybe_unused]] auto r = in->setTarget (t->itemID, true, nullptr, 0);
        }
        stripRawFl49Targets();
    }
    /** A note input that should follow the selected track: a physical keyboard, or (VEGA FL49) the processed FLkey keys */
    bool isKeysInput (te::InputDeviceInstance& in)
    {
        auto& dev = in.getInputDevice();
       #if VEGA_FL49
        if (dev.getDeviceType() == te::InputDevice::virtualMidiDevice) return dev.getName() == Fl49Hub::virtualName;
        if (fl49.isConnected() && Fl49Hub::isKeysPort (dev.getName())) return false;     // its notes arrive through "VEGA FL49 Keys"
       #endif
        return dev.getDeviceType() == te::InputDevice::physicalMidiDevice;
    }
    /** VEGA FL49: the FLkey's raw keys port must not feed any track (it would double every note) */
    void stripRawFl49Targets()
    {
       #if VEGA_FL49
        if (! fl49.isConnected()) return;
        for (auto* in : session.edit().getAllInputDevices())
            if (in->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice && Fl49Hub::isKeysPort (in->getInputDevice().getName()))
                for (auto id : in->getTargets()) [[maybe_unused]] auto r = in->removeTarget (id, nullptr);
       #endif
    }

    void open()
    {
        confirmDiscard ([this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Open project", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.vegaproj;*.tracktionedit");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().existsAsFile() && ! session.openProject (fc.getResult()))
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open", "Couldn't open " + fc.getResult().getFileName());
                else if (fc.getResult().existsAsFile()) addRecent (fc.getResult());
            });
        });
    }
public:
    void openFile (const juce::File& f) { if (f.existsAsFile() && session.openProject (f)) addRecent (f); }
    void save (bool as, std::function<void()> then = nullptr)
    {
        if (! as && session.saveProject()) { addRecent (session.getProjectFile()); clearAutosave(); if (then) then(); return; }
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Productions").createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save project", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Productions").getChildFile ("My Song.vegaproj"), "*.vegaproj");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this, then] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File()) return;
            if (session.saveProjectAs (fc.getResult())) { addRecent (session.getProjectFile()); clearAutosave(); if (then) then(); }
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save", "Couldn't save the project.");
        });
    }
private:
    void importFiles()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import audio or MIDI", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.mid;*.midi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [this] (const juce::FileChooser& fc)
        {
            juce::StringArray paths; for (auto& f : fc.getResults()) paths.add (f.getFullPathName());
            if (! paths.isEmpty()) arrange.filesDropped (paths, ArrangeView::headerW + 2, ArrangeView::rulerH + arrange.totalTracksHeight() + 10);
        });
    }
    void showAudioSettings()
    {
        auto* panel = new juce::Component();
        auto* sel = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager, 0, 64, 2, 64, true, false, true, false);
        sel->setSize (560, 520); panel->addAndMakeVisible (sel);
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Audio / MIDI settings"; o.content.setOwned (sel);
        o.dialogBackgroundColour = juce::Colour (0xff1e1e1e); o.resizable = true; o.useNativeTitleBar = true;
        delete panel;
        o.launchAsync();
        juce::Timer::callAfterDelay (500, [this] { setupInputs(); });
    }
    void showPluginList()
    {
        auto& pm = engine.getPluginManager();
        auto dead = engine.getPropertyStorage().getAppCacheFolder().getChildFile ("PluginScanDeadMansPedal");
        auto* list = new juce::PluginListComponent (pm.pluginFormatManager, pm.knownPluginList, dead, nullptr, true);
        list->setSize (760, 560);
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Plugins - press Options > Scan for new or updated VST3 plug-ins";
        o.content.setOwned (list); o.dialogBackgroundColour = juce::Colour (0xff1e1e1e); o.resizable = true; o.useNativeTitleBar = true;
        o.launchAsync();
    }

    struct Divider : juce::Component
    {
        std::function<void (int)> onDrag; int last = 0;
        Divider() { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff2a2a2a)); g.setColour (VegaColours::textDim); g.fillRect (getWidth() / 2 - 20, 2, 40, 2); }
        void mouseDown (const juce::MouseEvent& e) override { last = e.getScreenY(); }
        void mouseDrag (const juce::MouseEvent& e) override { if (onDrag) onDrag (e.getScreenY() - last); last = e.getScreenY(); }
    };

    te::Engine& engine;
    DawLookAndFeel lnf;
    DawSession session; te::SelectionManager selection { engine }; ViewState vs; ControllerHub hub; ApcHub apc;
   #if VEGA_FL49
    Fl49Hub fl49 { engine, session };
    Fl49Panel fl49Panel { fl49, hub.surface, hub, vs };
    HardwareStrip strip { fl49, hub.surface, hub, vs };
    bool fl49Shown = true, stripShown = false;
   #endif
    juce::MenuBarComponent menuBar;
    TransportBar transport; ArrangeView arrange; MixerView mixer; SessionView launcher; PianoRoll pianoRoll; PluginBrowser browser; Divider divider;
    int browserW = 250;
    int pianoH = 320;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
class VegaDawApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return VEGA_APP_NAME; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        vegawin::makeGenericEditor = [] (te::Plugin& p) -> std::unique_ptr<te::Plugin::EditorComponent> { return std::make_unique<dawui::GenericEditor> (p); };
        engine = std::make_unique<te::Engine> (getApplicationName(), std::make_unique<vegawin::ExtendedUIBehaviour>(), nullptr);
        window = std::make_unique<Window> (*engine);
        const juce::File f (commandLine.unquoted().trim());
        if (f.hasFileExtension ("vegaproj")) window->main->openFile (f);
    }
    void shutdown() override { window.reset(); engine.reset(); }
    void systemRequestedQuit() override
    {
        if (window == nullptr) { quit(); return; }
        window->main->confirmDiscard ([this] { if (window) window->main->clearAutosave(); juce::JUCEApplication::getInstance()->quit(); });
    }
    void anotherInstanceStarted (const juce::String& cl) override
    {
        const juce::File f (cl.unquoted().trim());
        if (window && f.hasFileExtension ("vegaproj")) window->main->confirmDiscard ([this, f] { window->main->openFile (f); });
    }

    struct Window : public juce::DocumentWindow
    {
        explicit Window (te::Engine& e) : DocumentWindow (VEGA_APP_NAME, VegaColours::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            main = new MainComponent (e);
            main->onTitle = [this] (juce::String t) { setName (t); };
            setContentOwned (main, true);
            setResizable (true, false);
            setResizeLimits (1100, 640, 10000, 10000);
            centreWithSize (std::min (1600, juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea.getWidth() - 40),
                            std::min (950, juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea.getHeight() - 40));
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
        MainComponent* main = nullptr;
    };
private:
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (VegaDawApp)
