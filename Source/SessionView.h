#pragma once
#include "DawUI.h"

/** Clip launcher: a grid of clip slots (tracks across, scenes down), like a Session view.
    Launching a slot clip takes that track over from the arrangement until "Back to arrangement". */
class SessionView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit SessionView (ViewState& v) : vs (v)
    {
        quant.addItemList ({ "Launch: none", "Launch: 8 bars", "Launch: 4 bars", "Launch: 2 bars", "Launch: 1 bar", "Launch: 1/2", "Launch: 1/4", "Launch: 1/8", "Launch: 1/16" }, 1);
        quant.setTooltip ("Global launch quantise: clips start on the next ...");
        quant.onChange = [this]
        {
            static const te::LaunchQType q[] { te::LaunchQType::none, te::LaunchQType::eightBars, te::LaunchQType::fourBars, te::LaunchQType::twoBars, te::LaunchQType::bar,
                                              te::LaunchQType::half, te::LaunchQType::quarter, te::LaunchQType::eighth, te::LaunchQType::sixteenth };
            vs.edit().getLaunchQuantisation().type = q[juce::jlimit (0, 8, quant.getSelectedItemIndex())];
        };
        addAndMakeVisible (quant);
        for (auto* b : { &stopAll, &backBtn, &addScene }) addAndMakeVisible (b);
        stopAll.setButtonText (juce::String::fromUTF8 ("\xe2\x96\xa0  STOP CLIPS")); backBtn.setButtonText ("BACK TO ARRANGEMENT"); addScene.setButtonText ("+ SCENE");
        backBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe8a23a));
        backBtn.setTooltip ("Tracks playing launcher clips go back to playing their arrangement clips");
        stopAll.onClick = [this] { vs.session.stopAllSlots(); };
        backBtn.onClick = [this] { vs.session.backToArrangement(); };
        addScene.onClick = [this] { vs.edit().getUndoManager().beginNewTransaction ("Add scene"); vs.session.ensureScenes (vs.session.numScenes() + 1); repaint(); };
        startTimerHz (20);
    }
    void editChanged() { scrollX = scrollY = 0; refreshQuant(); recording = {}; }

    void visibilityChanged() override { if (isVisible()) { vs.session.ensureScenes (8); refreshQuant(); } }
    void resized() override
    {
        auto top = getLocalBounds().removeFromTop (topH).reduced (8, 7);
        quant.setBounds (top.removeFromLeft (150)); top.removeFromLeft (8);
        stopAll.setBounds (top.removeFromLeft (120)); top.removeFromLeft (8);
        backBtn.setBounds (top.removeFromLeft (190)); top.removeFromLeft (8);
        addScene.setBounds (top.removeFromLeft (80));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff17181a));
        g.setColour (juce::Colour (0xff1e1f22)); g.fillRect (0, 0, getWidth(), topH);
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, true));
        g.drawText ("CLIP LAUNCHER  -  click a clip's triangle to launch it, a scene's triangle to launch the row. Double-click an empty slot on an instrument track for a new clip, or drop audio / MIDI files on a slot.",
                    backBtn.getRight() + 100, 0, getWidth() - backBtn.getRight() - 110, topH, juce::Justification::centredLeft, true);
        auto tracks = columns();
        const int scenes = vs.session.numScenes();
        const auto area = gridArea();
        g.saveState(); g.reduceClipRegion (area.withTrimmedTop (-headH).withHeight (area.getHeight() + headH + footH));
        auto& scList = vs.edit().getSceneList();
        for (int c = 0; c < tracks.size(); ++c)
        {
            auto* t = tracks[c];
            const int x = area.getX() + c * colW - scrollX;
            if (x + colW < area.getX() || x > area.getRight()) continue;
            const auto col = DawSession::trackColour (*t);
            // header
            auto h = juce::Rectangle<int> (x, area.getY() - headH, colW, headH).reduced (2, 0);
            g.setColour (juce::Colour (vs.selectedTrack == t->itemID ? 0xff2a2c31 : 0xff1f2023)); g.fillRect (h);
            g.setColour (col); g.fillRect (h.removeFromBottom (3));
            const bool slotsOn = t->playSlotClips.get();
            g.setColour (col.brighter (0.2f)); g.setFont (VegaFonts::ui (12.5f, true));
            g.drawText (juce::String (c + 1) + ":" + t->getName().toUpperCase(), h.reduced (4, 0), juce::Justification::centred, true);
            if (slotsOn) { g.setColour (juce::Colour (0xff3ddc84)); g.fillEllipse ((float) h.getRight() - 8, (float) h.getY() + 4, 5, 5); }
            // slots
            for (int sIdx = 0; sIdx < scenes; ++sIdx)
            {
                auto r = cellBounds (c, sIdx);
                if (r.getBottom() < area.getY() || r.getY() > area.getBottom()) continue;
                drawSlot (g, *t, sIdx, r.toFloat().reduced (1.5f), col);
            }
            // stop button
            auto st = juce::Rectangle<int> (x, area.getBottom(), colW, footH).reduced (3, 4);
            g.setColour (juce::Colour (0xff222327)); g.fillRoundedRectangle (st.toFloat(), 3.0f);
            g.setColour (anyPlaying (*t) ? VegaColours::text : VegaColours::textDim.withAlpha (0.4f));
            g.fillRect (st.withSizeKeepingCentre (9, 9));
        }
        // scenes column
        for (int sIdx = 0; sIdx < scenes; ++sIdx)
        {
            auto r = sceneBounds (sIdx);
            if (r.getBottom() < area.getY() || r.getY() > area.getBottom()) continue;
            const bool active = sIdx == activeScene;
            auto box = r.toFloat().reduced (2.0f, 2.5f);
            g.setColour (active ? juce::Colour (0xff3a3622) : juce::Colour (0xff222327)); g.fillRoundedRectangle (box, 3.0f);
            if (active) { g.setColour (VegaColours::gold); g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.5f); }
            auto btn = box.removeFromRight (box.getHeight()).reduced (1.0f);
            g.setColour (juce::Colour (active ? 0xff2c3a2c : 0xff2c2d31)); g.fillRoundedRectangle (btn, 3.0f);
            auto tri = btn.withSizeKeepingCentre (12, 14);
            juce::Path p; p.addTriangle (tri.getX(), tri.getY(), tri.getX(), tri.getBottom(), tri.getRight(), tri.getCentreY());
            g.setColour (active ? juce::Colour (0xff3ddc84) : VegaColours::textDim); g.fillPath (p);
            juce::String name;
            if (auto* sc = scList.getScenes()[sIdx]) name = sc->name.get();
            g.setColour (active ? VegaColours::gold : VegaColours::text); g.setFont (VegaFonts::ui (12.0f, true));
            g.drawText (juce::String (sIdx + 1) + ":" + (name.isNotEmpty() ? name : "Scene " + juce::String (sIdx + 1)).toUpperCase(), box.reduced (10, 0).toNearestInt(), juce::Justification::centredLeft, true);
            if (active) { g.setFont (VegaFonts::ui (9.0f, true)); g.drawText ("ACTIVE", box.reduced (8, 0).toNearestInt(), juce::Justification::centredRight); }
        }
        // the box an APC Key 25 (or other launcher controller) is showing
        if (vs.controllerWindow)
            if (auto box = vs.controllerWindow(); ! box.isEmpty())
            {
                auto a = cellBounds (box.getX(), box.getY()).getUnion (cellBounds (box.getRight() - 1, box.getBottom() - 1)).toFloat().expanded (1.5f);
                g.setColour (juce::Colour (0xffe04848)); g.drawRoundedRectangle (a, 4.0f, 2.0f);
            }
        g.restoreState();
        if (tracks.isEmpty())
        {
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (13.0f));
            g.drawText ("Add an audio or instrument track to use the clip launcher", area, juce::Justification::centred);
        }
        g.setColour (VegaColours::line); g.drawHorizontalLine (topH - 1, 0, (float) getWidth());
    }

    // ---------------------------------------------------------------- mouse
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragFrom = {}; dragging = false;
        auto tracks = columns();
        int c = -1, sIdx = -1;
        if (sceneAt (e.getPosition(), sIdx))
        {
            if (e.mods.isPopupMenu()) sceneMenu (sIdx);
            else { vs.session.launchScene (sIdx); activeScene = sIdx; repaint(); }
            return;
        }
        if (stopAt (e.getPosition(), c)) { vs.session.stopTrackSlots (*tracks[c]); return; }
        if (headerAt (e.getPosition(), c)) { vs.selectTrack (tracks[c]); return; }
        if (! cellAt (e.getPosition(), c, sIdx)) return;
        auto* t = tracks[c];
        vs.selectTrack (t);
        auto* clip = DawSession::slotClip (*t, sIdx);
        if (e.mods.isPopupMenu()) { if (clip != nullptr) clipMenu (*clip); else emptyMenu (*t, sIdx); return; }
        if (clip == nullptr) return;
        const auto r = cellBounds (c, sIdx);
        if (ringBounds (r.toFloat()).expanded (6).contains (e.position)) { vs.session.launchClip (*clip); return; }
        vs.selectClip (clip);
        dragFrom = { c, sIdx };
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragFrom.first >= 0 && e.getDistanceFromDragStart() > 6) { dragging = true; repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        dragging = false;
        auto tracks = columns();
        int c = -1, sIdx = -1;
        if (! cellAt (e.getPosition(), c, sIdx) || (c == dragFrom.first && sIdx == dragFrom.second)) return;
        auto* from = tracks[dragFrom.first];
        auto* clip = DawSession::slotClip (*from, dragFrom.second);
        if (clip == nullptr) return;
        const bool midiClip = dynamic_cast<te::AudioClipBase*> (clip) == nullptr;
        if (midiClip != (DawSession::kindOf (*tracks[c]) == DawSession::TrackKind::midi)) return;     // MIDI to instrument tracks, audio to audio tracks
        vs.edit().getUndoManager().beginNewTransaction (e.mods.isAltDown() ? "Copy slot" : "Move slot");
        if (auto* nc = vs.session.copyToSlot (*clip, *tracks[c], sIdx))
        {
            if (! e.mods.isAltDown()) clip->removeFromParent();
            vs.selectClip (nc);
        }
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto tracks = columns();
        int c = -1, sIdx = -1;
        if (! cellAt (e.getPosition(), c, sIdx)) return;
        auto* t = tracks[c];
        if (auto* clip = DawSession::slotClip (*t, sIdx))
        {
            vs.selectClip (clip);
            if (dynamic_cast<te::MidiClip*> (clip) != nullptr || dynamic_cast<te::StepClip*> (clip) != nullptr) if (vs.onOpenPianoRoll) vs.onOpenPianoRoll();
            return;
        }
        if (DawSession::kindOf (*t) == DawSession::TrackKind::midi) newMidi (*t, sIdx, 2);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY)) scrollX = std::max (0, scrollX - (int) ((std::abs (w.deltaX) > 0 ? w.deltaX : w.deltaY) * 300));
        else scrollY = std::max (0, scrollY - (int) (w.deltaY * 240));
        clampScroll(); repaint();
    }
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files) if (juce::File (f).hasFileExtension (te::soundFileAndMidiExtensions)) return true;
        return false;
    }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        auto tracks = columns();
        int c = -1, sIdx = -1;
        if (! cellAt ({ x, y }, c, sIdx)) return;
        for (auto& path : files)
        {
            const juce::File f (path);
            if (! f.hasFileExtension (te::soundFileAndMidiExtensions)) continue;
            const bool midi = f.hasFileExtension (te::midiFileExtensions);
            if (midi != (DawSession::kindOf (*tracks[c]) == DawSession::TrackKind::midi)) continue;
            vs.edit().getUndoManager().beginNewTransaction ("Drop on slot");
            vs.session.insertSlotFile (*tracks[c], sIdx, f);
            if (++sIdx >= vs.session.numScenes()) vs.session.ensureScenes (sIdx + 1);
        }
        repaint();
    }

    // for the APC: record into an empty slot / stop that recording
    bool startSlotRecording (te::AudioTrack& t, int sIdx) { recordIntoSlot (t, sIdx); return recording.started; }
    bool isRecordingSlot (te::AudioTrack& t, int sIdx) const { return recording.started && recording.track == t.itemID && recording.scene == sIdx; }
    void stopSlotRecording() { if (recording.started) vs.edit().getTransport().stop (false, false); }
private:
    static constexpr int topH = 40, headH = 34, footH = 30, colW = 118, rowH = 52, sceneW = 200;
    int activeScene = -1;                 // the scene launched last (shown ACTIVE)
    juce::Rectangle<int> gridArea() const { return { 8, topH + headH, getWidth() - sceneW - 16, getHeight() - topH - headH - footH - 4 }; }
    juce::Rectangle<int> cellBounds (int c, int s) const { const auto a = gridArea(); return { a.getX() + c * colW - scrollX, a.getY() + s * rowH - scrollY, colW, rowH }; }
    juce::Rectangle<int> sceneBounds (int s) const { const auto a = gridArea(); return { a.getRight() + 8, a.getY() + s * rowH - scrollY, sceneW, rowH }; }
    juce::Array<te::AudioTrack*> columns() const
    {
        juce::Array<te::AudioTrack*> r;
        for (auto* t : vs.session.getTracks (false)) if (! vs.session.isHiddenByFolder (*t)) r.add (t);
        return r;
    }
    bool cellAt (juce::Point<int> p, int& c, int& s) const
    {
        const auto a = gridArea();
        if (! a.contains (p)) return false;
        c = (p.x - a.getX() + scrollX) / colW; s = (p.y - a.getY() + scrollY) / rowH;
        return juce::isPositiveAndBelow (c, columns().size()) && juce::isPositiveAndBelow (s, vs.session.numScenes());
    }
    bool sceneAt (juce::Point<int> p, int& s) const
    {
        const auto a = gridArea();
        if (p.x < a.getRight() + 8 || p.x > a.getRight() + 8 + sceneW || p.y < a.getY() || p.y > a.getBottom()) return false;
        s = (p.y - a.getY() + scrollY) / rowH;
        return juce::isPositiveAndBelow (s, vs.session.numScenes());
    }
    bool headerAt (juce::Point<int> p, int& c) const
    {
        const auto a = gridArea();
        if (p.y < a.getY() - headH || p.y >= a.getY() || p.x < a.getX() || p.x > a.getRight()) return false;
        c = (p.x - a.getX() + scrollX) / colW;
        return juce::isPositiveAndBelow (c, columns().size());
    }
    bool stopAt (juce::Point<int> p, int& c) const
    {
        const auto a = gridArea();
        if (p.y < a.getBottom() || p.y > a.getBottom() + footH || p.x < a.getX() || p.x > a.getRight()) return false;
        c = (p.x - a.getX() + scrollX) / colW;
        return juce::isPositiveAndBelow (c, columns().size());
    }
    void clampScroll()
    {
        const auto a = gridArea();
        scrollX = juce::jlimit (0, std::max (0, columns().size() * colW - a.getWidth()), scrollX);
        scrollY = juce::jlimit (0, std::max (0, vs.session.numScenes() * rowH - a.getHeight()), scrollY);
    }
    static bool anyPlaying (te::AudioTrack& t)
    {
        for (auto* s : t.getClipSlotList().getClipSlots()) if (auto* c = s != nullptr ? s->getClip() : nullptr) if (DawSession::isSlotPlaying (*c) || DawSession::isSlotQueued (*c)) return true;
        return false;
    }

    static juce::Rectangle<float> ringBounds (juce::Rectangle<float> cell)
    {
        const float d = juce::jmin (28.0f, cell.getHeight() - 16.0f);
        return juce::Rectangle<float> (d, d).withCentre (cell.getCentre());
    }
    void drawSlot (juce::Graphics& g, te::AudioTrack& t, int sIdx, juce::Rectangle<float> r, juce::Colour col)
    {
        auto* clip = DawSession::slotClip (t, sIdx);
        const bool isRec = recording.track == t.itemID && recording.scene == sIdx;
        r = r.reduced (1.5f, 2.0f);
        if (clip == nullptr)
        {
            g.setColour (juce::Colour (0xff1c1d20)); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (col.withAlpha (0.10f)); g.fillRoundedRectangle (r, 3.0f);
            if (isRec || dawui::isArmed (t))
            {
                g.setColour (juce::Colour (isRec && (blink % 10 < 5) ? 0xffe04848 : 0x66e04848));
                g.fillEllipse (r.withSizeKeepingCentre (10, 10));
            }
            return;
        }
        const bool playing = DawSession::isSlotPlaying (*clip), queued = DawSession::isSlotQueued (*clip);
        const bool sel = vs.selectedClip == clip->itemID;
        const bool flash = queued && blink % 6 < 3;
        // body: a stopped clip is a dark tint of its track colour, a playing one lights up fully
        if (playing || flash) { g.setColour (col.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (2.0f), 5.0f); }
        g.setColour (playing ? col : col.withAlpha (flash ? 0.6f : 0.22f)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (col.withAlpha (playing ? 1.0f : 0.45f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        // launch ring (click it to launch); while playing it shows how far through the loop the clip is
        auto ring = ringBounds (r);
        const float thick = 3.0f;
        juce::Path circle; circle.addCentredArc (ring.getCentreX(), ring.getCentreY(), ring.getWidth() / 2, ring.getHeight() / 2, 0, 0, juce::MathConstants<float>::twoPi, true);
        if (playing)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f)); g.strokePath (circle, juce::PathStrokeType (thick));
            double prog = 0;
            if (auto lh = clip->getLaunchHandle())
                if (auto played = lh->getPlayedRange())
                {
                    const double len = clip->isLooping() ? clip->getLoopLengthBeats().inBeats() : clip->getLengthInBeats().inBeats();
                    if (len > 0) prog = std::fmod (played->getLength().inBeats(), len) / len;
                }
            juce::Path arc; arc.addCentredArc (ring.getCentreX(), ring.getCentreY(), ring.getWidth() / 2, ring.getHeight() / 2, 0, 0, (float) (prog * juce::MathConstants<double>::twoPi), true);
            g.setColour (juce::Colours::white); g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour (col.withAlpha (flash ? 1.0f : 0.85f)); g.strokePath (circle, juce::PathStrokeType (thick));
            // a small play triangle in the middle of the ring
            auto tri = ring.withSizeKeepingCentre (8, 9).translated (1, 0);
            juce::Path p; p.addTriangle (tri.getX(), tri.getY(), tri.getX(), tri.getBottom(), tri.getRight(), tri.getCentreY());
            g.setColour (col.withAlpha (0.6f)); g.fillPath (p);
        }
        g.setColour (playing ? juce::Colour (0xff111111) : juce::Colours::white.withAlpha (0.75f)); g.setFont (VegaFonts::ui (9.5f, true));
        g.drawText (clip->getName(), r.reduced (5, 2).toNearestInt(), juce::Justification::topLeft, true);
        if (clip->getFollowActions() != nullptr && ! clip->getFollowActions()->getActions().empty())
        {
            g.setFont (VegaFonts::ui (9.0f, true)); g.drawText (juce::String::fromUTF8 ("\xe2\x86\xbb"), r.reduced (5, 2).toNearestInt(), juce::Justification::bottomRight);
        }
        if (sel) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (r.expanded (1.0f), 4.0f, 1.5f); }
    }

    void newMidi (te::AudioTrack& t, int sIdx, int bars)
    {
        vs.edit().getUndoManager().beginNewTransaction ("New slot clip");
        if (auto* mc = vs.session.createSlotMidiClip (t, sIdx, bars)) { vs.selectClip (mc); if (vs.onOpenPianoRoll) vs.onOpenPianoRoll(); }
    }

    void emptyMenu (te::AudioTrack& t, int sIdx)
    {
        juce::PopupMenu m;
        const bool inst = DawSession::kindOf (t) == DawSession::TrackKind::midi;
        if (inst)
        {
            m.addItem (1, "New MIDI clip (1 bar)"); m.addItem (2, "New MIDI clip (2 bars)"); m.addItem (4, "New MIDI clip (4 bars)");
            m.addItem (5, "New step-sequencer clip (drums)");
        }
        m.addItem (6, inst ? "Insert a MIDI file..." : "Insert an audio file...");
        auto* sel = vs.getSelectedClip();
        const bool canPaste = sel != nullptr && sel->getClipSlot() == nullptr && (dynamic_cast<te::AudioClipBase*> (sel) == nullptr) == inst;
        m.addItem (7, "Copy the selected arrangement clip here", canPaste);
        m.addSeparator();
        m.addItem (8, "Record into this slot");
        m.showMenuAsync ({}, [this, tp = te::AudioTrack::Ptr (&t), sIdx] (int r)
        {
            if (tp == nullptr || r == 0) return;
            if (r <= 4) newMidi (*tp, sIdx, r);
            else if (r == 5)
            {
                vs.session.ensureScenes (sIdx + 1);
                if (auto* s = DawSession::slot (*tp, sIdx))
                {
                    vs.edit().getUndoManager().beginNewTransaction ("New step clip");
                    const auto bar = vs.edit().tempoSequence.toTime (te::BeatPosition::fromBeats (vs.edit().tempoSequence.getTimeSig (0)->numerator.get()));
                    if (auto* c = te::insertNewClip (*s, te::TrackItem::Type::step, "Steps", te::TimeRange (te::TimePosition(), bar)))
                    { te::prepareClipForLauncher (*c); vs.selectClip (c); if (vs.onOpenPianoRoll) vs.onOpenPianoRoll(); }
                }
            }
            else if (r == 6)
            {
                const bool midi = DawSession::kindOf (*tp) == DawSession::TrackKind::midi;
                chooser = std::make_unique<juce::FileChooser> ("Insert into slot", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                               midi ? "*.mid;*.midi" : "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, tp, sIdx] (const juce::FileChooser& fc)
                {
                    if (fc.getResult().existsAsFile() && tp != nullptr) { vs.edit().getUndoManager().beginNewTransaction ("Insert into slot"); vs.session.insertSlotFile (*tp, sIdx, fc.getResult()); }
                });
            }
            else if (r == 7) { if (auto* c = vs.getSelectedClip()) { vs.edit().getUndoManager().beginNewTransaction ("Copy to slot"); vs.session.copyToSlot (*c, *tp, sIdx); } }
            else if (r == 8) recordIntoSlot (*tp, sIdx);
            repaint();
        });
    }

    void clipMenu (te::Clip& c)
    {
        juce::PopupMenu m, q, fa, after;
        m.addItem (1, "Launch"); m.addItem (2, "Stop");
        m.addSeparator();
        m.addItem (3, "Rename..."); m.addItem (4, "Duplicate to the next empty slot"); m.addItem (5, "Copy to the arrangement at the playhead");
        m.addItem (6, "Loop", true, c.isLooping());
        // launch quantise
        static const te::LaunchQType qs[] { te::LaunchQType::none, te::LaunchQType::eightBars, te::LaunchQType::fourBars, te::LaunchQType::twoBars, te::LaunchQType::bar,
                                           te::LaunchQType::half, te::LaunchQType::quarter, te::LaunchQType::eighth, te::LaunchQType::sixteenth };
        static const char* qn[] { "None", "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" };
        const bool global = c.usesGlobalLaunchQuatisation();
        q.addItem (100, "Global (top bar)", true, global);
        for (int i = 0; i < 9; ++i) q.addItem (101 + i, qn[i], true, ! global && c.getLaunchQuantisation() != nullptr && c.getLaunchQuantisation()->type.get() == qs[i]);
        m.addSubMenu ("Launch quantise", q);
        // follow action
        static const te::FollowAction fas[] { te::FollowAction::none, te::FollowAction::globalPlayAgain, te::FollowAction::trackNext, te::FollowAction::trackPrevious,
                                             te::FollowAction::trackFirst, te::FollowAction::trackLast, te::FollowAction::trackAny, te::FollowAction::trackOther,
                                             te::FollowAction::trackRoundRobin, te::FollowAction::globalStop, te::FollowAction::globalReturnToArrangement };
        static const char* fan[] { "None (keep looping)", "Play again", "Next clip", "Previous clip", "First clip", "Last clip", "Any clip", "Other clip", "Round robin", "Stop", "Back to arrangement" };
        te::FollowAction current = te::FollowAction::none;
        if (auto* f = c.getFollowActions(); f != nullptr && ! f->getActions().empty()) current = f->getActions().front()->action.get();
        for (int i = 0; i < 11; ++i) fa.addItem (200 + i, fan[i], c.getFollowActions() != nullptr, current == fas[i]);
        fa.addSeparator();
        for (int n : { 1, 2, 4, 8, 16 }) after.addItem (300 + n, juce::String (n) + (n == 1 ? " loop" : " loops"), true, juce::roundToInt (c.followActionNumLoops.get()) == n);
        fa.addSubMenu ("After", after);
        m.addSubMenu ("Follow action", fa);
        m.addSeparator();
        m.addItem (9, "Delete");
        m.showMenuAsync ({}, [this, cp = te::Clip::Ptr (&c)] (int r)
        {
            if (cp == nullptr || r == 0) return;
            auto& c2 = *cp;
            auto* um = &vs.edit().getUndoManager();
            if (r == 1) vs.session.launchClip (c2);
            else if (r == 2) { if (auto lh = c2.getLaunchHandle()) lh->stop (std::nullopt); }
            else if (r == 3)
            {
                auto* w = new juce::AlertWindow ("Rename clip", {}, juce::MessageBoxIconType::NoIcon);
                w->addTextEditor ("name", c2.getName()); w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0);
                w->enterModalState (true, juce::ModalCallbackFunction::create ([w, cp] (int res) { if (res == 1 && cp != nullptr) cp->setName (w->getTextEditorContents ("name")); }), true);
            }
            else if (r == 4)
            {
                if (auto* s = c2.getClipSlot())
                    if (auto* t = dynamic_cast<te::AudioTrack*> (&s->track))
                    {
                        int n = s->getIndex() + 1;
                        while (DawSession::slotClip (*t, n) != nullptr) ++n;
                        vs.session.ensureScenes (n + 1);
                        um->beginNewTransaction ("Duplicate slot");
                        vs.session.copyToSlot (c2, *t, n);
                    }
            }
            else if (r == 5) { um->beginNewTransaction ("Copy to arrangement"); vs.session.copyToArrangement (c2, vs.edit().getTransport().getPosition()); }
            else if (r == 6)
            {
                um->beginNewTransaction ("Loop");
                if (c2.isLooping()) c2.disableLooping(); else c2.setLoopRangeBeats ({ te::BeatPosition(), c2.getLengthInBeats() });
            }
            else if (r == 9) { um->beginNewTransaction ("Delete slot clip"); if (auto lh = c2.getLaunchHandle()) lh->stop (std::nullopt); c2.removeFromParent(); vs.selectClip (nullptr); }
            else if (r == 100) c2.setUsesGlobalLaunchQuatisation (true);
            else if (r > 100 && r < 110)
            {
                c2.setUsesGlobalLaunchQuatisation (false);
                if (auto* lq = c2.getLaunchQuantisation()) lq->type = qs[r - 101];
            }
            else if (r >= 200 && r < 211)
            {
                if (auto* f = c2.getFollowActions())
                {
                    um->beginNewTransaction ("Follow action");
                    while (! f->getActions().empty()) f->removeAction (*f->getActions().front());
                    if (fas[r - 200] != te::FollowAction::none)
                    {
                        auto& a = f->addAction();
                        a.action = fas[r - 200]; a.weight = 1.0;
                        c2.followActionDurationType = te::Clip::FollowActionDurationType::loops;
                        if (c2.followActionNumLoops.get() < 1.0) c2.followActionNumLoops = 1.0;
                    }
                }
            }
            else if (r > 300 && r <= 316)
            {
                c2.followActionDurationType = te::Clip::FollowActionDurationType::loops;
                c2.followActionNumLoops = (double) (r - 300);
            }
            repaint();
        });
    }

    void sceneMenu (int sIdx)
    {
        juce::PopupMenu m;
        m.addItem (1, "Launch scene"); m.addItem (2, "Rename...");
        m.addItem (3, "Insert scene above"); m.addItem (4, "Insert scene below");
        m.addItem (5, "Delete scene (and its clips)", vs.session.numScenes() > 1);
        m.showMenuAsync ({}, [this, sIdx] (int r)
        {
            auto* um = &vs.edit().getUndoManager();
            if (r == 1) vs.session.launchScene (sIdx);
            else if (r == 2)
            {
                auto* sc = vs.edit().getSceneList().getScenes()[sIdx];
                if (sc == nullptr) return;
                auto* w = new juce::AlertWindow ("Rename scene", {}, juce::MessageBoxIconType::NoIcon);
                w->addTextEditor ("name", sc->name.get()); w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0);
                w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, sIdx] (int res)
                {
                    if (res == 1) if (auto* s2 = vs.edit().getSceneList().getScenes()[sIdx]) s2->name = w->getTextEditorContents ("name");
                    repaint();
                }), true);
            }
            else if (r == 3 || r == 4) { um->beginNewTransaction ("Insert scene"); vs.session.insertScene (sIdx + (r == 4 ? 1 : 0)); }
            else if (r == 5) { um->beginNewTransaction ("Delete scene"); vs.session.deleteScene (sIdx); }
            repaint();
        });
    }

    // ---------------------------------------------------------------- recording into a slot
    struct Recording { te::EditItemID track; int scene = -1; bool wasArmed = false; juce::Array<te::InputDeviceInstance*> inputs; bool started = false; };
    Recording recording;
    void recordIntoSlot (te::AudioTrack& t, int sIdx)
    {
        auto& ed = vs.edit();
        if (ed.getTransport().isRecording()) return;
        vs.session.ensureScenes (sIdx + 1);
        auto* s = DawSession::slot (t, sIdx);
        if (s == nullptr) return;
        if (auto* old = s->getClip()) old->removeFromParent();
        const bool wasArmed = dawui::isArmed (t);
        if (! wasArmed) dawui::toggleArm (t);      // gives the track an input if it has none
        recording = { t.itemID, sIdx, wasArmed, {}, false };
        for (auto* in : ed.getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, 0))
            {
                in->setRecordingEnabled (t.itemID, false);
                if (in->setTarget (s->itemID, true, nullptr, std::nullopt)) { in->setRecordingEnabled (s->itemID, true); recording.inputs.add (in); }
            }
        if (recording.inputs.isEmpty())
        {
            recording = {};
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Record into slot", "This track has no input - choose one with IN in the arrangement.");
            return;
        }
        ed.getTransport().record (false);
        recording.started = true;
    }
    void finishSlotRecording()
    {
        auto& ed = vs.edit();
        te::AudioTrack* t = nullptr;
        for (auto* at : te::getAudioTracks (ed)) if (at->itemID == recording.track) t = at;
        auto* s = t != nullptr ? DawSession::slot (*t, recording.scene) : nullptr;
        for (auto* in : recording.inputs)
        {
            if (s != nullptr) in->setRecordingEnabled (s->itemID, false);
            if (t != nullptr) { [[maybe_unused]] auto res = in->setTarget (t->itemID, true, nullptr, 0); in->setRecordingEnabled (t->itemID, recording.wasArmed); }
        }
        if (s != nullptr)
            if (auto* c = s->getClip())
            {
                te::prepareClipForLauncher (*c);
                vs.selectClip (c);
                vs.session.launchClip (*c);
            }
        recording = {};
    }

    void refreshQuant()
    {
        static const te::LaunchQType q[] { te::LaunchQType::none, te::LaunchQType::eightBars, te::LaunchQType::fourBars, te::LaunchQType::twoBars, te::LaunchQType::bar,
                                          te::LaunchQType::half, te::LaunchQType::quarter, te::LaunchQType::eighth, te::LaunchQType::sixteenth };
        const auto cur = vs.edit().getLaunchQuantisation().type.get();
        for (int i = 0; i < 9; ++i) if (q[i] == cur) quant.setSelectedItemIndex (i, juce::dontSendNotification);
    }
    void timerCallback() override
    {
        ++blink;
        if (recording.started && ! vs.edit().getTransport().isRecording()) finishSlotRecording();
        bool anySlots = false;
        for (auto* t : vs.session.getTracks (false)) if (t->playSlotClips.get()) anySlots = true;
        backBtn.setToggleState (anySlots, juce::dontSendNotification);
        if (isVisible()) repaint();
    }

    ViewState& vs;
    juce::ComboBox quant; juce::TextButton stopAll, backBtn, addScene;
    int scrollX = 0, scrollY = 0, blink = 0;
    std::pair<int, int> dragFrom { -1, -1 }; bool dragging = false;
    std::unique_ptr<juce::FileChooser> chooser;
};
