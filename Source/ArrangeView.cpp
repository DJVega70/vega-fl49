#include "ArrangeView.h"

//==============================================================================
class ArrangeView::TrackHeader : public juce::Component
{
public:
    TrackHeader (ArrangeView& o, te::AudioTrack& t) : owner (o), track (&t)
    {
        name.setText (t.getName(), juce::dontSendNotification);
        name.setEditable (false, true);
        name.setFont (VegaFonts::ui (13.0f, true));
        name.setColour (juce::Label::textColourId, VegaColours::text);
        name.onTextChange = [this] { if (track) track->setName (name.getText()); };
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);
        auto setup = [this] (juce::TextButton& b, const char* txt, juce::Colour on)
        {
            b.setButtonText (txt); b.setClickingTogglesState (false);
            b.setColour (juce::TextButton::buttonOnColourId, on);
            addAndMakeVisible (b);
        };
        setup (mute, "M", juce::Colour (0xffe0a030)); setup (solo, "S", juce::Colour (0xff48e1e8)); setup (arm, "R", juce::Colour (0xffe04848)); setup (input, "IN", VegaColours::accentDim);
        mute.onClick = [this] { track->setMute (! track->isMuted (false)); refresh(); };
        solo.onClick = [this] { track->setSolo (! track->isSolo (false)); refresh(); };
        arm.onClick = [this] { toggleArm(); };
        input.onClick = [this] { inputMenu(); };
        const bool bus = DawSession::kindOf (t) == DawSession::TrackKind::bus;
        arm.setVisible (! bus); input.setVisible (! bus);
        // plug-in access straight from the track: INST opens the instrument, FX lists everything on the track
        setup (instBtn, "INST", VegaColours::gold); setup (fxBtn, "FX", VegaColours::accentDim);
        instBtn.setTooltip ("Open this track's instrument (right-click to change it)");
        fxBtn.setTooltip ("Open or add plug-ins on this track");
        instBtn.setVisible (DawSession::kindOf (t) == DawSession::TrackKind::midi);
        instBtn.onClick = [this]
        {
            owner.vs.selectTrack (track.get());
            auto* first = track->pluginList[0];
            if (first != nullptr && first->isSynth() && ! juce::ModifierKeys::currentModifiers.isPopupMenu()) dawui::openPlugin (first);
            else dawui::pluginChainMenu (owner.vs.session, *track, &instBtn, [this] { repaint(); });
        };
        fxBtn.onClick = [this] { owner.vs.selectTrack (track.get()); dawui::pluginChainMenu (owner.vs.session, *track, &fxBtn, [this] { refresh(); }); };
        // automation lane
        setup (autoBtn, "A", VegaColours::accentDim);
        autoBtn.setTooltip ("Show / hide this track's automation lane");
        autoBtn.onClick = [this]
        {
            track->state.setProperty ("vegaAutoShown", ! (bool) track->state.getProperty ("vegaAutoShown", false), nullptr);
            owner.resized(); owner.repaint();
        };
        laneParamBox.onChange = [this]
        {
            const int i = laneParamBox.getSelectedId() - 1;
            if (juce::isPositiveAndBelow (i, laneParams.size())) track->state.setProperty ("vegaAutoParam", dawui::paramKey (*laneParams[i]), nullptr);
            owner.repaint();
        };
        addChildComponent (laneParamBox);
        // folders: collapse / expand
        setup (foldBtn, "", VegaColours::accentDim);
        foldBtn.setVisible (DawSession::isFolder (t));
        foldBtn.setTooltip ("Collapse / expand this folder");
        foldBtn.onClick = [this] { DawSession::setCollapsed (*track, ! DawSession::isCollapsed (*track)); owner.rebuildHeaders(); };
        // comping: show the take lanes
        setup (takesBtn, "TK", juce::Colour (0xffe8a23a));
        takesBtn.setTooltip ("Show / hide the take lanes (loop-record to get takes, then drag across a take to comp)");
        takesBtn.setVisible (false);
        takesBtn.onClick = [this] { track->state.setProperty ("vegaTakesShown", ! (bool) track->state.getProperty ("vegaTakesShown", false), nullptr); owner.resized(); owner.repaint(); };
        vol.setSliderStyle (juce::Slider::LinearBar); vol.setRange (-60.0, 6.0, 0.1); vol.setSkewFactorFromMidPoint (-12.0);
        vol.setTextValueSuffix (" dB"); vol.setColour (juce::Slider::trackColourId, DawSession::trackColour (t).withAlpha (0.45f));
        vol.onValueChange = [this] { if (auto vp = track->getVolumePlugin()) vp->setVolumeDb ((float) vol.getValue()); };
        vol.onDragStart = [this] { if (auto vp = track->getVolumePlugin()) dawui::gesture (vp->volParam.get(), true); };
        vol.onDragEnd   = [this] { if (auto vp = track->getVolumePlugin()) dawui::gesture (vp->volParam.get(), false); };
        addAndMakeVisible (vol);
        refresh();
    }
    void refresh()
    {
        if (track == nullptr) return;
        mute.setToggleState (track->isMuted (false), juce::dontSendNotification);
        solo.setToggleState (track->isSolo (false), juce::dontSendNotification);
        arm.setToggleState (isArmed(), juce::dontSendNotification);
        {
            int n = 0; for (auto* p : owner.vs.session.getInsertPlugins (*track)) if (! p->isSynth()) ++n;
            fxBtn.setButtonText (n > 0 ? "FX " + juce::String (n) : "FX");
        }
        if (auto vp = track->getVolumePlugin()) if (! vol.isMouseButtonDown()) vol.setValue (vp->getVolumeDb(), juce::dontSendNotification);
        if (name.getText() != track->getName() && ! name.isBeingEdited()) name.setText (track->getName(), juce::dontSendNotification);
        const bool hasTakes = ArrangeView::maxTakes (*track) > 1;
        if (takesBtn.isVisible() != hasTakes) { takesBtn.setVisible (hasTakes); resized(); }
        takesBtn.setToggleState (owner.takesHeight (*track) > 0, juce::dontSendNotification);
        foldBtn.setButtonText (DawSession::isCollapsed (*track) ? juce::String::fromUTF8 ("\xe2\x96\xb8") : juce::String::fromUTF8 ("\xe2\x96\xbe"));
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const bool sel = owner.vs.selectedTrack == track->itemID;
        // the mixer strips' charcoal panel
        g.setGradientFill (juce::ColourGradient (VegaColours::panelLight, 0, 0, VegaColours::panel, 0, (float) getHeight(), false));
        g.fillRect (getLocalBounds().reduced (0, 1));
        if (sel) { g.setColour (VegaColours::accent.withAlpha (0.06f)); g.fillRect (getLocalBounds().reduced (0, 1)); g.setColour (VegaColours::accent.withAlpha (0.5f)); g.drawRect (getLocalBounds().reduced (0, 1), 1); }
        auto& sess = owner.vs.session;
        const bool folder = DawSession::isFolder (*track);
        auto* parent = sess.folderOf (*track);
        if (parent != nullptr)
        {   // inside a folder: the folder's colour runs down the left, the track's own colour next to it
            g.setColour (DawSession::trackColour (*parent).withAlpha (0.55f)); g.fillRect (0, 0, 4, getHeight());
            g.setColour (DawSession::trackColour (*track)); g.fillRect (6, 1, 4, getHeight() - 2);
        }
        else
        {
            g.setColour (DawSession::trackColour (*track)); g.fillRect (0, 1, folder ? 8 : 5, getHeight() - 2);
            if (folder) { g.setColour (DawSession::trackColour (*track).withAlpha (0.10f)); g.fillRect (8, 1, getWidth() - 8, getHeight() - 2); }
        }
        g.setColour (VegaColours::line); g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
        const auto kind = DawSession::kindOf (*track);
        g.setFont (VegaFonts::ui (9.5f, true)); g.setColour (VegaColours::textDim);
        juce::String sub = folder ? "FOLDER  -  " + juce::String (sess.folderMembers (*track).size()) + " TRACKS"
                         : kind == DawSession::TrackKind::bus ? "BUS" : kind == DawSession::TrackKind::midi ? "INSTRUMENT" : "AUDIO";
        if (kind == DawSession::TrackKind::midi)
            if (auto* p = track->pluginList[0]; p != nullptr && p->isSynth()) sub = p->getName().toUpperCase();
        if (auto* out = owner.vs.session.getOutputBus (*track); out != nullptr && out != parent) sub << "  >  " << out->getName().toUpperCase();
        if (track->isFrozen (te::Track::individualFreeze))
        {
            g.setColour (juce::Colour (0xff7fd8ff)); g.setFont (VegaFonts::ui (9.0f, true));
            g.drawText (juce::String::fromUTF8 ("\xe2\x9d\x84 FROZEN"), getWidth() - 70, 26, 62, 12, juce::Justification::right);
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.5f, true));
        }
        g.drawText (sub, 12, 26, getWidth() - 20, 12, juce::Justification::left);
        if (const int tk = owner.takesHeight (*track); tk > 0)
        {
            const int ch = owner.clipHeight (*track);
            for (int i = 0; i * ArrangeView::takeLaneH < tk; ++i)
            {
                const int y = ch + i * ArrangeView::takeLaneH;
                g.setColour (juce::Colour (i % 2 ? 0xff1c1c1c : 0xff1f1f1f)); g.fillRect (10, y, getWidth() - 10, ArrangeView::takeLaneH - 1);
                g.setColour (juce::Colour (0xffe8a23a)); g.setFont (VegaFonts::ui (10.0f, true));
                g.drawText ("TAKE " + juce::String (i + 1), 16, y, getWidth() - 24, ArrangeView::takeLaneH, juce::Justification::centredLeft);
            }
        }
        if (const int lh = owner.laneHeight (*track); lh > 0)
        {
            const int ch = owner.clipHeight (*track) + owner.takesHeight (*track);
            g.setColour (juce::Colour (0xff1a1d1d)); g.fillRect (5, ch, getWidth() - 5, lh - 1);
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.0f, true));
            g.drawText ("AUTOMATION", 12, ch + 34, getWidth() - 20, 12, juce::Justification::left);
        }
    }
    void resized() override
    {
        auto top = juce::Rectangle<int> (8, 4, getWidth() - 16, 22);
        fxBtn.setBounds (top.removeFromRight (38).reduced (0, 1)); top.removeFromRight (3);
        autoBtn.setBounds (top.removeFromRight (24).reduced (0, 1)); top.removeFromRight (3);
        if (instBtn.isVisible()) { instBtn.setBounds (top.removeFromRight (40).reduced (0, 1)); top.removeFromRight (3); }
        if (takesBtn.isVisible()) { takesBtn.setBounds (top.removeFromRight (28).reduced (0, 1)); top.removeFromRight (3); }
        if (foldBtn.isVisible()) { foldBtn.setBounds (top.removeFromLeft (20).reduced (0, 2)); top.removeFromLeft (2); }
        if (owner.vs.session.folderOf (*track) != nullptr) top.removeFromLeft (6);
        name.setBounds (top);
        auto row = juce::Rectangle<int> (10, 42, getWidth() - 20, 22);
        for (auto* b : { &mute, &solo, &arm, &input }) { if (! b->isVisible()) continue; b->setBounds (row.removeFromLeft (b == &input ? 34 : 26)); row.removeFromLeft (4); }
        vol.setBounds (row.withTrimmedLeft (4));
        const int ch = owner.clipHeight (*track);
        if (ch < 70) { vol.setVisible (false); } else vol.setVisible (true);
        const bool lane = owner.laneHeight (*track) > 0;
        autoBtn.setToggleState (lane, juce::dontSendNotification);
        laneParamBox.setVisible (lane);
        if (lane) { refreshLaneParams(); laneParamBox.setBounds (10, ch + owner.takesHeight (*track) + 8, getWidth() - 20, 22); }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.vs.selectTrack (track.get());
        if (e.mods.isPopupMenu()) menu();
        dragStartY = e.getScreenY();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { name.showEditor(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        // drag the bottom edge to resize, or drag the header to reorder
        if (dragStartY - getScreenY() > getHeight() - 10 || resizing)
        {
            resizing = true;
            track->state.setProperty ("vegaHeight", juce::jlimit (44, 260, e.getScreenY() - getScreenY() - owner.laneHeight (*track) - owner.takesHeight (*track)), nullptr);
            owner.resized(); return;
        }
        auto p = owner.getLocalPoint (nullptr, e.getScreenPosition());
        if (auto* target = owner.trackAtY (p.y - rulerH))
            if (target != track)
            {
                auto tracks = owner.vs.session.getTracks();
                owner.vs.session.moveTrack (*track, tracks.indexOf (target));
            }
    }
    void mouseUp (const juce::MouseEvent&) override { resizing = false; }
    te::AudioTrack::Ptr track;
private:
    bool isArmed()
    {
        for (auto* in : track->edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, *track, 0) && in->isRecordingEnabled (track->itemID)) return true;
        return false;
    }
    void ensureInput()
    {
        // assign a default input if the track has none: first stereo/mono wave input for audio, all MIDI ins for instruments
        bool has = false;
        for (auto* in : track->edit.getAllInputDevices()) if (te::isOnTargetTrack (*in, *track, 0)) has = true;
        if (has) return;
        const bool midi = DawSession::kindOf (*track) == DawSession::TrackKind::midi;
        for (auto* in : track->edit.getAllInputDevices())
        {
            const auto type = in->getInputDevice().getDeviceType();
            if (midi ? type == te::InputDevice::physicalMidiDevice : type == te::InputDevice::waveDevice)
            {
                [[maybe_unused]] auto r = in->setTarget (track->itemID, ! midi, &track->edit.getUndoManager(), 0);
                if (! midi) break;
            }
        }
    }
    void toggleArm()
    {
        ensureInput();
        const bool now = ! isArmed();
        for (auto* in : track->edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, *track, 0)) in->setRecordingEnabled (track->itemID, now);
        refresh();
    }
    void inputMenu()
    {
        juce::PopupMenu m;
        int id = 1; std::vector<te::InputDeviceInstance*> list;
        for (auto* in : track->edit.getAllInputDevices())
        {
            const auto type = in->getInputDevice().getDeviceType();
            if (type != te::InputDevice::waveDevice && type != te::InputDevice::physicalMidiDevice) continue;
            const bool on = te::isOnTargetTrack (*in, *track, 0);
            m.addItem (id++, (type == te::InputDevice::waveDevice ? "Audio: " : "MIDI: ") + in->getInputDevice().getName(), true, on);
            list.push_back (in);
        }
        if (list.empty()) m.addItem (-1, "No inputs enabled - Options > Audio / MIDI Settings", false);
        m.addSeparator();
        m.addItem (900, "Monitor input (hear it live)", true, monitoring());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&input), [this, list] (int r)
        {
            if (r == 900)
            {
                for (auto* in : track->edit.getAllInputDevices())
                    if (te::isOnTargetTrack (*in, *track, 0))
                        in->getInputDevice().setMonitorMode (monitoring() ? te::InputDevice::MonitorMode::off : te::InputDevice::MonitorMode::on);
                return;
            }
            if (r < 1 || r > (int) list.size()) return;
            auto* in = list[(size_t) r - 1];
            if (te::isOnTargetTrack (*in, *track, 0)) [[maybe_unused]] auto res = in->removeTarget (track->itemID, &track->edit.getUndoManager());
            else [[maybe_unused]] auto res = in->setTarget (track->itemID, in->getInputDevice().getDeviceType() == te::InputDevice::waveDevice, &track->edit.getUndoManager(), 0);
            refresh();
        });
    }
    bool monitoring()
    {
        for (auto* in : track->edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, *track, 0)) return in->getInputDevice().getMonitorMode() == te::InputDevice::MonitorMode::on;
        return false;
    }
    void menu()
    {
        juce::PopupMenu m, colours, route;
        m.addItem (1, "Rename"); 
        static const juce::uint32 pal[] { 0xff48e1e8, 0xffe8b448, 0xff7ee87a, 0xffe8708a, 0xffa58bff, 0xff6fb7ff, 0xffffa25e, 0xff5ee8b6, 0xffcccccc };
        for (int i = 0; i < 9; ++i) colours.addColouredItem (100 + i, "Colour " + juce::String (i + 1), juce::Colour (pal[i]));
        m.addSubMenu ("Colour", colours);
        auto& s = owner.vs.session;
        route.addItem (200, "Master", true, s.getOutputBus (*track) == nullptr);
        auto buses = s.getBuses();
        for (int i = 0; i < buses.size(); ++i) if (buses[i] != track.get()) route.addItem (201 + i, buses[i]->getName(), true, s.getOutputBus (*track) == buses[i]);
        m.addSubMenu ("Output", route);
        const bool isFolderTrack = DawSession::isFolder (*track);
        juce::PopupMenu fold;
        juce::Array<te::AudioTrack*> folders;
        for (auto* t : s.getTracks()) if (DawSession::isFolder (*t)) folders.add (t);
        if (! isFolderTrack)
        {
            fold.addItem (300, "New folder with this track");
            for (int i = 0; i < folders.size(); ++i) fold.addItem (301 + i, folders[i]->getName(), true, s.folderOf (*track) == folders[i]);
            if (s.folderOf (*track) != nullptr) { fold.addSeparator(); fold.addItem (299, "Take out of folder"); }
            m.addSubMenu ("Folder", fold);
        }
        else
        {
            m.addItem (8, DawSession::isCollapsed (*track) ? "Expand folder" : "Collapse folder");
            m.addItem (9, "Ungroup folder (keep the tracks)");
        }
        if (! isFolderTrack) m.addItem (7, "Duplicate track");
        m.addSeparator();
        if (DawSession::kindOf (*track) != DawSession::TrackKind::bus)
        {
            m.addItem (10, "Bounce in place (loop range, or the whole track)");
            m.addItem (11, "Consolidate clips (loop range, or the whole track)");
        }
        if (ArrangeView::maxTakes (*track) > 1) m.addItem (12, owner.takesHeight (*track) > 0 ? "Hide take lanes" : "Show take lanes");
        m.addSeparator();
        const bool frozen = track->isFrozen (te::Track::individualFreeze);
        if (DawSession::kindOf (*track) != DawSession::TrackKind::bus)
            m.addItem (4, frozen ? "Unfreeze track" : "Freeze track (render plug-ins to save CPU)");
        m.addItem (5, (bool) track->state.getProperty ("vegaAutoShown", false) ? "Hide automation lane" : "Show automation lane");
        m.addSeparator();
        m.addItem (3, DawSession::isFolder (*track) ? "Delete folder and its tracks" : "Delete track");
        m.showMenuAsync ({}, [this, buses, folders] (int r)
        {
            if (track == nullptr) return;
            auto& ss = owner.vs.session;
            if (r == 300)
            {
                ss.edit().getUndoManager().beginNewTransaction ("New folder");
                auto* f = ss.addFolder();
                if (f != nullptr)
                {
                    ss.moveTrack (*f, ss.getTracks().indexOf (track.get()));
                    ss.moveToFolder (*track, f);
                }
                owner.rebuildHeaders(); return;
            }
            if (r > 300 && r <= 300 + folders.size()) { ss.moveToFolder (*track, folders[r - 301]); owner.rebuildHeaders(); return; }
            if (r == 299) { ss.moveToFolder (*track, nullptr); owner.rebuildHeaders(); return; }
            if (r == 7) { if (auto* d = ss.duplicateTrack (*track)) owner.vs.selectTrack (d); owner.rebuildHeaders(); return; }
            if (r == 8) { DawSession::setCollapsed (*track, ! DawSession::isCollapsed (*track)); owner.rebuildHeaders(); return; }
            if (r == 9)
            {
                auto t = track;
                for (auto* m2 : ss.folderMembers (*t)) ss.moveToFolder (*m2, nullptr);
                ss.deleteTrack (*t); return;
            }
            if ((r == 10 || r == 11) && owner.onRenderTrack) { owner.onRenderTrack (*track, r == 10); return; }
            if (r == 12) { track->state.setProperty ("vegaTakesShown", owner.takesHeight (*track) == 0, nullptr); owner.resized(); owner.repaint(); return; }
            if (r == 1) name.showEditor();
            else if (r >= 100 && r < 109) { static const juce::uint32 pal[] { 0xff48e1e8, 0xffe8b448, 0xff7ee87a, 0xffe8708a, 0xffa58bff, 0xff6fb7ff, 0xffffa25e, 0xff5ee8b6, 0xffcccccc }; track->state.setProperty ("vegaColour", juce::Colour (pal[r - 100]).toString(), nullptr); }
            else if (r == 200) owner.vs.session.routeToBus (*track, nullptr);
            else if (r > 200) owner.vs.session.routeToBus (*track, buses[r - 201]);
            else if (r == 3)
            {
                auto t = track;
                if (DawSession::isFolder (*t))
                    for (auto* m2 : owner.vs.session.folderMembers (*t)) owner.vs.session.deleteTrack (*m2);   // the folder and everything in it
                owner.vs.session.deleteTrack (*t);
            }
            else if (r == 4)
            {
                const bool now = ! track->isFrozen (te::Track::individualFreeze);
                auto res = track->setFrozen (now, te::Track::individualFreeze);
                if (res.failed()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Freeze", res.getErrorMessage());
                refresh();
            }
            else if (r == 5) { track->state.setProperty ("vegaAutoShown", ! (bool) track->state.getProperty ("vegaAutoShown", false), nullptr); owner.resized(); }
            owner.repaint();
        });
    }
    ArrangeView& owner;
    juce::Label name; juce::TextButton mute, solo, arm, input, instBtn, fxBtn, autoBtn, foldBtn, takesBtn; juce::Slider vol;
    juce::ComboBox laneParamBox; juce::Array<te::AutomatableParameter*> laneParams;
    void refreshLaneParams()
    {
        laneParams = dawui::automatableParams (*track);
        auto* cur = dawui::laneParam (*track);
        laneParamBox.clear (juce::dontSendNotification);
        for (int i = 0; i < laneParams.size(); ++i)
            laneParamBox.addItem (dawui::paramDisplayName (*laneParams[i]) + (laneParams[i]->hasAutomationPoints() ? "  *" : ""), i + 1);
        laneParamBox.setSelectedId (laneParams.indexOf (cur) + 1, juce::dontSendNotification);
    }
    int dragStartY = 0; bool resizing = false;
};

//==============================================================================
class ArrangeView::Lanes : public juce::Component
{
public:
    explicit Lanes (ArrangeView& o) : owner (o), vs (o.vs) { setWantsKeyboardFocus (false); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff181818));
        auto& ed = vs.edit();
        // rows
        int y = -vs.scrollY;
        for (auto* t : owner.visibleTracks())
        {
            const int h = owner.trackHeight (*t);
            if (y + h >= 0 && y < getHeight())
            {
                g.setColour (juce::Colour (vs.selectedTrack == t->itemID ? 0xff1f2323 : 0xff1b1b1b)); g.fillRect (0, y + 1, getWidth(), h - 2);
                g.setColour (juce::Colour (0xff111111)); g.drawHorizontalLine (y + h - 1, 0, (float) getWidth());
            }
            y += h;
        }
        // grid
        const double first = std::floor (vs.scrollBeats), last = vs.xToBeat ((float) getWidth(), 0);
        const int sub = vs.lineDiv();
        for (double b = first; b <= last; b += 1.0)
        {
            const bool bar = vs.isBarStart (b);
            if (! bar && vs.pixelsPerBeat < 12) continue;
            g.setColour (juce::Colours::white.withAlpha (bar ? 0.09f : 0.035f));
            g.drawVerticalLine ((int) vs.beatToX (b, 0), 0, (float) std::min (getHeight(), y));
            // the finer lines "Snap: Line" uses when zoomed in
            g.setColour (juce::Colours::white.withAlpha (0.018f));
            for (int k = 1; k < sub; ++k) g.drawVerticalLine ((int) vs.beatToX (b + (double) k / sub, 0), 0, (float) std::min (getHeight(), y));
        }
        // loop
        auto& tr = ed.getTransport();
        if (tr.looping)
        {
            const auto lr = tr.getLoopRange();
            const float x0 = vs.beatToX (vs.timeToBeats (lr.getStart()), 0), x1 = vs.beatToX (vs.timeToBeats (lr.getEnd()), 0);
            g.setColour (VegaColours::accent.withAlpha (0.05f)); g.fillRect (juce::Rectangle<float> (x0, 0, x1 - x0, (float) getHeight()));
        }
        // clips
        y = -vs.scrollY;
        for (auto* t : owner.visibleTracks())
        {
            const int h = owner.trackHeight (*t), ch = owner.clipHeight (*t), tk = owner.takesHeight (*t), lh = owner.laneHeight (*t);
            if (y + h >= 0 && y < getHeight())
            {
                if (DawSession::isFolder (*t)) drawFolderOverview (g, *t, y, ch);
                for (auto* c : t->getClips()) drawClip (g, *c, y, ch);
                if (tk > 0) drawTakes (g, *t, y + ch, tk);
                if (lh > 0) drawLane (g, *t, y + ch + tk, lh);
            }
            y += h;
        }
        // recording preview
        if (tr.isRecording())
        {
            const float x0 = vs.beatToX (vs.timeToBeats (tr.getTimeWhenStarted()), 0), x1 = vs.beatToX (vs.timeToBeats (tr.getPosition()), 0);
            int yy = -vs.scrollY;
            for (auto* t : owner.visibleTracks())
            {
                const int h = owner.trackHeight (*t);
                bool armed = false;
                for (auto* in : ed.getAllInputDevices()) if (te::isOnTargetTrack (*in, *t, 0) && in->isRecordingEnabled (t->itemID)) armed = true;
                if (armed) { g.setColour (juce::Colour (0x55e04848)); g.fillRect (juce::Rectangle<float> (x0, (float) yy + 4, x1 - x0, (float) h - 8)); }
                yy += h;
            }
        }
        // drop hint
        if (dropY >= 0) { g.setColour (VegaColours::accent); g.fillRect (0, dropY, getWidth(), 2); }
        // playhead
        const float px = vs.beatToX (vs.timeToBeats (tr.getPosition()), 0);
        g.setColour (VegaColours::gold); g.fillRect (px - 0.5f, 0.0f, 1.5f, (float) getHeight());
    }

    // ---------------------------------------------------------------- warp markers (elastic audio)
    /** (sourceTime, warpTime) pairs in seconds, sorted by warp time */
    static std::vector<std::pair<double, double>> warpMarkers (te::WaveAudioClip& ac)
    {
        std::vector<std::pair<double, double>> m;
        for (auto* wm : ac.getWarpTimeManager().getMarkers()) m.push_back ({ wm->sourceTime.inSeconds(), wm->warpTime.inSeconds() });
        std::sort (m.begin(), m.end(), [] (auto& a, auto& b) { return a.second < b.second; });
        return m;
    }
    float warpToX (te::WaveAudioClip& ac, double warpSecs) const
    {
        const auto pos = ac.getPosition();
        return vs.beatToX (vs.timeToBeats (pos.getStart() + te::TimeDuration::fromSeconds (warpSecs - pos.getOffset().inSeconds())), 0);
    }
    double xToWarp (te::WaveAudioClip& ac, float x) const
    {
        const auto pos = ac.getPosition();
        return (vs.beatsToTime (vs.xToBeat (x, 0)) - pos.getStart()).inSeconds() + pos.getOffset().inSeconds();
    }
    static void enableWarpMarkers (te::WaveAudioClip& ac, bool on) { DawSession::setElastic (ac, on); }
    int warpMarkerAt (te::WaveAudioClip& ac, float x) const
    {
        int best = -1; float bestD = 6.0f;
        auto& list = ac.getWarpTimeManager().getMarkers();
        for (int i = 0; i < list.size(); ++i)
        {
            const float d = std::abs (warpToX (ac, list[i]->warpTime.inSeconds()) - x);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }
    te::EditItemID warpDragClip; int warpDragIndex = -1;

    void drawClip (juce::Graphics& g, te::Clip& c, int rowY, int rowH)
    {
        const auto pos = c.getPosition();
        const float x0 = vs.beatToX (vs.timeToBeats (pos.getStart()), 0), x1 = vs.beatToX (vs.timeToBeats (pos.getEnd()), 0);
        if (x1 < 0 || x0 > getWidth()) return;
        auto r = juce::Rectangle<float> (x0, (float) rowY + 3, std::max (2.0f, x1 - x0), (float) rowH - 6);
        const auto col = dawui::clipColour (c);
        const bool sel = vs.selectedClip == c.itemID;
        g.setColour (col.withAlpha (0.22f).withMultipliedBrightness (c.isMuted() ? 0.4f : 1.0f)); g.fillRoundedRectangle (r, 4.0f);
        auto title = r.removeFromTop (15.0f);
        g.setColour (col.withAlpha (0.55f)); g.fillRoundedRectangle (title.withHeight (15.0f), 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f)); g.setFont (VegaFonts::ui (10.5f, true));
        g.drawText (c.getName(), title.reduced (5, 0).toNearestInt(), juce::Justification::centredLeft, true);
        g.saveState(); g.reduceClipRegion (r.toNearestInt());
        if (auto* ac = dynamic_cast<te::WaveAudioClip*> (&c))
        {
            auto& th = thumbFor (*ac);
            const auto srcStart = ac->getPosition().getOffset().inSeconds() * ac->getSpeedRatio();
            const auto srcLen = pos.getLength().inSeconds() * ac->getSpeedRatio();
            g.setColour (col);
            if (ac->getWarpTime())
            {   // elastic audio: draw each segment between warp markers stretched to its warped length
                auto markers = warpMarkers (*ac);
                for (size_t i = 0; i + 1 < markers.size(); ++i)
                {
                    const float wx0 = warpToX (*ac, markers[i].second), wx1 = warpToX (*ac, markers[i + 1].second);
                    if (wx1 - wx0 < 1.0f) continue;
                    th.drawChannels (g, juce::Rectangle<float> (wx0, r.getY(), wx1 - wx0, r.getHeight()).toNearestInt(),
                                     { te::TimePosition::fromSeconds (markers[i].first), te::TimePosition::fromSeconds (markers[i + 1].first) }, juce::Decibels::decibelsToGain (ac->getGainDB()));
                }
                for (size_t i = 0; i < markers.size(); ++i)
                {
                    const float mx = warpToX (*ac, markers[i].second);
                    const bool hot = warpDragClip == ac->itemID && warpDragIndex == (int) i;
                    g.setColour (hot ? juce::Colours::white : VegaColours::gold);
                    g.drawVerticalLine ((int) mx, r.getY(), r.getBottom());
                    juce::Path tri; tri.addTriangle (mx - 5, r.getY(), mx + 5, r.getY(), mx, r.getY() + 7);
                    g.fillPath (tri);
                }
                g.setColour (col);
            }
            else
                th.drawChannels (g, r.toNearestInt(), { te::TimePosition::fromSeconds (srcStart), te::TimePosition::fromSeconds (srcStart + srcLen) }, juce::Decibels::decibelsToGain (ac->getGainDB()));
            // fades
            const float fi = (float) (ac->getFadeIn().inSeconds() / pos.getLength().inSeconds()) * r.getWidth();
            const float fo = (float) (ac->getFadeOut().inSeconds() / pos.getLength().inSeconds()) * r.getWidth();
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            if (fi > 1) g.drawLine (r.getX(), r.getBottom(), r.getX() + fi, r.getY(), 1.2f);
            if (fo > 1) g.drawLine (r.getRight() - fo, r.getY(), r.getRight(), r.getBottom(), 1.2f);
            if (const int nt = DawSession::takeFiles (*ac).size(); nt > 1)
            {
                g.setFont (VegaFonts::ui (9.5f, true)); g.setColour (juce::Colour (0xffe8a23a));
                g.drawText ("TAKE " + juce::String (DawSession::currentTake (*ac) + 1) + "/" + juce::String (nt), r.reduced (4, 2).toNearestInt(), juce::Justification::bottomRight);
            }
            if (ac->getAutoTempo() || ac->getWarpTime() || std::abs (ac->getPitchChange()) > 0.01f)
            {
                g.setFont (VegaFonts::ui (9.5f, true)); g.setColour (VegaColours::gold);
                juce::String tag; if (ac->getAutoTempo()) tag << "WARP "; if (ac->getWarpTime()) tag << "ELASTIC "; if (std::abs (ac->getPitchChange()) > 0.01f) tag << juce::String (ac->getPitchChange(), 1) << " st";
                g.drawText (tag, r.reduced (4, 2).toNearestInt(), juce::Justification::topRight);
            }
        }
        else if (auto* mc = dynamic_cast<te::MidiClip*> (&c))
        {
            auto& seq = mc->getSequence();
            int lo = 127, hi = 0;
            for (auto* n : seq.getNotes()) { lo = std::min (lo, n->getNoteNumber()); hi = std::max (hi, n->getNoteNumber()); }
            if (hi >= lo)
            {
                lo -= 2; hi += 2;
                const float nh = std::max (1.5f, std::min (6.0f, r.getHeight() / (float) (hi - lo + 1)));
                g.setColour (col);
                const double clipStartBeat = vs.timeToBeats (pos.getStart()) - mc->getOffsetInBeats().inBeats();
                for (auto* n : seq.getNotes())
                {
                    const float nx0 = vs.beatToX (clipStartBeat + n->getStartBeat().inBeats(), 0), nx1 = vs.beatToX (clipStartBeat + n->getEndBeat().inBeats(), 0);
                    const float ny = r.getBottom() - (n->getNoteNumber() - lo + 1) * (r.getHeight() / (float) (hi - lo + 1));
                    g.fillRect (nx0, ny, std::max (1.5f, nx1 - nx0 - 1), nh);
                }
            }
        }
        else if (auto* sc = dynamic_cast<te::StepClip*> (&c))
        {   // step clip: the pattern, repeated across the clip
            auto pattern = sc->getPattern (0);
            const int rows = sc->getChannels().size(), steps = pattern.getNumNotes();
            const double stepBeats = pattern.getNoteLength().inBeats();
            if (rows > 0 && steps > 0 && stepBeats > 0)
            {
                const float rh = r.getHeight() / (float) rows;
                const double startBeat = vs.timeToBeats (pos.getStart()) - sc->getOffsetInBeats().inBeats(), endBeat = vs.timeToBeats (pos.getEnd());
                g.setColour (col);
                for (int i = 0; startBeat + i * stepBeats < endBeat && i < 4096; ++i)
                    for (int ch = 0; ch < rows; ++ch)
                        if (pattern.getNote (ch, i % steps))
                        {
                            const float x = vs.beatToX (startBeat + i * stepBeats, 0);
                            g.fillRect (x, r.getY() + ch * rh, std::max (1.5f, (float) (stepBeats * vs.pixelsPerBeat) - 1.0f), std::max (1.0f, rh - 1.0f));
                        }
            }
        }
        g.restoreState();
        g.setColour (sel ? juce::Colours::white : col.withAlpha (0.8f));
        g.drawRoundedRectangle (juce::Rectangle<float> (x0, (float) rowY + 3, std::max (2.0f, x1 - x0), (float) rowH - 6), 4.0f, sel ? 2.0f : 1.0f);
    }

    te::SmartThumbnail& thumbFor (te::WaveAudioClip& c)
    {
        const auto key = c.itemID.toString() + c.getOriginalFile().getFullPathName();
        auto it = thumbs.find (key);
        if (it == thumbs.end())
            it = thumbs.emplace (key, std::make_unique<te::SmartThumbnail> (vs.edit().engine, te::AudioFile (vs.edit().engine, c.getOriginalFile()), *this, &vs.edit())).first;
        return *it->second;
    }

    // ---------------------------------------------------------------- folders: a summary of what's inside
    void drawFolderOverview (juce::Graphics& g, te::AudioTrack& folder, int rowY, int rowH)
    {
        auto members = vs.session.folderMembers (folder);
        const auto col = DawSession::trackColour (folder);
        g.setColour (col.withAlpha (0.05f)); g.fillRect (0, rowY + 1, getWidth(), rowH - 2);
        if (members.isEmpty())
        {
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f));
            g.drawText ("Empty folder - right-click a track header > Folder to put tracks in it", 10, rowY + 4, getWidth() - 20, 16, juce::Justification::centredLeft);
            return;
        }
        const float lineH = juce::jlimit (2.0f, 8.0f, (rowH - 12.0f) / (float) members.size());
        for (int i = 0; i < members.size(); ++i)
            for (auto* c : members[i]->getClips())
            {
                const auto pos = c->getPosition();
                const float x0 = vs.beatToX (vs.timeToBeats (pos.getStart()), 0), x1 = vs.beatToX (vs.timeToBeats (pos.getEnd()), 0);
                if (x1 < 0 || x0 > getWidth()) continue;
                g.setColour (DawSession::trackColour (*members[i]).withAlpha (c->isMuted() ? 0.25f : 0.8f));
                g.fillRoundedRectangle (x0, rowY + 6 + i * lineH, std::max (2.0f, x1 - x0), std::max (1.5f, lineH - 1.0f), 1.0f);
            }
    }

    // ---------------------------------------------------------------- take lanes (comping)
    void drawTakes (juce::Graphics& g, te::AudioTrack& t, int top, int h)
    {
        const auto col = DawSession::trackColour (t);
        const int lanes = h / ArrangeView::takeLaneH;
        for (int i = 0; i < lanes; ++i)
        {
            const int y = top + i * ArrangeView::takeLaneH;
            g.setColour (juce::Colour (i % 2 ? 0xff151515 : 0xff181818)); g.fillRect (0, y, getWidth(), ArrangeView::takeLaneH - 1);
        }
        for (auto* c : t.getClips())
        {
            auto* ac = dynamic_cast<te::WaveAudioClip*> (c);
            if (ac == nullptr) continue;
            auto files = DawSession::takeFiles (*ac);
            if (files.size() < 2) continue;
            const auto pos = ac->getPosition();
            const float x0 = vs.beatToX (vs.timeToBeats (pos.getStart()), 0), x1 = vs.beatToX (vs.timeToBeats (pos.getEnd()), 0);
            if (x1 < 0 || x0 > getWidth()) continue;
            const int cur = DawSession::currentTake (*ac);
            const double srcStart = pos.getOffset().inSeconds() * ac->getSpeedRatio(), srcLen = pos.getLength().inSeconds() * ac->getSpeedRatio();
            for (int i = 0; i < std::min (lanes, files.size()); ++i)
            {
                auto r = juce::Rectangle<float> (x0, (float) (top + i * ArrangeView::takeLaneH) + 2, std::max (2.0f, x1 - x0), (float) ArrangeView::takeLaneH - 5);
                const bool on = i == cur;
                g.setColour (on ? col.withAlpha (0.35f) : juce::Colour (0x22ffffff)); g.fillRoundedRectangle (r, 3.0f);
                g.setColour (on ? col : juce::Colour (0xff7a7a7a));
                fileThumb (files[i]).drawChannels (g, r.reduced (0, 2).toNearestInt(), { te::TimePosition::fromSeconds (srcStart), te::TimePosition::fromSeconds (srcStart + srcLen) }, 1.0f);
                if (on) { g.setColour (col.brighter (0.4f)); g.drawRoundedRectangle (r, 3.0f, 1.5f); }
            }
        }
        if (takeTrack == &t && takeDragging)
        {   // the range being comped
            const float a = vs.beatToX (std::min (takeFrom, takeTo), 0), b = vs.beatToX (std::max (takeFrom, takeTo), 0);
            const int y = top + takeIndex * ArrangeView::takeLaneH;
            g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRect (juce::Rectangle<float> (a, (float) y, b - a, (float) ArrangeView::takeLaneH - 1));
            g.setColour (juce::Colours::white); g.drawRect (juce::Rectangle<float> (a, (float) y, b - a, (float) ArrangeView::takeLaneH - 1), 1.0f);
        }
    }
    te::SmartThumbnail& fileThumb (const juce::File& f)
    {
        const auto key = "take:" + f.getFullPathName();
        auto it = thumbs.find (key);
        if (it == thumbs.end())
            it = thumbs.emplace (key, std::make_unique<te::SmartThumbnail> (vs.edit().engine, te::AudioFile (vs.edit().engine, f), *this, &vs.edit())).first;
        return *it->second;
    }
    /** Which take lane (if any) is under the point */
    bool takeHit (juce::Point<float> p, te::AudioTrack*& track, int& index)
    {
        int rowTop = 0;
        auto* t = owner.trackAtY ((int) p.y, &rowTop);
        if (t == nullptr) return false;
        const int ch = owner.clipHeight (*t), tk = owner.takesHeight (*t);
        if (tk == 0 || p.y < rowTop + ch || p.y >= rowTop + ch + tk) return false;
        track = t; index = (int) ((p.y - rowTop - ch) / ArrangeView::takeLaneH);
        return true;
    }
    bool takeMouseDown (const juce::MouseEvent& e)
    {
        te::AudioTrack* t = nullptr; int idx = 0;
        if (! takeHit (e.position, t, idx)) return false;
        vs.selectTrack (t);
        takeTrack = t; takeIndex = idx; takeDragging = false;
        takeFrom = takeTo = vs.snapBeat (vs.xToBeat (e.position.x, 0), e.mods.isShiftDown());
        takeClickBeat = vs.xToBeat (e.position.x, 0);
        return true;
    }
    void takeMouseDrag (const juce::MouseEvent& e)
    {
        if (e.getDistanceFromDragStart() > 3) takeDragging = true;
        takeTo = vs.snapBeat (vs.xToBeat (e.position.x, 0), e.mods.isShiftDown());
        repaint();
    }
    void takeMouseUp()
    {
        auto* t = takeTrack; takeTrack = nullptr;
        if (t == nullptr) return;
        // a click picks the take for the whole clip under the mouse; a drag comps just that range
        te::TimeRange range;
        if (takeDragging && std::abs (takeTo - takeFrom) > 1.0e-3) range = { vs.beatsToTime (std::min (takeFrom, takeTo)), vs.beatsToTime (std::max (takeFrom, takeTo)) };
        else { const auto at = vs.beatsToTime (takeClickBeat); range = { at, at }; }
        juce::Array<te::WaveAudioClip*> targets;
        for (auto* c : t->getClips())
            if (auto* ac = dynamic_cast<te::WaveAudioClip*> (c); ac != nullptr && DawSession::takeFiles (*ac).size() > takeIndex)
            {
                const auto cr = ac->getPosition().time;
                if (range.getLength() > 0s ? cr.overlaps (range) : (cr.contains (range.getStart())))
                    targets.add (ac);
            }
        for (auto* ac : targets)
        {
            if (range.getLength() > 0s) vs.session.compTake (*ac, takeIndex, range);
            else { vs.edit().getUndoManager().beginNewTransaction ("Choose take"); DawSession::setTake (*ac, takeIndex); }
        }
        takeDragging = false;
        owner.repaint();
    }
    te::AudioTrack* takeTrack = nullptr; int takeIndex = 0; bool takeDragging = false; double takeFrom = 0, takeTo = 0, takeClickBeat = 0;

    // ---------------------------------------------------------------- automation lane
    float laneValueToY (te::AutomatableParameter& p, float v, int top, int h) const
    {
        const auto r = p.getValueRange();
        const float n = r.getLength() > 0 ? (v - r.getStart()) / r.getLength() : 0.5f;
        return (float) top + 5.0f + (1.0f - juce::jlimit (0.0f, 1.0f, n)) * (float) (h - 10);
    }
    float laneYToValue (te::AutomatableParameter& p, float y, int top, int h) const
    {
        const auto r = p.getValueRange();
        const float n = 1.0f - juce::jlimit (0.0f, 1.0f, (y - (float) top - 5.0f) / (float) (h - 10));
        return r.getStart() + n * r.getLength();
    }
    void drawLane (juce::Graphics& g, te::AudioTrack& t, int top, int h)
    {
        g.setColour (juce::Colour (0xff131515)); g.fillRect (0, top, getWidth(), h - 1);
        g.setColour (juce::Colour (0xff262a2a)); g.drawHorizontalLine (top, 0, (float) getWidth());
        auto* p = dawui::laneParam (t);
        if (p == nullptr) return;
        auto& curve = p->getCurve();
        const auto col = DawSession::trackColour (t);
        const int n = curve.getNumPoints();
        juce::Path path;
        for (int x = 0; x <= getWidth(); x += 3)
        {
            const auto tm = vs.beatsToTime (vs.xToBeat ((float) x, 0));
            const float v = n > 0 ? curve.getValueAt (tm, p->getCurrentValue()) : p->getCurrentValue();
            const float y = laneValueToY (*p, v, top, h);
            if (x == 0) path.startNewSubPath ((float) x, y); else path.lineTo ((float) x, y);
        }
        if (n == 0)
        {
            juce::Path dashed; const float dl[] { 4.0f, 4.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, path, dl, 2);
            g.setColour (col.withAlpha (0.45f)); g.fillPath (dashed);
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f));
            g.drawText ("Click to add automation points for " + dawui::paramDisplayName (*p) + "  (enable WRITE in the transport to record moves)", 10, top + 4, getWidth() - 20, 14, juce::Justification::centredLeft);
            return;
        }
        auto fill = path; fill.lineTo ((float) getWidth(), (float) top + h - 1); fill.lineTo (0, (float) top + h - 1); fill.closeSubPath();
        g.setColour (col.withAlpha (0.10f)); g.fillPath (fill);
        g.setColour (col.withAlpha (0.9f)); g.strokePath (path, juce::PathStrokeType (1.6f));
        for (int i = 0; i < n; ++i)
        {
            const float x = vs.beatToX (vs.timeToBeats (curve.getPointTime (i)), 0), y = laneValueToY (*p, curve.getPointValue (i), top, h);
            if (x < -6 || x > getWidth() + 6) continue;
            const bool hot = autoTrack == &t && autoPoint == i;
            g.setColour (hot ? juce::Colours::white : col); g.fillEllipse (x - 4, y - 4, 8, 8);
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (x - 4, y - 4, 8, 8, 1.0f);
        }
        if (autoTrack == &t && autoPoint >= 0 && autoPoint < n)
        {
            g.setColour (juce::Colours::white); g.setFont (VegaFonts::ui (10.0f, true));
            const float x = vs.beatToX (vs.timeToBeats (curve.getPointTime (autoPoint)), 0);
            g.drawText (p->valueToString (curve.getPointValue (autoPoint)), (int) x + 8, top + 3, 90, 13, juce::Justification::centredLeft);
        }
    }
    struct LaneHit { te::AudioTrack* track = nullptr; te::AutomatableParameter* param = nullptr; int top = 0, h = 0, point = -1; };
    LaneHit laneHit (juce::Point<float> p)
    {
        int rowTop = 0;
        auto* t = owner.trackAtY ((int) p.y, &rowTop);
        if (t == nullptr) return {};
        const int ch = owner.clipHeight (*t) + owner.takesHeight (*t), lh = owner.laneHeight (*t);
        if (lh == 0 || p.y < rowTop + ch) return {};
        LaneHit r { t, dawui::laneParam (*t), rowTop + ch, lh, -1 };
        if (r.param == nullptr) return {};
        auto& curve = r.param->getCurve();
        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            const float x = vs.beatToX (vs.timeToBeats (curve.getPointTime (i)), 0), y = laneValueToY (*r.param, curve.getPointValue (i), r.top, r.h);
            if (std::abs (x - p.x) <= 6 && std::abs (y - p.y) <= 6) { r.point = i; break; }
        }
        return r;
    }
    bool laneMouseDown (const juce::MouseEvent& e)
    {
        auto h = laneHit (e.position);
        if (h.track == nullptr) return false;
        vs.selectTrack (h.track);
        auto& curve = h.param->getCurve();
        auto* um = &vs.edit().getUndoManager();
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            if (h.point >= 0) m.addItem (1, "Delete point");
            m.addItem (2, "Clear all automation for " + dawui::paramDisplayName (*h.param), curve.getNumPoints() > 0);
            m.addItem (3, "Hide lane");
            te::AudioTrack::Ptr tp (h.track);
            m.showMenuAsync ({}, [this, tp, pt = h.point, param = te::AutomatableParameter::Ptr (h.param)] (int r)
            {
                auto* um2 = &vs.edit().getUndoManager();
                if (r == 1 && pt >= 0) param->getCurve().removePoint (pt, um2);
                else if (r == 2) param->getCurve().clear (um2);
                else if (r == 3) { tp->state.setProperty ("vegaAutoShown", false, nullptr); owner.resized(); }
                owner.repaint();
            });
            return true;
        }
        um->beginNewTransaction ("Automation");
        autoTrack = h.track; autoParam = h.param;
        if (h.point >= 0) autoPoint = h.point;
        else
        {
            const auto tm = vs.beatsToTime (std::max (0.0, vs.snapBeat (vs.xToBeat (e.position.x, 0), e.mods.isShiftDown())));
            autoPoint = curve.addPoint (tm, laneYToValue (*h.param, e.position.y, h.top, h.h), 0.0f, um);
        }
        autoTop = h.top; autoH = h.h;
        repaint();
        return true;
    }
    void laneMouseDrag (const juce::MouseEvent& e)
    {
        if (autoParam == nullptr || autoPoint < 0) return;
        auto& curve = autoParam->getCurve();
        if (autoPoint >= curve.getNumPoints()) return;
        const auto tm = vs.beatsToTime (std::max (0.0, vs.snapBeat (vs.xToBeat (e.position.x, 0), e.mods.isShiftDown())));
        autoPoint = curve.movePoint (*autoParam, autoPoint, tm, laneYToValue (*autoParam, e.position.y, autoTop, autoH), false, &vs.edit().getUndoManager());
        repaint();
    }
    te::AudioTrack* autoTrack = nullptr; te::AutomatableParameter::Ptr autoParam; int autoPoint = -1, autoTop = 0, autoH = 0;
    void forgetEdit() { autoTrack = nullptr; autoParam = nullptr; autoPoint = -1; warpDragIndex = -1; drag = Drag::none; takeTrack = nullptr; thumbs.clear(); }

    // ---------------------------------------------------------------- mouse
    enum class Drag { none, move, trimStart, trimEnd, fadeIn, fadeOut };
    struct Hit { te::Clip* clip = nullptr; Drag zone = Drag::none; };
    Hit hitTest (juce::Point<float> p)
    {
        int rowTop = 0;
        auto* t = owner.trackAtY ((int) p.y, &rowTop);
        if (t == nullptr) return {};
        const int h = owner.trackHeight (*t);
        if (p.y >= rowTop + owner.clipHeight (*t)) return {};     // automation lane
        for (auto* c : t->getClips())
        {
            const auto pos = c->getPosition();
            const float x0 = vs.beatToX (vs.timeToBeats (pos.getStart()), 0), x1 = vs.beatToX (vs.timeToBeats (pos.getEnd()), 0);
            if (p.x < x0 || p.x > x1) continue;
            const bool audio = dynamic_cast<te::AudioClipBase*> (c) != nullptr;
            const float top = (float) rowTop + 3;
            if (audio && p.y < top + 15 && p.x < x0 + 10) return { c, Drag::fadeIn };
            if (audio && p.y < top + 15 && p.x > x1 - 10) return { c, Drag::fadeOut };
            if (p.x < x0 + 6) return { c, Drag::trimStart };
            if (p.x > x1 - 6) return { c, Drag::trimEnd };
            juce::ignoreUnused (h);
            return { c, Drag::move };
        }
        return {};
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto h = hitTest (e.position);
        setMouseCursor (h.zone == Drag::trimStart || h.zone == Drag::trimEnd ? juce::MouseCursor::LeftRightResizeCursor
                        : h.zone == Drag::fadeIn || h.zone == Drag::fadeOut ? juce::MouseCursor::CrosshairCursor
                        : h.zone == Drag::move ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.grabKeyboardFocus();
        autoParam = nullptr; autoPoint = -1; warpDragIndex = -1; takeTrack = nullptr;
        if (! e.mods.isPopupMenu() && takeMouseDown (e)) return;
        if (laneMouseDown (e)) return;
        const auto h = hitTest (e.position);
        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (h.clip); wc != nullptr && wc->getWarpTime() && ! e.mods.isPopupMenu() && h.zone == Drag::move)
        {
            vs.selectClip (wc);
            int idx = warpMarkerAt (*wc, e.position.x);
            if (idx < 0 && e.mods.isCommandDown())
            {   // Ctrl / Cmd-click: new warp marker here
                auto& wtm = wc->getWarpTimeManager();
                const auto wt = te::TimePosition::fromSeconds (xToWarp (*wc, e.position.x));
                vs.edit().getUndoManager().beginNewTransaction ("Add warp marker");
                idx = wtm.insertMarker (te::WarpMarker (wtm.warpTimeToSourceTime (wt), wt));
            }
            if (idx >= 0) { warpDragClip = wc->itemID; warpDragIndex = idx; drag = Drag::none; repaint(); return; }
        }
        drag = h.zone; dragged = false;
        if (h.clip != nullptr)
        {
            vs.selectClip (h.clip);
            const auto pos = h.clip->getPosition();
            origStart = pos.getStart(); origEnd = pos.getEnd(); origOffset = pos.getOffset();
            grabBeat = vs.xToBeat (e.position.x, 0);
            if (auto* ac = dynamic_cast<te::AudioClipBase*> (h.clip)) { origFadeIn = ac->getFadeIn(); origFadeOut = ac->getFadeOut(); }
            if (e.mods.isPopupMenu()) { clipMenu (*h.clip, e); drag = Drag::none; }
            else if (e.mods.isAltDown() && drag == Drag::move)
            {   // alt-drag duplicates
                owner.duplicateSelectedClip();
            }
        }
        else
        {
            vs.selectClip (nullptr);
            if (auto* t = owner.trackAtY ((int) e.position.y)) vs.selectTrack (t);
            if (! e.mods.isPopupMenu())
                vs.edit().getTransport().setPosition (vs.beatsToTime (std::max (0.0, vs.snapBeat (vs.xToBeat (e.position.x, 0), e.mods.isShiftDown()))));
            else if (auto* t = owner.trackAtY ((int) e.position.y); t != nullptr && DawSession::kindOf (*t) == DawSession::TrackKind::midi)
                emptyMenu (*t, e.position.x);
        }
        owner.repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (takeTrack != nullptr) { takeMouseDrag (e); return; }
        if (autoParam != nullptr) { laneMouseDrag (e); return; }
        if (warpDragIndex >= 0)
        {
            if (auto* wc = dynamic_cast<te::WaveAudioClip*> (vs.getSelectedClip()); wc != nullptr && wc->itemID == warpDragClip)
            {
                auto& wtm = wc->getWarpTimeManager();
                auto& list = wtm.getMarkers();
                if (warpDragIndex < list.size())
                {
                    // keep it between its neighbours
                    double lo = -1.0e9, hi = 1.0e9; const double cur = list[warpDragIndex]->warpTime.inSeconds();
                    for (auto* m : list) { const double w = m->warpTime.inSeconds(); if (w < cur - 1.0e-6) lo = std::max (lo, w); if (w > cur + 1.0e-6) hi = std::min (hi, w); }
                    double nw = xToWarp (*wc, e.position.x);
                    if (! e.mods.isShiftDown()) nw = (vs.beatsToTime (vs.snapBeat (vs.xToBeat (e.position.x, 0))) - wc->getPosition().getStart()).inSeconds() + wc->getPosition().getOffset().inSeconds();
                    nw = juce::jlimit (lo + 0.01, hi - 0.01, nw);
                    wtm.moveMarker (warpDragIndex, te::TimePosition::fromSeconds (nw));
                    repaint();
                }
            }
            return;
        }
        auto* c = vs.getSelectedClip();
        if (c == nullptr || drag == Drag::none) return;
        if (! dragged && e.getDistanceFromDragStart() < 3) return;
        if (! dragged) { vs.edit().getUndoManager().beginNewTransaction ("Edit clip"); dragged = true; }
        const bool free = e.mods.isShiftDown();
        const double beat = vs.xToBeat (e.position.x, 0);
        const double startB = vs.timeToBeats (origStart), endB = vs.timeToBeats (origEnd);
        switch (drag)
        {
            case Drag::move:
            {
                double newStart = vs.snapBeat (startB + (beat - grabBeat), free);
                newStart = std::max (0.0, newStart);
                c->setStart (vs.beatsToTime (newStart), false, true);
                // move between tracks of the same kind
                if (auto* target = owner.trackAtY ((int) e.position.y))
                    if (target != c->getTrack() && (DawSession::kindOf (*target) == DawSession::TrackKind::midi) == (dynamic_cast<te::MidiClip*> (c) != nullptr))
                        c->moveTo (*target);
                break;
            }
            case Drag::trimStart:
            {
                double ns = juce::jlimit (0.0, endB - 1.0 / 16, vs.snapBeat (beat, free));
                const auto newStart = vs.beatsToTime (ns);
                const auto delta = newStart - origStart;
                if (origOffset + delta < 0s) break;              // can't reveal before the source start
                c->setStart (newStart, false, false);
                c->setOffset (origOffset + delta);
                c->setLength (origEnd - newStart, false);
                break;
            }
            case Drag::trimEnd:
            {
                const double ne = std::max (startB + 1.0 / 16, vs.snapBeat (beat, free));
                auto newLen = vs.beatsToTime (ne) - origStart;
                if (auto* ac = dynamic_cast<te::WaveAudioClip*> (c); ac != nullptr && ! ac->getAutoTempo() && ! ac->isLooping())
                {   // audio can't extend past its source
                    const double maxLen = ac->getSourceLength().inSeconds() / ac->getSpeedRatio() - origOffset.inSeconds();
                    newLen = te::TimeDuration::fromSeconds (std::min (newLen.inSeconds(), maxLen));
                }
                c->setLength (newLen, false);
                break;
            }
            case Drag::fadeIn: case Drag::fadeOut:
            {
                if (auto* ac = dynamic_cast<te::AudioClipBase*> (c))
                {
                    const double len = (origEnd - origStart).inSeconds();
                    const double t = (vs.beatsToTime (std::max (0.0, beat)) - origStart).inSeconds();
                    if (drag == Drag::fadeIn) ac->setFadeIn (te::TimeDuration::fromSeconds (juce::jlimit (0.0, len, t)));
                    else ac->setFadeOut (te::TimeDuration::fromSeconds (juce::jlimit (0.0, len, len - t)));
                }
                break;
            }
            default: break;
        }
        owner.repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (takeTrack != nullptr) takeMouseUp(); drag = Drag::none; autoParam = nullptr; warpDragIndex = -1; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        { te::AudioTrack* tt = nullptr; int ti = 0; if (takeHit (e.position, tt, ti)) return; }
        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (hitTest (e.position).clip); wc != nullptr && wc->getWarpTime())
        {   // double-click a warp marker (not the first / last) to remove it
            const int idx = warpMarkerAt (*wc, e.position.x);
            auto& list = wc->getWarpTimeManager().getMarkers();
            if (idx > 0 && idx < list.size() - 1) { wc->getWarpTimeManager().removeMarker (idx); warpDragIndex = -1; repaint(); return; }
        }
        if (auto lh = laneHit (e.position); lh.track != nullptr)
        {   // double-click a point to delete it
            if (lh.point >= 0) lh.param->getCurve().removePoint (lh.point, &vs.edit().getUndoManager());
            autoParam = nullptr; autoPoint = -1; repaint();
            return;
        }
        const auto h = hitTest (e.position);
        if (h.clip != nullptr)
        {
            if ((dynamic_cast<te::MidiClip*> (h.clip) != nullptr || dynamic_cast<te::StepClip*> (h.clip) != nullptr) && vs.onOpenPianoRoll) { vs.selectClip (h.clip); vs.onOpenPianoRoll(); }
            return;
        }
        if (auto* t = owner.trackAtY ((int) e.position.y))
            if (DawSession::kindOf (*t) == DawSession::TrackKind::midi)
            {   // new one-bar MIDI clip at the bar under the mouse
                const int num = vs.edit().tempoSequence.getTimeSig (0)->numerator;
                const double bar = std::floor (vs.xToBeat (e.position.x, 0) / num) * num;
                auto clip = vs.session.insertMidiClip (*t, vs.beatsToTime (std::max (0.0, bar)), vs.beatsToTime (bar + num) - vs.beatsToTime (bar));
                if (clip) { vs.selectClip (clip.get()); if (vs.onOpenPianoRoll) vs.onOpenPianoRoll(); }
            }
    }
    /** Right-click on an empty part of an instrument track */
    void emptyMenu (te::AudioTrack& t, float x)
    {
        juce::PopupMenu m;
        m.addItem (1, "New MIDI clip here (1 bar)");
        m.addItem (2, "New step-sequencer clip here (drums, 1 bar)");
        m.showMenuAsync ({}, [this, tp = te::AudioTrack::Ptr (&t), x] (int r)
        {
            if (r == 0 || tp == nullptr) return;
            const int num = vs.beatsPerBarAt (vs.xToBeat (x, 0));
            const double bar = std::floor (vs.xToBeat (x, 0) / num) * num;
            const te::TimeRange range { vs.beatsToTime (std::max (0.0, bar)), vs.beatsToTime (std::max (0.0, bar) + num) };
            vs.edit().getUndoManager().beginNewTransaction ("New clip");
            te::Clip* c = r == 1 ? (te::Clip*) vs.session.insertMidiClip (*tp, range.getStart(), range.getLength()).get()
                                 : tp->insertNewClip (te::TrackItem::Type::step, "Steps", range, nullptr);
            if (c != nullptr) { vs.selectClip (c); if (vs.onOpenPianoRoll) vs.onOpenPianoRoll(); }
        });
    }
    void clipMenu (te::Clip& c, const juce::MouseEvent&)
    {
        juce::PopupMenu m;
        m.addItem (1, "Split at playhead      Ctrl+E");
        m.addItem (2, "Duplicate                    Ctrl+D");
        m.addItem (3, "Delete                         Del");
        m.addItem (4, c.isMuted() ? "Unmute" : "Mute");
        m.addItem (5, "Rename");
        auto* ac = dynamic_cast<te::WaveAudioClip*> (&c);
        if (ac != nullptr)
        {
            m.addSeparator();
            m.addItem (10, "Warp to project tempo", true, ac->getAutoTempo());
            juce::PopupMenu pitch; for (int st = -12; st <= 12; ++st) pitch.addItem (100 + st + 12, (st > 0 ? "+" : "") + juce::String (st) + " semitones", true, juce::roundToInt (ac->getPitchChange()) == st);
            m.addSubMenu ("Pitch", pitch);
            m.addItem (11, "Reverse", true, ac->getIsReversed());
            m.addItem (12, "Loop clip", true, ac->isLooping());
            juce::PopupMenu gain; for (int db : { -12, -6, -3, 0, 3, 6, 12 }) gain.addItem (200 + db + 12, juce::String (db) + " dB", true, juce::roundToInt (ac->getGainDB()) == db);
            m.addSubMenu ("Clip gain", gain);
            m.addItem (13, "Slice at transients (new clips)");
            m.addSeparator();
            m.addItem (15, "Elastic audio (warp markers)", true, ac->getWarpTime());
            m.addItem (14, "Separate stems (AI)...");
            if (auto files = DawSession::takeFiles (*ac); files.size() > 1)
            {
                juce::PopupMenu tk;
                for (int i = 0; i < files.size(); ++i) tk.addItem (400 + i, "Take " + juce::String (i + 1), true, DawSession::currentTake (*ac) == i);
                tk.addSeparator();
                tk.addItem (30, "Show take lanes (drag across a take to comp)");
                tk.addItem (31, "Keep the current take only");
                tk.addItem (32, "Put every take on its own track");
                m.addSubMenu ("Takes (" + juce::String (files.size()) + ")", tk);
            }
        }
        else if (auto* mc = dynamic_cast<te::MidiClip*> (&c))
        {
            m.addSeparator();
            m.addItem (20, "Open in piano roll");
            m.addItem (21, "Quantise notes to grid");
            juce::ignoreUnused (mc);
        }
        m.addSeparator();
        m.addItem (40, "Copy to the clip launcher (next free slot)");
        m.showMenuAsync ({}, [this, id = c.itemID] (int r)
        {
            auto* c = vs.getSelectedClip(); if (c == nullptr || c->itemID != id) return;
            auto* ac = dynamic_cast<te::WaveAudioClip*> (c);
            if (r == 1) owner.splitSelectedAtPlayhead();
            else if (r == 2) owner.duplicateSelectedClip();
            else if (r == 3) owner.deleteSelectedClip();
            else if (r == 4) c->setMuted (! c->isMuted());
            else if (r == 5)
            {
                auto* w = new juce::AlertWindow ("Rename clip", {}, juce::MessageBoxIconType::NoIcon);
                w->addTextEditor ("name", c->getName()); w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0);
                w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, id] (int res) { if (res == 1) if (auto* cc = vs.getSelectedClip(); cc && cc->itemID == id) cc->setName (w->getTextEditorContents ("name")); owner.repaint(); }), true);
            }
            else if (ac != nullptr && r == 10) DawSession::setClipWarp (*ac, ! ac->getAutoTempo(), ac->getPitchChange());
            else if (ac != nullptr && r >= 100 && r <= 124) DawSession::setClipWarp (*ac, ac->getAutoTempo(), (float) (r - 112));
            else if (ac != nullptr && r == 11) ac->setIsReversed (! ac->getIsReversed());
            else if (ac != nullptr && r == 12) { if (ac->isLooping()) ac->disableLooping(); else ac->setLoopRangeBeats ({ te::BeatPosition(), ac->getLengthInBeats() }); }
            else if (ac != nullptr && r >= 188 && r <= 224) ac->setGainDB ((float) (r - 212));
            else if (ac != nullptr && r == 13) owner.vs.session.sliceAtTransients (*ac);
            else if (ac != nullptr && r == 14 && vs.onSeparateClip) vs.onSeparateClip (ac);
            else if (ac != nullptr && r == 15) enableWarpMarkers (*ac, ! ac->getWarpTime());
            else if (r == 20 && vs.onOpenPianoRoll) vs.onOpenPianoRoll();
            else if (ac != nullptr && r >= 400 && r < 500) { vs.edit().getUndoManager().beginNewTransaction ("Choose take"); DawSession::setTake (*ac, r - 400); }
            else if (ac != nullptr && r == 30) { if (auto* t = ac->getTrack()) t->state.setProperty ("vegaTakesShown", true, nullptr); owner.resized(); }
            else if (ac != nullptr && r == 31) { vs.edit().getUndoManager().beginNewTransaction ("Keep take"); DawSession::keepCurrentTakeOnly (*ac); owner.resized(); }
            else if (ac != nullptr && r == 32) { vs.session.unpackTakes (*ac); owner.rebuildHeaders(); }
            else if (r == 40)
            {
                if (auto* t = dynamic_cast<te::AudioTrack*> (c->getTrack()))
                {
                    int sc = 0; vs.session.ensureScenes (8);
                    while (DawSession::slotClip (*t, sc) != nullptr) ++sc;
                    vs.session.ensureScenes (sc + 1);
                    vs.edit().getUndoManager().beginNewTransaction ("Copy to launcher");
                    if (vs.session.copyToSlot (*c, *t, sc) != nullptr && vs.onShowSession) vs.onShowSession();
                }
            }
            else if (r == 21) if (auto* mc = dynamic_cast<te::MidiClip*> (c)) quantise (*mc);
            owner.repaint();
        });
    }
    void quantise (te::MidiClip& mc)
    {
        const int div = std::max (1, vs.snapDivisions());
        auto* um = &vs.edit().getUndoManager(); um->beginNewTransaction ("Quantise");
        for (auto* n : mc.getSequence().getNotes())
        {
            const double s = std::round (n->getStartBeat().inBeats() * div) / div;
            n->setStartAndLength (te::BeatPosition::fromBeats (s), n->getLengthBeats(), um);
        }
    }
    int dropY = -1;
private:
    ArrangeView& owner; ViewState& vs;
    std::map<juce::String, std::unique_ptr<te::SmartThumbnail>> thumbs;
    Drag drag = Drag::none; bool dragged = false;
    te::TimePosition origStart, origEnd; te::TimeDuration origOffset, origFadeIn, origFadeOut; double grabBeat = 0;
};

//==============================================================================
class ArrangeView::Ruler : public juce::Component
{
public:
    explicit Ruler (ArrangeView& o) : owner (o), vs (o.vs) {}
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1b1b));
        auto& tr = vs.edit().getTransport();
        // loop range
        const auto lr = tr.getLoopRange();
        const float lx0 = vs.beatToX (vs.timeToBeats (lr.getStart()), 0), lx1 = vs.beatToX (vs.timeToBeats (lr.getEnd()), 0);
        g.setColour ((tr.looping ? VegaColours::accent : VegaColours::textDim).withAlpha (tr.looping ? 0.35f : 0.15f));
        g.fillRect (juce::Rectangle<float> (lx0, 0, lx1 - lx0, 9.0f));
        const double first = std::floor (vs.scrollBeats), last = vs.xToBeat ((float) getWidth(), 0);
        g.setFont (VegaFonts::ui (10.5f, true));
        for (double b = first; b <= last; b += 1.0)
        {
            const float x = vs.beatToX (b, 0);
            const bool bar = vs.isBarStart (b);
            if (! bar && vs.pixelsPerBeat < 10) continue;
            g.setColour (bar ? VegaColours::text : VegaColours::textDim.withAlpha (0.5f));
            g.drawVerticalLine ((int) x, bar ? 12.0f : 21.0f, (float) getHeight());
            if (bar)
            {
                const int barNum = vs.barNumber (b);
                if (vs.pixelsPerBeat * vs.beatsPerBarAt (b) > 28 || (barNum - 1) % 4 == 0)
                    g.drawText (juce::String (barNum), (int) x + 3, 10, 40, 14, juce::Justification::left);
            }
        }
        // tempo and time-signature changes (the first ones are in the transport bar)
        auto& ts = vs.edit().tempoSequence;
        g.setFont (VegaFonts::ui (9.0f, true));
        for (int i = 1; i < ts.getNumTempos(); ++i)
        {
            auto* t = ts.getTempo (i);
            const float x = vs.beatToX (t->getStartBeat().inBeats(), 0);
            if (x < -60 || x > getWidth()) continue;
            g.setColour (VegaColours::gold); g.fillRect (x, 21.0f, 1.5f, (float) getHeight() - 21);
            g.drawText (juce::String (t->getBpm(), 1) + " BPM", (int) x + 3, getHeight() - 11, 70, 10, juce::Justification::left);
        }
        for (int i = 1; i < ts.getNumTimeSigs(); ++i)
        {
            auto* sg = ts.getTimeSig (i);
            const float x = vs.beatToX (sg->getStartBeat().inBeats(), 0);
            if (x < -40 || x > getWidth()) continue;
            g.setColour (juce::Colour (0xffb183f2)); g.fillRect (x, 21.0f, 1.5f, (float) getHeight() - 21);
            g.drawText (sg->getStringTimeSig(), (int) x + 3, getHeight() - 21, 40, 10, juce::Justification::left);
        }
        // markers
        for (auto& m : vs.edit().getMarkerManager().getMarkers())
        {
            const float mx = vs.beatToX (vs.timeToBeats (m->getPosition().getStart()), 0);
            if (mx < -40 || mx > getWidth()) continue;
            juce::Path flag; flag.addTriangle (mx, 10, mx + 9, 14, mx, 18);
            g.setColour (juce::Colour (0xfff2867c)); g.fillPath (flag); g.fillRect (mx, 10.0f, 1.2f, (float) getHeight() - 10);
            const auto label = m->getName().isNotEmpty() && m->getName() != juce::String (m->getMarkerID()) ? m->getName() : juce::String (m->getMarkerID());
            g.setFont (VegaFonts::ui (9.0f, true)); g.drawText (label, (int) mx + 10, 9, 90, 10, juce::Justification::left);
        }
        const float px = vs.beatToX (vs.timeToBeats (tr.getPosition()), 0);
        juce::Path tri; tri.addTriangle (px - 6, 18, px + 6, 18, px, (float) getHeight());
        g.setColour (VegaColours::gold); g.fillPath (tri);
        g.setColour (VegaColours::line); g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { menu (e); return; }
        loopDrag = e.position.y < 10 || e.mods.isShiftDown();
        startBeat = vs.snapBeat (vs.xToBeat (e.position.x, 0));
        if (! loopDrag) setPos (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! loopDrag) { setPos (e); return; }
        const double b = vs.snapBeat (vs.xToBeat (e.position.x, 0));
        if (std::abs (b - startBeat) < 1e-6) return;
        auto& tr = vs.edit().getTransport();
        tr.setLoopRange ({ vs.beatsToTime (std::max (0.0, std::min (b, startBeat))), vs.beatsToTime (std::max (b, startBeat)) });
        tr.looping = true;
        repaint(); owner.lanes->repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.position.y < 10) { auto& tr = vs.edit().getTransport(); tr.looping = ! tr.looping.get(); repaint(); return; }
        if (auto* m = markerNear (e.position.x)) renameMarker (m);
    }
private:
    te::MarkerClip* markerNear (float x)
    {
        for (auto& m : vs.edit().getMarkerManager().getMarkers())
            if (std::abs (vs.beatToX (vs.timeToBeats (m->getPosition().getStart()), 0) - x) < 8) return m;
        return nullptr;
    }
    void renameMarker (te::MarkerClip* m)
    {
        auto* w = new juce::AlertWindow ("Rename marker", {}, juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", m->getName()); w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, id = m->itemID] (int r)
        {
            if (r != 1) return;
            for (auto& mm : vs.edit().getMarkerManager().getMarkers()) if (mm->itemID == id) mm->setName (w->getTextEditorContents ("name"));
            repaint();
        }), true);
    }
    void menu (const juce::MouseEvent& e)
    {
        const auto at = vs.beatsToTime (std::max (0.0, vs.snapBeat (vs.xToBeat (e.position.x, 0))));
        auto* near = markerNear (e.position.x);
        juce::PopupMenu m;
        m.addItem (1, "Add marker here");
        if (near != nullptr) { m.addItem (2, "Rename marker..."); m.addItem (3, "Delete marker"); }
        m.addSeparator();
        m.addItem (4, "Tempo change here...");
        m.addItem (5, "Time signature change here (next bar)...");
        m.addSeparator();
        m.addItem (6, "Markers & tempo list...");
        m.showMenuAsync ({}, [this, at, nearID = near != nullptr ? near->itemID : te::EditItemID()] (int r)
        {
            auto& ss = vs.session;
            auto find = [this, nearID] () -> te::MarkerClip* { for (auto* mm : vs.edit().getMarkerManager().getMarkers()) if (mm->itemID == nearID) return mm; return nullptr; };
            if (r == 1) ss.addMarker (at);
            else if (r == 2) { if (auto* mk = find()) renameMarker (mk); }
            else if (r == 3) { if (auto* mk = find()) mk->removeFromParent(); }
            else if (r == 4)
            {
                auto* w = new juce::AlertWindow ("Tempo change", "New tempo from " + dawui::formatBarsBeats (vs.edit(), at) + " on:", juce::MessageBoxIconType::NoIcon);
                w->addTextEditor ("bpm", juce::String (vs.edit().tempoSequence.getBpmAt (at), 1), "BPM");
                w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0);
                w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, at] (int res)
                {
                    const double bpm = w->getTextEditorContents ("bpm").getDoubleValue();
                    if (res == 1 && bpm >= 20 && bpm <= 400) { vs.edit().getUndoManager().beginNewTransaction ("Tempo change"); vs.session.addTempoChange (at, bpm); }
                    repaint();
                }), true);
            }
            else if (r == 5)
            {
                auto* w = new juce::AlertWindow ("Time signature change", "From the next bar line on:", juce::MessageBoxIconType::NoIcon);
                w->addComboBox ("sig", { "4/4", "3/4", "6/8", "7/8", "5/4", "2/4", "12/8" });
                w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0);
                w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, at] (int res)
                {
                    if (res != 1) return;
                    const auto parts = juce::StringArray::fromTokens (w->getComboBoxComponent ("sig")->getText(), "/", {});
                    vs.edit().getUndoManager().beginNewTransaction ("Time signature change");
                    vs.session.addTimeSigChange (at, parts[0].getIntValue(), parts[1].getIntValue());
                    repaint();
                }), true);
            }
            else if (r == 6 && vs.onShowSongMap) vs.onShowSongMap();
            repaint();
        });
    }
    void setPos (const juce::MouseEvent& e)
    {
        vs.edit().getTransport().setPosition (vs.beatsToTime (std::max (0.0, vs.snapBeat (vs.xToBeat (e.position.x, 0), e.mods.isShiftDown()))));
        repaint(); owner.lanes->repaint();
    }
    ArrangeView& owner; ViewState& vs; bool loopDrag = false; double startBeat = 0;
};

//==============================================================================
ArrangeView::ArrangeView (ViewState& v) : vs (v)
{
    ruler = std::make_unique<Ruler> (*this); lanes = std::make_unique<Lanes> (*this);
    addAndMakeVisible (*ruler); addAndMakeVisible (*lanes);
    addAndMakeVisible (hbar); addAndMakeVisible (vbar);
    hbar.setAutoHide (false); vbar.setAutoHide (false);
    hbar.addListener (this); vbar.addListener (this);
    setWantsKeyboardFocus (true);
    editChanged();
    startTimerHz (30);
}
ArrangeView::~ArrangeView() { watched.removeListener (this); }
void ArrangeView::scrollBarMoved (juce::ScrollBar* bar, double s)
{
    if (bar == &hbar) { if (std::abs (s - vs.scrollBeats) > 1e-6) { vs.scrollBeats = s; vs.followPlayhead = false; repaint(); } }
    else if ((int) s != vs.scrollY) { vs.scrollY = (int) s; resized(); }
}

void ArrangeView::editChanged()
{
    watched.removeListener (this);
    watched = vs.edit().state;
    watched.addListener (this);
    vs.selectedClip = {}; vs.selectedTrack = {};
    rebuildHeaders();
}

void ArrangeView::rebuildHeaders()
{
    headers.clear();
    for (auto* t : visibleTracks())
    {
        auto* h = headers.add (new TrackHeader (*this, *t));
        addAndMakeVisible (h);
    }
    resized(); repaint();
}

int ArrangeView::totalTracksHeight() { int h = 0; for (auto* t : visibleTracks()) h += trackHeight (*t); return h; }
te::AudioTrack* ArrangeView::trackAtY (int y, int* rowTop)
{
    int top = -vs.scrollY;
    for (auto* t : visibleTracks())
    {
        const int h = trackHeight (*t);
        if (y >= top && y < top + h) { if (rowTop) *rowTop = top; return t; }
        top += h;
    }
    return nullptr;
}
int ArrangeView::trackTop (te::AudioTrack& tr) { int top = -vs.scrollY; for (auto* t : visibleTracks()) { if (t == &tr) return top; top += trackHeight (*t); } return top; }

void ArrangeView::resized()
{
    const int sb = 12;
    ruler->setBounds (headerW, 0, getWidth() - headerW - sb, rulerH);
    lanes->setBounds (headerW, rulerH, getWidth() - headerW - sb, getHeight() - rulerH - sb);
    hbar.setBounds (headerW, getHeight() - sb, getWidth() - headerW - sb, sb);
    vbar.setBounds (getWidth() - sb, rulerH, sb, getHeight() - rulerH - sb);
    const int total = totalTracksHeight() + 120;
    vs.scrollY = juce::jlimit (0, std::max (0, total - lanes->getHeight()), vs.scrollY);
    vbar.setRangeLimits (0, std::max (total, lanes->getHeight())); vbar.setCurrentRange (vs.scrollY, lanes->getHeight(), juce::dontSendNotification);
    int y = rulerH - vs.scrollY;
    for (auto* h : headers)
    {
        const int th = h->track != nullptr ? trackHeight (*h->track) : 80;
        h->setBounds (0, y, headerW, th);
        h->setVisible (y + th > rulerH && y < getHeight());
        y += th;
    }
    lanes->toFront (false); ruler->toFront (false);
}

void ArrangeView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1d1d1d));
    g.setColour (juce::Colour (0xff1b1b1b)); g.fillRect (0, 0, headerW, rulerH);
    g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, true));
    g.drawText ("TRACKS", 10, 0, 100, rulerH, juce::Justification::centredLeft);
    g.setColour (VegaColours::line); g.drawVerticalLine (headerW - 1, 0, (float) getHeight());
}

void ArrangeView::timerCallback()
{
    if (needsRebuild) { needsRebuild = false; rebuildHeaders(); }
    auto& tr = vs.edit().getTransport();
    if (tr.isPlaying() && vs.followPlayhead)
    {
        const double pb = vs.timeToBeats (tr.getPosition()), visible = lanes->getWidth() / vs.pixelsPerBeat;
        if (pb > vs.scrollBeats + visible * 0.92 || pb < vs.scrollBeats) vs.scrollBeats = std::max (0.0, pb - visible * 0.1);
    }
    const double totalBeats = std::max (vs.timeToBeats (vs.session.contentRange().getEnd()) + 64.0, vs.scrollBeats + lanes->getWidth() / vs.pixelsPerBeat);
    hbar.setRangeLimits (0, totalBeats); hbar.setCurrentRange (vs.scrollBeats, lanes->getWidth() / vs.pixelsPerBeat, juce::dontSendNotification);
    if (++tick % 10 == 0 || needsRepaint) { for (auto* h : headers) h->refresh(); needsRepaint = false; }
    ruler->repaint(); lanes->repaint();
}

void ArrangeView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {   // zoom around the mouse
        const float x = (float) (e.getEventRelativeTo (lanes.get()).position.x);
        const double anchor = vs.xToBeat (x, 0);
        vs.pixelsPerBeat = juce::jlimit (2.0, 400.0, vs.pixelsPerBeat * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15));
        vs.scrollBeats = std::max (0.0, anchor - x / vs.pixelsPerBeat);
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const float d = std::abs (w.deltaX) > 0 ? w.deltaX : w.deltaY;
        vs.scrollBeats = std::max (0.0, vs.scrollBeats - d * 300.0 / vs.pixelsPerBeat);
        vs.followPlayhead = false;
    }
    else { vs.scrollY = std::max (0, vs.scrollY - (int) (w.deltaY * 240)); }
    resized(); repaint();
}

bool ArrangeView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSelectedClip(); return true; }
    if (k == juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0)) { duplicateSelectedClip(); return true; }
    if (k == juce::KeyPress ('e', juce::ModifierKeys::commandModifier, 0)) { splitSelectedAtPlayhead(); return true; }
    return false;
}

void ArrangeView::deleteSelectedClip()
{
    if (auto* c = vs.getSelectedClip()) { vs.edit().getUndoManager().beginNewTransaction ("Delete clip"); c->removeFromParent(); vs.selectClip (nullptr); repaint(); }
}
void ArrangeView::duplicateSelectedClip()
{
    auto* c = vs.getSelectedClip(); if (c == nullptr) return;
    auto* track = dynamic_cast<te::ClipTrack*> (c->getTrack()); if (track == nullptr) return;
    vs.edit().getUndoManager().beginNewTransaction ("Duplicate clip");
    auto state = c->state.createCopy();
    te::EditItemID::remapIDs (state, nullptr, vs.edit());
    if (auto* nc = track->insertClipWithState (state))
    {
        const auto len = c->getPosition().getLength();
        nc->setStart (c->getPosition().getEnd(), false, true);
        nc->setLength (len, false);
        vs.selectClip (nc);
    }
    repaint();
}
void ArrangeView::splitSelectedAtPlayhead()
{
    if (auto* c = vs.getSelectedClip()) { vs.edit().getUndoManager().beginNewTransaction ("Split"); vs.session.splitClip (*c, vs.edit().getTransport().getPosition()); repaint(); }
}

void ArrangeView::filesDropped (const juce::StringArray& files, int x, int y)
{
    lanes->dropY = -1;
    auto p = lanes->getLocalPoint (this, juce::Point<int> (x, y));
    const double beat = std::max (0.0, vs.snapBeat (vs.xToBeat ((float) p.x, 0)));
    auto* target = trackAtY (p.y);
    for (auto& path : files)
    {
        const juce::File f (path);
        const bool midi = f.hasFileExtension ("mid;midi");
        if (target == nullptr || (DawSession::kindOf (*target) == DawSession::TrackKind::midi) != midi || DawSession::kindOf (*target) == DawSession::TrackKind::bus)
            target = vs.session.addTrack (midi ? DawSession::TrackKind::midi : DawSession::TrackKind::audio, f.getFileNameWithoutExtension());
        if (target == nullptr) continue;
        if (midi) vs.session.importMidiFile (*target, f, vs.beatsToTime (beat));
        else vs.session.insertAudioFile (*target, f, vs.beatsToTime (beat));
        target = nullptr;
    }
    rebuildHeaders();
}

void ArrangeView::itemDropped (const SourceDetails& d)
{
    auto p = lanes->getLocalPoint (this, d.localPosition.toInt());
    auto* t = p.y >= 0 ? trackAtY (p.y) : nullptr;
    if (vs.onPluginDropped) vs.onPluginDropped (d.description, t);
}

void ArrangeView::clearAll()
{
    headers.clear();
    if (lanes != nullptr) lanes->forgetEdit();
}

int ArrangeView::maxTakes (te::Track& t)
{
    int n = 0;
    if (auto* ct = dynamic_cast<te::ClipTrack*> (&t))
        for (auto* c : ct->getClips())
            if (dynamic_cast<te::WaveAudioClip*> (c) != nullptr) n = std::max (n, c->state.getChildWithName (te::IDs::TAKES).getNumChildren());
    return n;
}

juce::Array<te::AudioTrack*> ArrangeView::visibleTracks() const
{
    juce::Array<te::AudioTrack*> r;
    for (auto* t : vs.session.getTracks()) if (! vs.session.isHiddenByFolder (*t)) r.add (t);
    return r;
}
