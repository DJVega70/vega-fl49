#pragma once
#include "DawUI.h"

/** Markers & tempo list: every marker, tempo change and time-signature change, in song order.
    Jump to any of them, rename markers, edit tempos / signatures, delete them, or add new ones at the playhead. */
class SongMap : public juce::Component, private juce::Timer
{
public:
    explicit SongMap (ViewState& v) : vs (v)
    {
        for (auto* b : { &addMarker, &addTempo, &addSig }) { addAndMakeVisible (b); }
        addMarker.setButtonText ("+ MARKER AT PLAYHEAD"); addTempo.setButtonText ("+ TEMPO CHANGE"); addSig.setButtonText ("+ SIGNATURE CHANGE");
        addMarker.onClick = [this] { vs.edit().getUndoManager().beginNewTransaction ("Add marker"); vs.session.addMarker (vs.edit().getTransport().getPosition()); rebuild(); };
        addTempo.onClick = [this]
        {
            const auto at = vs.edit().getTransport().getPosition();
            vs.edit().getUndoManager().beginNewTransaction ("Tempo change");
            vs.session.addTempoChange (at, vs.edit().tempoSequence.getBpmAt (at));
            rebuild();
        };
        addSig.onClick = [this]
        {
            const auto at = vs.edit().getTransport().getPosition();
            auto& sig = vs.edit().tempoSequence.getTimeSigAt (at);
            vs.edit().getUndoManager().beginNewTransaction ("Time signature change");
            vs.session.addTimeSigChange (at, sig.numerator.get() == 4 ? 3 : 4, 4);
            rebuild();
        };
        view.setViewedComponent (&inner, false); view.setScrollBarsShown (true, false);
        addAndMakeVisible (view);
        setSize (560, 460);
        rebuild();
        startTimerHz (2);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1b1d));
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, true));
        g.drawText ("POSITION", 12, 48, 90, 16, juce::Justification::centredLeft);
        g.drawText ("TYPE", 104, 48, 90, 16, juce::Justification::centredLeft);
        g.drawText ("VALUE", 196, 48, 160, 16, juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto top = getLocalBounds().removeFromTop (44).reduced (10, 8);
        addMarker.setBounds (top.removeFromLeft (180)); top.removeFromLeft (6);
        addTempo.setBounds (top.removeFromLeft (140)); top.removeFromLeft (6);
        addSig.setBounds (top.removeFromLeft (170));
        view.setBounds (getLocalBounds().withTrimmedTop (66));
        layoutRows();
    }

private:
    struct Row : juce::Component
    {
        enum class Kind { marker, tempo, sig } kind = Kind::marker;
        te::TimePosition at; int index = 0; te::EditItemID markerID;
        juce::Label pos, type; juce::TextEditor value; juce::TextButton go, del;
        std::function<void()> onChange;
        Row()
        {
            for (auto* l : { &pos, &type }) { l->setFont (VegaFonts::ui (12.0f, true)); l->setColour (juce::Label::textColourId, VegaColours::text); addAndMakeVisible (l); }
            value.setFont (VegaFonts::ui (12.0f)); addAndMakeVisible (value);
            go.setButtonText ("GO"); del.setButtonText ("DELETE");
            addAndMakeVisible (go); addAndMakeVisible (del);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6, 3);
            pos.setBounds (r.removeFromLeft (88)); type.setBounds (r.removeFromLeft (92));
            del.setBounds (r.removeFromRight (70)); r.removeFromRight (4); go.setBounds (r.removeFromRight (40)); r.removeFromRight (8);
            value.setBounds (r);
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (kind == Kind::marker ? juce::Colour (0xfff2867c) : kind == Kind::tempo ? VegaColours::gold : juce::Colour (0xffb183f2));
            g.fillRect (0, 4, 3, getHeight() - 8);
            g.setColour (VegaColours::line); g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
        }
    };

    void rebuild()
    {
        rows.clear();
        auto& ed = vs.edit();
        auto& ts = ed.tempoSequence;
        for (auto* m : ed.getMarkerManager().getMarkers())
        {
            auto* r = rows.add (new Row());
            r->kind = Row::Kind::marker; r->at = m->getPosition().getStart(); r->markerID = m->itemID;
            r->type.setText ("Marker " + juce::String (m->getMarkerID()), juce::dontSendNotification);
            r->value.setText (m->getName(), false);
            r->value.onReturnKey = r->value.onFocusLost = [this, r]
            {
                for (auto* mm : vs.edit().getMarkerManager().getMarkers()) if (mm->itemID == r->markerID && mm->getName() != r->value.getText()) mm->setName (r->value.getText());
            };
            r->del.onClick = [this, id = m->itemID]
            {
                for (auto* mm : vs.edit().getMarkerManager().getMarkers()) if (mm->itemID == id) mm->removeFromParent();
                juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<SongMap> (this)] { if (sp) sp->rebuild(); });
            };
        }
        for (int i = 0; i < ts.getNumTempos(); ++i)
        {
            auto* t = ts.getTempo (i);
            auto* r = rows.add (new Row());
            r->kind = Row::Kind::tempo; r->at = t->getStartTime(); r->index = i;
            r->type.setText (i == 0 ? "Start tempo" : "Tempo", juce::dontSendNotification);
            r->value.setText (juce::String (t->getBpm(), 2) + " BPM", false);
            r->value.onReturnKey = r->value.onFocusLost = [this, r]
            {
                const double bpm = r->value.getText().getDoubleValue();
                if (auto* tt = vs.edit().tempoSequence.getTempo (r->index); tt != nullptr && bpm >= 20 && bpm <= 400 && std::abs (bpm - tt->getBpm()) > 1.0e-3)
                { vs.edit().getUndoManager().beginNewTransaction ("Tempo"); tt->setBpm (bpm); }
            };
            r->del.setEnabled (i > 0);
            r->del.onClick = [this, i]
            {
                vs.edit().tempoSequence.removeTempo (i, false);
                juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<SongMap> (this)] { if (sp) sp->rebuild(); });
            };
        }
        for (int i = 0; i < ts.getNumTimeSigs(); ++i)
        {
            auto* sg = ts.getTimeSig (i);
            auto* r = rows.add (new Row());
            r->kind = Row::Kind::sig; r->at = ts.toTime (sg->getStartBeat()); r->index = i;
            r->type.setText (i == 0 ? "Start signature" : "Signature", juce::dontSendNotification);
            r->value.setText (sg->getStringTimeSig(), false);
            r->value.onReturnKey = r->value.onFocusLost = [this, r]
            {
                const auto parts = juce::StringArray::fromTokens (r->value.getText(), "/", {});
                if (auto* s2 = vs.edit().tempoSequence.getTimeSig (r->index); s2 != nullptr && parts.size() == 2 && parts[0].getIntValue() > 0)
                    if (s2->getStringTimeSig() != r->value.getText().removeCharacters (" "))
                    { vs.edit().getUndoManager().beginNewTransaction ("Time signature"); s2->numerator = juce::jlimit (1, 16, parts[0].getIntValue()); s2->denominator = parts[1].getIntValue(); }
            };
            r->del.setEnabled (i > 0);
            r->del.onClick = [this, i]
            {
                vs.edit().tempoSequence.removeTimeSig (i);
                juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<SongMap> (this)] { if (sp) sp->rebuild(); });
            };
        }
        std::stable_sort (rows.begin(), rows.end(), [] (Row* a, Row* b) { return a->at < b->at; });
        for (auto* r : rows)
        {
            r->pos.setText (dawui::formatBarsBeats (vs.edit(), r->at), juce::dontSendNotification);
            r->go.onClick = [this, at = r->at] { vs.edit().getTransport().setPosition (at); };
            inner.addAndMakeVisible (r);
        }
        signature = currentSignature();
        layoutRows();
    }
    void layoutRows()
    {
        const int w = std::max (300, view.getWidth() - view.getScrollBarThickness());
        inner.setSize (w, std::max (40, rows.size() * 32));
        for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (0, i * 32, w, 32);
    }
    juce::String currentSignature()
    {
        auto& ed = vs.edit(); auto& ts = ed.tempoSequence;
        juce::String s; s << ed.getMarkerManager().getMarkers().size() << "/" << ts.getNumTempos() << "/" << ts.getNumTimeSigs();
        for (auto* m : ed.getMarkerManager().getMarkers()) s << "," << m->getPosition().getStart().inSeconds();
        return s;
    }
    void timerCallback() override
    {   // something changed elsewhere (ruler, controller): refresh, unless the user is typing
        bool typing = false; for (auto* r : rows) if (r->value.hasKeyboardFocus (false)) typing = true;
        if (! typing && currentSignature() != signature) rebuild();
    }
    ViewState& vs;
    juce::TextButton addMarker, addTempo, addSig;
    juce::Viewport view; juce::Component inner;
    juce::OwnedArray<Row> rows;
    juce::String signature;
};
