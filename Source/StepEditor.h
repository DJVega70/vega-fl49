#pragma once
#include "DawUI.h"

/** Drum-machine style step sequencer for a step clip (bottom panel, in place of the piano roll).
    Rows are drum sounds (General MIDI notes, so they play Vega Productions Drums directly), columns are steps. */
class StepEditor : public juce::Component, private juce::Timer
{
public:
    explicit StepEditor (ViewState& v) : vs (v)
    {
        stepsBox.addItemList ({ "8 steps", "16 steps", "32 steps", "64 steps" }, 1);
        stepsBox.onChange = [this]
        {
            if (auto* c = clip())
            {
                static const int n[] { 8, 16, 32, 64 };
                vs.edit().getUndoManager().beginNewTransaction ("Steps");
                c->getPattern (0).setNumNotes (n[juce::jlimit (0, 3, stepsBox.getSelectedItemIndex())]);
            }
        };
        rateBox.addItemList ({ "1/8 notes", "1/16 notes", "1/32 notes", "1/16 triplets" }, 1);
        rateBox.onChange = [this]
        {
            if (auto* c = clip())
            {
                static const double len[] { 0.5, 0.25, 0.125, 1.0 / 6.0 };
                vs.edit().getUndoManager().beginNewTransaction ("Step length");
                c->getPattern (0).setNoteLength (te::BeatDuration::fromBeats (len[juce::jlimit (0, 3, rateBox.getSelectedItemIndex())]));
            }
        };
        for (auto* b : { &clearBtn, &randomBtn, &addRowBtn, &closeBtn }) addAndMakeVisible (b);
        clearBtn.setButtonText ("CLEAR"); randomBtn.setButtonText ("RANDOM"); addRowBtn.setButtonText ("+ ROW"); closeBtn.setButtonText ("CLOSE");
        clearBtn.onClick = [this] { if (auto* c = clip()) { vs.edit().getUndoManager().beginNewTransaction ("Clear steps"); c->getPattern (0).clear(); } };
        randomBtn.onClick = [this] { if (auto* c = clip()) { vs.edit().getUndoManager().beginNewTransaction ("Random steps"); c->getPattern (0).randomiseSteps(); } };
        addRowBtn.onClick = [this] { if (auto* c = clip()) { c->insertNewChannel (c->getChannels().size()); if (auto* ch = c->getChannels().getLast()) { ch->noteNumber = 37; ch->name = juce::MidiMessage::getRhythmInstrumentName (37); } } };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (stepsBox); addAndMakeVisible (rateBox);
        title.setFont (VegaFonts::ui (12.0f, true)); title.setColour (juce::Label::textColourId, VegaColours::accent);
        addAndMakeVisible (title);
        startTimerHz (25);
    }
    std::function<void()> onClose;
    te::StepClip* clip() { return dynamic_cast<te::StepClip*> (vs.getSelectedClip()); }

    void resized() override
    {
        title.setBounds (8, 4, 340, 24);
        closeBtn.setBounds (getWidth() - 76, 5, 70, 22);
        addRowBtn.setBounds (getWidth() - 146, 5, 66, 22);
        randomBtn.setBounds (getWidth() - 226, 5, 76, 22);
        clearBtn.setBounds (getWidth() - 294, 5, 64, 22);
        rateBox.setBounds (getWidth() - 420, 5, 122, 22);
        stepsBox.setBounds (getWidth() - 520, 5, 96, 22);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff161616));
        g.setColour (juce::Colour (0xff1e1e1e)); g.fillRect (0, 0, getWidth(), toolbarH);
        auto* c = clip();
        if (c == nullptr) return;
        auto pattern = c->getPattern (0);
        const int steps = pattern.getNumNotes(), rows = c->getChannels().size();
        title.setText ("STEP SEQUENCER  -  " + c->getName() + "  (" + (c->getTrack() ? c->getTrack()->getName() : juce::String()) + ")", juce::dontSendNotification);
        const auto col = dawui::clipColour (*c);
        const auto grid = gridArea();
        const float cw = grid.getWidth() / (float) std::max (1, steps), rh = std::min (28.0f, grid.getHeight() / (float) std::max (1, rows));
        const int playing = playingStep (*c, steps);
        for (int r = 0; r < rows; ++r)
        {
            auto* ch = c->getChannels()[r];
            const float y = (float) grid.getY() + r * rh;
            g.setColour (juce::Colour (r % 2 ? 0xff1b1b1b : 0xff1f1f1f)); g.fillRect (0.0f, y, (float) getWidth(), rh);
            g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (11.0f, true));
            g.drawText (ch->getDisplayName(), 8, (int) y, namesW - 40, (int) rh, juce::Justification::centredLeft);
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.0f));
            g.drawText (juce::MidiMessage::getMidiNoteName (ch->noteNumber, true, true, 3), namesW - 38, (int) y, 32, (int) rh, juce::Justification::centredRight);
            for (int s = 0; s < steps; ++s)
            {
                auto cell = juce::Rectangle<float> (grid.getX() + s * cw, y, cw, rh).reduced (1.5f);
                const bool on = pattern.getNote (r, s);
                const bool beat = s % 4 == 0;
                g.setColour (on ? col.withAlpha (0.35f + 0.65f * pattern.getVelocity (r, s) / 127.0f)
                                : juce::Colour (beat ? 0xff2c2c2c : 0xff252525));
                g.fillRoundedRectangle (cell, 2.5f);
                if (s == playing) { g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.12f)); g.drawRoundedRectangle (cell, 2.5f, 1.5f); }
            }
        }
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f));
        g.drawText ("Click / drag to set steps. Shift-click for a soft hit. Right-click a sound's name to change its note or remove it.",
                    namesW, getHeight() - 18, getWidth() - namesW - 8, 16, juce::Justification::centredLeft);
        g.setColour (VegaColours::line); g.drawHorizontalLine (0, 0, (float) getWidth());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* c = clip(); if (c == nullptr) return;
        int r = 0, s = 0;
        if (e.x < namesW && rowAt (e.position, r))
        {
            if (e.mods.isPopupMenu()) rowMenu (r);
            else audition (c->getChannels()[r]->noteNumber, 100);
            return;
        }
        if (! cellAt (e.position, r, s)) return;
        vs.edit().getUndoManager().beginNewTransaction ("Steps");
        auto pattern = c->getPattern (0);
        paintValue = ! pattern.getNote (r, s);
        setCell (*c, r, s, e.mods.isShiftDown());
        if (paintValue) audition (c->getChannels()[r]->noteNumber, e.mods.isShiftDown() ? 60 : 100);
        lastR = r; lastS = s;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* c = clip(); if (c == nullptr) return;
        int r = 0, s = 0;
        if (cellAt (e.position, r, s) && (r != lastR || s != lastS)) { setCell (*c, r, s, e.mods.isShiftDown()); lastR = r; lastS = s; }
    }

private:
    static constexpr int toolbarH = 32, namesW = 150;
    juce::Rectangle<int> gridArea() const { return { namesW, toolbarH + 4, getWidth() - namesW - 8, getHeight() - toolbarH - 26 }; }
    bool rowAt (juce::Point<float> p, int& r)
    {
        auto* c = clip(); if (c == nullptr) return false;
        const auto grid = gridArea();
        const int rows = c->getChannels().size();
        const float rh = std::min (28.0f, grid.getHeight() / (float) std::max (1, rows));
        r = (int) ((p.y - grid.getY()) / rh);
        return p.y >= grid.getY() && juce::isPositiveAndBelow (r, rows);
    }
    bool cellAt (juce::Point<float> p, int& r, int& s)
    {
        auto* c = clip(); if (c == nullptr || ! rowAt (p, r)) return false;
        const auto grid = gridArea();
        const int steps = c->getPattern (0).getNumNotes();
        s = (int) ((p.x - grid.getX()) / (grid.getWidth() / (float) std::max (1, steps)));
        return p.x >= grid.getX() && juce::isPositiveAndBelow (s, steps);
    }
    void setCell (te::StepClip& c, int r, int s, bool soft)
    {
        auto pattern = c.getPattern (0);
        pattern.setNote (r, s, paintValue);
        if (paintValue) pattern.setVelocity (r, s, soft ? 60 : 110);
    }
    int playingStep (te::StepClip& c, int steps)
    {
        auto& tr = vs.edit().getTransport();
        if (! tr.isPlaying() || c.getClipSlot() != nullptr) return -1;
        const auto pos = c.getPosition();
        if (! pos.time.contains (tr.getPosition())) return -1;
        const double beats = vs.timeToBeats (tr.getPosition()) - vs.timeToBeats (pos.getStart()) + c.getOffsetInBeats().inBeats();
        const double len = c.getPattern (0).getNoteLength().inBeats();
        return len > 0 ? (int) std::floor (beats / len) % std::max (1, steps) : -1;
    }
    void audition (int note, int vel)
    {
        if (auto* c = clip())
            if (auto* t = dynamic_cast<te::AudioTrack*> (c->getTrack()))
            {
                t->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), {});
                juce::Timer::callAfterDelay (200, [tp = te::AudioTrack::Ptr (t), note] { tp->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, note), {}); });
            }
    }
    void rowMenu (int r)
    {
        juce::PopupMenu m, drums, notes;
        for (int n = 35; n <= 81; ++n) drums.addItem (1000 + n, juce::String (n) + "  " + juce::MidiMessage::getRhythmInstrumentName (n));
        for (int n = 24; n <= 96; ++n) notes.addItem (1000 + n + 200, juce::MidiMessage::getMidiNoteName (n, true, true, 3));
        m.addSubMenu ("Drum sound (General MIDI)", drums);
        m.addSubMenu ("Note", notes);
        m.addSeparator();
        m.addItem (1, "Clear this row");
        m.addItem (2, "Every 2nd step"); m.addItem (3, "Every 4th step");
        m.addItem (4, "Remove this row");
        m.showMenuAsync ({}, [this, r] (int res)
        {
            auto* c = clip(); if (c == nullptr) return;
            auto* ch = c->getChannels()[r]; if (ch == nullptr) return;
            vs.edit().getUndoManager().beginNewTransaction ("Step row");
            if (res >= 1200) { ch->noteNumber = res - 1200; ch->name = juce::MidiMessage::getMidiNoteName (res - 1200, true, true, 3); }
            else if (res >= 1000) { ch->noteNumber = res - 1000; ch->name = juce::MidiMessage::getRhythmInstrumentName (res - 1000); }
            else if (res == 1) c->getPattern (0).clearChannel (r);
            else if (res == 2 || res == 3) { c->getPattern (0).clearChannel (r); c->getPattern (0).toggleAtInterval (r, res == 2 ? 2 : 4); }
            else if (res == 4 && c->getChannels().size() > 1) c->removeChannel (r);
            repaint();
        });
    }
    void timerCallback() override
    {
        if (auto* c = clip())
        {
            if (c->itemID != shown)
            {
                shown = c->itemID;
                const int n = c->getPattern (0).getNumNotes();
                stepsBox.setSelectedItemIndex (n <= 8 ? 0 : n <= 16 ? 1 : n <= 32 ? 2 : 3, juce::dontSendNotification);
                const double len = c->getPattern (0).getNoteLength().inBeats();
                rateBox.setSelectedItemIndex (len >= 0.49 ? 0 : len >= 0.24 ? 1 : len >= 0.16 ? 3 : 2, juce::dontSendNotification);
            }
        }
        repaint();
    }
    ViewState& vs;
    juce::ComboBox stepsBox, rateBox; juce::TextButton clearBtn, randomBtn, addRowBtn, closeBtn; juce::Label title;
    te::EditItemID shown; bool paintValue = true; int lastR = -1, lastS = -1;
};
