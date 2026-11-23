#include "PianoRoll.h"

PianoRoll::PianoRoll (ViewState& v) : vs (v)
{
    gridBox.addItemList ({ "Grid 1/4", "Grid 1/8", "Grid 1/16", "Grid 1/32", "Grid 1/8T", "Grid 1/16T", "Grid Off" }, 1);
    gridBox.setSelectedId (3, juce::dontSendNotification);
    addAndMakeVisible (gridBox);
    laneBox.addItemList ({ "Velocity", "Mod wheel (CC1)", "Breath (CC2)", "Volume (CC7)", "Pan (CC10)", "Expression (CC11)", "Sustain pedal (CC64)",
                           "Filter cutoff (CC74)", "Resonance (CC71)", "Pitch bend", "Aftertouch" }, 1);
    laneBox.setSelectedId (1, juce::dontSendNotification);
    laneBox.setTooltip ("What the bottom lane edits. Draw controller moves with the mouse, right-click to erase.");
    laneBox.onChange = [this] { repaint(); };
    addAndMakeVisible (laneBox);
    quantBtn.setButtonText ("QUANTISE"); clearBtn.setButtonText ("CLEAR"); closeBtn.setButtonText ("CLOSE");
    quantBtn.onClick = [this]
    {
        if (auto* c = clip())
        {
            auto* um = &vs.edit().getUndoManager(); um->beginNewTransaction ("Quantise");
            for (auto* n : c->getSequence().getNotes()) n->setStartAndLength (te::BeatPosition::fromBeats (std::max (0.0, snap (n->getStartBeat().inBeats(), false))), n->getLengthBeats(), um);
        }
    };
    clearBtn.onClick = [this] { if (auto* c = clip()) { vs.edit().getUndoManager().beginNewTransaction ("Clear notes"); c->getSequence().clear (&vs.edit().getUndoManager()); } };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    for (auto* b : { &quantBtn, &clearBtn, &closeBtn }) addAndMakeVisible (b);
    title.setFont (VegaFonts::ui (12.0f, true)); title.setColour (juce::Label::textColourId, VegaColours::accent);
    addAndMakeVisible (title);
    addChildComponent (stepEditor);
    stepEditor.onClose = [this] { if (onClose) onClose(); };
    setDetached (false);
    detachBtn.onClick = [this] { if (onDetach) onDetach(); };
    addAndMakeVisible (detachBtn);
    setWantsKeyboardFocus (true);
    startTimerHz (25);
}

te::MidiClip* PianoRoll::clip() { return dynamic_cast<te::MidiClip*> (vs.getSelectedClip()); }

double PianoRoll::snap (double b, bool bypass) const
{
    static const double div[] { 1, 2, 4, 8, 3, 6, 0 };
    const double d = div[juce::jlimit (0, 6, gridBox.getSelectedItemIndex())];
    if (d <= 0 || bypass) return b;
    return std::round (b * d) / d;
}

void PianoRoll::resized()
{
    title.setBounds (keysW + 6, 4, 360, 24);
    closeBtn.setBounds (getWidth() - 76, 5, 70, 22); clearBtn.setBounds (getWidth() - 152, 5, 70, 22);
    quantBtn.setBounds (getWidth() - 248, 5, 90, 22); gridBox.setBounds (getWidth() - 360, 5, 106, 22);
    laneBox.setBounds (getWidth() - 520, 5, 152, 22);
    detachBtn.setBounds (getWidth() - 600, 5, 74, 22);
    stepEditor.setBounds (getLocalBounds());
}

void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161616));
    g.setColour (juce::Colour (0xff1e1e1e)); g.fillRect (0, 0, getWidth(), toolbarH);
    auto* c = clip();
    if (c == nullptr)
    {
        title.setText ("PIANO ROLL", juce::dontSendNotification);
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (13.0f));
        g.drawText ("Double-click a MIDI clip (or an empty spot on an instrument track) to edit notes", getLocalBounds(), juce::Justification::centred);
        return;
    }
    if (c->itemID != lastClip)
    {   // new clip: frame it and centre on its notes
        lastClip = c->itemID; scroll = c->getOffsetInBeats().inBeats();
        const double len = c->getLengthInBeats().inBeats();
        ppb = juce::jlimit (12.0, 400.0, (getWidth() - keysW - 20) / std::max (1.0, len));
        int lo = 127, hi = 0; for (auto* n : c->getSequence().getNotes()) { lo = std::min (lo, n->getNoteNumber()); hi = std::max (hi, n->getNoteNumber()); }
        const int visibleRows = (int) ((getHeight() - toolbarH - velH) / rowH);
        topNote = hi >= lo ? juce::jlimit (visibleRows, 127, (lo + hi) / 2 + visibleRows / 2) : 84;
    }
    title.setText ("PIANO ROLL  -  " + c->getName() + "  (" + (c->getTrack() ? c->getTrack()->getName() : juce::String()) + ")", juce::dontSendNotification);
    const auto grid = gridArea();
    const auto col = dawui::clipColour (*c);
    g.saveState(); g.reduceClipRegion (grid.withX (0).withWidth (getWidth()));
    // rows + keys
    for (int n = 127; n >= 0; --n)
    {
        const float y = noteToY (n);
        if (y > grid.getBottom() || y + rowH < grid.getY()) continue;
        const bool black = juce::MidiMessage::isMidiNoteBlack (n);
        g.setColour (juce::Colour (black ? 0xff191919 : 0xff1f1f1f)); g.fillRect ((float) keysW, y, (float) grid.getWidth(), rowH);
        g.setColour (juce::Colour (0xff141414)); g.drawHorizontalLine ((int) (y + rowH), (float) keysW, (float) getWidth());
        g.setColour (black ? juce::Colour (0xff202020) : juce::Colour (0xffd8d8d8)); g.fillRect (0.0f, y, (float) keysW - 2, rowH - 1);
        if (n % 12 == 0) { g.setColour (juce::Colours::black); g.setFont (VegaFonts::ui (9.0f, true)); g.drawText (juce::MidiMessage::getMidiNoteName (n, true, true, 3), 2, (int) y, keysW - 6, (int) rowH, juce::Justification::centredRight); }
    }
    // beat lines + clip range
    const double clipStart = c->getOffsetInBeats().inBeats(), clipEnd = clipStart + c->getLengthInBeats().inBeats();
    const int num = vs.edit().tempoSequence.getTimeSig (0)->numerator;
    for (double b = std::floor (scroll * 4) / 4; b < xToBeat ((float) getWidth()); b += 0.25)
    {
        const bool beat = std::abs (b - std::round (b)) < 1e-6, bar = beat && ((int) std::round (b) % num == 0);
        if (! beat && ppb < 40) continue;
        g.setColour (juce::Colours::white.withAlpha (bar ? 0.14f : beat ? 0.07f : 0.03f));
        g.drawVerticalLine ((int) beatToX (b), (float) grid.getY(), (float) grid.getBottom());
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (juce::Rectangle<float> ((float) keysW, (float) grid.getY(), std::max (0.0f, beatToX (clipStart) - keysW), (float) grid.getHeight()));
    g.fillRect (juce::Rectangle<float> (beatToX (clipEnd), (float) grid.getY(), std::max (0.0f, (float) getWidth() - beatToX (clipEnd)), (float) grid.getHeight()));
    // notes
    for (auto* n : c->getSequence().getNotes())
    {
        auto r = juce::Rectangle<float> (beatToX (n->getStartBeat().inBeats()), noteToY (n->getNoteNumber()) + 1, (float) (n->getLengthBeats().inBeats() * ppb) - 1, rowH - 2);
        g.setColour (col.withAlpha (0.45f + 0.55f * n->getVelocity() / 127.0f)); g.fillRoundedRectangle (r, 2.0f);
        g.setColour (n == dragNote ? juce::Colours::white : col.brighter (0.3f)); g.drawRoundedRectangle (r, 2.0f, 1.0f);
    }
    // playhead (if inside this clip)
    auto& tr = vs.edit().getTransport();
    const double ph = vs.timeToBeats (tr.getPosition()) - vs.timeToBeats (c->getPosition().getStart()) + clipStart;
    if (ph >= clipStart && ph <= clipEnd) { g.setColour (VegaColours::gold); g.fillRect (beatToX (ph), (float) grid.getY(), 1.5f, (float) grid.getHeight()); }
    g.restoreState();
    // velocity lane
    auto va = velArea();
    g.setColour (juce::Colour (0xff121212)); g.fillRect (va);
    g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.5f, true));
    g.drawFittedText (laneName (laneType()).toUpperCase(), 4, va.getY() + 2, keysW - 8, va.getHeight() - 4, juce::Justification::topLeft, 3);
    if (laneType() >= 0) drawControllerLane (g, *c, col);
    else
        for (auto* n : c->getSequence().getNotes())
        {
            const float x = beatToX (n->getStartBeat().inBeats()), h = (va.getHeight() - 8) * n->getVelocity() / 127.0f;
            if (x < keysW) continue;
            g.setColour (col); g.fillRect (x, (float) va.getBottom() - h - 2, 3.0f, h); g.fillEllipse (x - 2, (float) va.getBottom() - h - 5, 7, 7);
        }
    g.setColour (VegaColours::line); g.drawHorizontalLine (va.getY(), 0, (float) getWidth()); g.drawHorizontalLine (0, 0, (float) getWidth());
}

te::MidiNote* PianoRoll::noteAt (juce::Point<float> p, bool& edge)
{
    auto* c = clip(); if (c == nullptr) return nullptr;
    for (auto* n : c->getSequence().getNotes())
    {
        auto r = juce::Rectangle<float> (beatToX (n->getStartBeat().inBeats()), noteToY (n->getNoteNumber()), (float) (n->getLengthBeats().inBeats() * ppb), rowH);
        if (r.contains (p)) { edge = p.x > r.getRight() - std::min (8.0f, r.getWidth() * 0.3f); return n; }
    }
    return nullptr;
}

void PianoRoll::audition (int note, int vel)
{
    if (auto* c = clip())
        if (auto* t = dynamic_cast<te::AudioTrack*> (c->getTrack()))
        {
            t->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), {});
            juce::Timer::callAfterDelay (220, [tp = te::AudioTrack::Ptr (t), note] { tp->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, note), {}); });
        }
}

void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    bool edge = false; auto* n = noteAt (e.position, edge);
    setMouseCursor (n == nullptr ? juce::MouseCursor::NormalCursor : edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    auto* c = clip(); if (c == nullptr) return;
    auto* um = &vs.edit().getUndoManager();
    mode = Mode::none; dragNote = nullptr;
    if (velArea().contains (e.getPosition()))
    {
        mode = Mode::velocity; lastCcBeat = -1.0;
        um->beginNewTransaction (laneType() >= 0 ? "Draw " + laneName (laneType()) : juce::String ("Velocity"));
        mouseDrag (e); return;
    }
    if (! gridArea().contains (e.getPosition()))
    {
        if (e.x < keysW && e.y > toolbarH) audition (yToNote ((float) e.y), 100);
        return;
    }
    bool edge = false;
    if (auto* n = noteAt (e.position, edge))
    {
        if (e.mods.isPopupMenu()) { um->beginNewTransaction ("Delete note"); c->getSequence().removeNote (*n, um); return; }
        um->beginNewTransaction (edge ? "Resize note" : "Move note");
        dragNote = n; mode = edge ? Mode::resize : Mode::move;
        origStart = n->getStartBeat().inBeats(); origLen = n->getLengthBeats().inBeats(); origNote = n->getNoteNumber();
        grabBeat = xToBeat (e.position.x); grabNote = yToNote (e.position.y);
        lastLen = origLen;
        if (! edge) audition (origNote, n->getVelocity());
        return;
    }
    if (e.mods.isPopupMenu()) return;
    // create a note
    um->beginNewTransaction ("Add note");
    static const double divs[] { 1, 2, 4, 8, 3, 6, 0 };
    const double d = divs[juce::jlimit (0, 6, gridBox.getSelectedItemIndex())];
    const double clicked = xToBeat (e.position.x);
    const double st = (d > 0 && ! e.mods.isShiftDown()) ? std::floor (clicked * d) / d : clicked;
    const int note = juce::jlimit (0, 127, yToNote (e.position.y));
    dragNote = c->getSequence().addNote (note, te::BeatPosition::fromBeats (std::max (0.0, st)), te::BeatDuration::fromBeats (lastLen), 100, 0, um);
    if (dragNote) { mode = Mode::resize; origStart = dragNote->getStartBeat().inBeats(); origLen = lastLen; grabBeat = xToBeat (e.position.x); audition (note, 100); }
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    auto* c = clip(); if (c == nullptr) return;
    auto* um = &vs.edit().getUndoManager();
    const bool free = e.mods.isShiftDown();
    if (mode == Mode::velocity && laneType() >= 0) { paintController (*c, e, e.mods.isPopupMenu()); return; }
    if (mode == Mode::velocity)
    {
        const auto va = velArea();
        for (auto* n : c->getSequence().getNotes())
            if (std::abs (beatToX (n->getStartBeat().inBeats()) - e.position.x) < 5)
                n->setVelocity (juce::jlimit (1, 127, (int) ((va.getBottom() - e.position.y) / (va.getHeight() - 8) * 127)), um);
        return;
    }
    if (dragNote == nullptr) return;
    if (mode == Mode::move)
    {
        const double ns = std::max (0.0, snap (origStart + xToBeat (e.position.x) - grabBeat, free));
        const int nn = juce::jlimit (0, 127, origNote + yToNote (e.position.y) - grabNote);
        dragNote->setStartAndLength (te::BeatPosition::fromBeats (ns), te::BeatDuration::fromBeats (origLen), um);
        if (nn != dragNote->getNoteNumber()) { dragNote->setNoteNumber (nn, um); audition (nn, dragNote->getVelocity()); }
    }
    else if (mode == Mode::resize)
    {
        const double end = snap (xToBeat (e.position.x), free);
        const double len = std::max (1.0 / 32, end - origStart);
        dragNote->setStartAndLength (te::BeatPosition::fromBeats (origStart), te::BeatDuration::fromBeats (len), um);
        lastLen = len;
    }
}

void PianoRoll::mouseUp (const juce::MouseEvent&) { mode = Mode::none; dragNote = nullptr; }

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    bool edge = false;
    if (auto* n = noteAt (e.position, edge)) if (auto* c = clip()) { vs.edit().getUndoManager().beginNewTransaction ("Delete note"); c->getSequence().removeNote (*n, &vs.edit().getUndoManager()); }
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double anchor = xToBeat (e.position.x);
        ppb = juce::jlimit (8.0, 800.0, ppb * (w.deltaY > 0 ? 1.15 : 1 / 1.15));
        scroll = anchor - (e.position.x - keysW) / ppb;
    }
    else if (e.mods.isShiftDown()) scroll -= w.deltaY * 200 / ppb;
    else topNote = juce::jlimit (20, 127, topNote + (w.deltaY > 0 ? 3 : -3));
    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey && onClose) { onClose(); return true; }
    return false;
}

//============================================================================== controller lane
int PianoRoll::laneType() const
{
    static const int types[] { -1, 1, 2, 7, 10, 11, 64, 74, 71, te::MidiControllerEvent::pitchWheelType, te::MidiControllerEvent::channelPressureType };
    return types[juce::jlimit (0, 10, laneBox.getSelectedItemIndex())];
}

juce::String PianoRoll::laneName (int type)
{
    if (type < 0) return "Velocity";
    if (type == te::MidiControllerEvent::pitchWheelType) return "Pitch bend";
    if (type == te::MidiControllerEvent::channelPressureType) return "Aftertouch";
    if (type == 64) return "Sustain";
    if (type == 1) return "Mod wheel";
    if (type == 11) return "Expression";
    return "CC " + juce::String (type);
}

float PianoRoll::ccToY (int v) const
{
    const auto va = velArea();
    return (float) va.getBottom() - 3.0f - (va.getHeight() - 8) * juce::jlimit (0, 16383, v) / 16383.0f;
}
int PianoRoll::yToCc (float y) const
{
    const auto va = velArea();
    const int v = (int) std::round (((float) va.getBottom() - 3.0f - y) / (float) (va.getHeight() - 8) * 16383.0f);
    if (laneType() == 64) return v >= 8192 ? 127 << 7 : 0;             // the pedal is on or off
    return juce::jlimit (0, 16383, v);
}

void PianoRoll::drawControllerLane (juce::Graphics& g, te::MidiClip& c, juce::Colour col)
{
    const int type = laneType();
    const auto va = velArea();
    juce::Array<te::MidiControllerEvent*> evs;
    for (auto* e : c.getSequence().getControllerEvents()) if (e->getType() == type) evs.add (e);
    if (type == te::MidiControllerEvent::pitchWheelType)
    {   // centre line
        g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawHorizontalLine ((int) ccToY (8192), (float) keysW, (float) getWidth());
    }
    if (evs.isEmpty())
    {
        g.setColour (VegaColours::textDim.withAlpha (0.7f)); g.setFont (VegaFonts::ui (10.5f));
        g.drawText ("Draw " + laneName (type) + " moves here (right-click to erase)", va.withTrimmedLeft (keysW + 8), juce::Justification::centredLeft);
        return;
    }
    juce::Path p, fill;
    const float x0 = beatToX (evs[0]->getBeatPosition().inBeats());
    p.startNewSubPath (x0, ccToY (evs[0]->getControllerValue()));
    for (int i = 1; i < evs.size(); ++i)
    {   // controller values hold until the next one: draw steps
        const float x = beatToX (evs[i]->getBeatPosition().inBeats());
        p.lineTo (x, ccToY (evs[i - 1]->getControllerValue()));
        p.lineTo (x, ccToY (evs[i]->getControllerValue()));
    }
    p.lineTo ((float) getWidth(), ccToY (evs.getLast()->getControllerValue()));
    fill = p; fill.lineTo ((float) getWidth(), (float) va.getBottom()); fill.lineTo (x0, (float) va.getBottom()); fill.closeSubPath();
    g.saveState(); g.reduceClipRegion (va.withTrimmedLeft (keysW));
    g.setColour (col.withAlpha (0.18f)); g.fillPath (fill);
    g.setColour (col); g.strokePath (p, juce::PathStrokeType (1.5f));
    for (auto* e : evs) { const float x = beatToX (e->getBeatPosition().inBeats()); g.fillEllipse (x - 2.5f, ccToY (e->getControllerValue()) - 2.5f, 5, 5); }
    g.restoreState();
}

void PianoRoll::paintController (te::MidiClip& c, const juce::MouseEvent& e, bool erase)
{
    auto* um = &vs.edit().getUndoManager();
    const int type = laneType();
    static const double divs[] { 1, 2, 4, 8, 3, 6, 0 };
    double step = divs[juce::jlimit (0, 6, gridBox.getSelectedItemIndex())];
    step = step > 0 ? 1.0 / step : 1.0 / 16.0;
    step = std::min (step, 1.0 / 8.0);                       // draw at least 1/32-note detail... at most 1/8 apart
    const double beat = std::max (0.0, std::floor (xToBeat (e.position.x) / step) * step);
    // fill any gap from a fast mouse move, so a drawn line has no holes
    const double from = lastCcBeat >= 0 ? std::min (lastCcBeat, beat) : beat, to = lastCcBeat >= 0 ? std::max (lastCcBeat, beat) : beat;
    for (double b = from; b <= to + 1.0e-6; b += step)
    {
        auto& seq = c.getSequence();
        seq.removeControllersBetween (type, te::BeatPosition::fromBeats (b - 1.0e-6), te::BeatPosition::fromBeats (b + step - 1.0e-6), um);
        if (! erase) seq.addControllerEvent (te::BeatPosition::fromBeats (b), type, yToCc (e.position.y), um);
    }
    lastCcBeat = beat;
}
