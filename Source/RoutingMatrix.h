#pragma once
#include "DawUI.h"

/** Routing matrix: every track / bus down the side; across the top, OUTPUT (Master + each bus, one per row)
    and SENDS (one column per bus, any number per row). Click a cell to route / add / remove. */
class RoutingMatrix : public juce::Component, private juce::Timer
{
public:
    explicit RoutingMatrix (DawSession& s) : session (s) { setSize (760, 460); startTimerHz (5); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1b1d));
        const auto rows = session.getTracks (true);
        const auto buses = session.getBuses();
        const int nOut = 1 + buses.size(), nSend = buses.size();
        layout (rows.size(), nOut, nSend);
        // headers
        g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (11.0f, true));
        g.drawText ("OUTPUT  (one per track)", outX, 6, nOut * cell, 14, juce::Justification::centred);
        if (nSend > 0) g.drawText ("SENDS  (any number)", sendX, 6, std::max (nSend * cell, 150), 14, juce::Justification::centredLeft);
        g.setColour (VegaColours::line); g.drawVerticalLine (sendX - 6, 6.0f, (float) getHeight() - 6);
        auto colHeader = [&g, this] (int x, const juce::String& name, juce::Colour c)
        {
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, (float) x + cell * 0.5f, (float) top - 6));
            g.setColour (c); g.setFont (VegaFonts::ui (10.5f, true));
            g.drawText (name, x + cell / 2 - 2, top - 14, top - 30, 16, juce::Justification::centredLeft);
            g.restoreState();
        };
        colHeader (outX, "Master", VegaColours::gold);
        for (int i = 0; i < buses.size(); ++i)
        {
            colHeader (outX + (i + 1) * cell, buses[i]->getName(), DawSession::trackColour (*buses[i]));
            colHeader (sendX + i * cell, buses[i]->getName(), DawSession::trackColour (*buses[i]));
        }
        // rows
        for (int r = 0; r < rows.size(); ++r)
        {
            auto* t = rows[r];
            const int y = top + r * rowH;
            if (r % 2 == 0) { g.setColour (juce::Colour (0xff202023)); g.fillRect (6, y, getWidth() - 12, rowH); }
            g.setColour (DawSession::trackColour (*t)); g.fillRect (8, y + 4, 4, rowH - 8);
            g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (12.0f, true));
            g.drawText (t->getName(), 18, y, nameW - 24, rowH, juce::Justification::centredLeft, true);
            auto* out = session.getOutputBus (*t);
            for (int c = 0; c < nOut; ++c)
            {
                auto* dest = c == 0 ? nullptr : buses[c - 1];
                drawCell (g, outX + c * cell, y, dest == t ? -1 : (out == dest ? 1 : 0), true);
            }
            for (int c = 0; c < nSend; ++c)
            {
                auto* bus = buses[c];
                auto* sp = bus == t ? nullptr : session.findSend (*t, *bus);
                drawCell (g, sendX + c * cell, y, bus == t ? -1 : (sp != nullptr ? 1 : 0), false);
                if (sp != nullptr)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.setFont (VegaFonts::ui (8.5f, true));
                    g.drawText (juce::String (juce::roundToInt (sp->getGainDb())), sendX + c * cell, y, cell, rowH, juce::Justification::centred);
                }
            }
        }
        if (nSend == 0)
        {
            g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f));
            g.drawText ("Add a bus (+ BUS) to route tracks into groups or set up sends.", sendX, top, getWidth() - sendX - 10, 40, juce::Justification::centredLeft);
        }
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f));
        g.drawText ("Click to route. Sends: click to add (-6 dB) / remove, drag up / down on a send to change its level.", 10, getHeight() - 20, getWidth() - 20, 14, juce::Justification::centredLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto hit = hitCell (e.getPosition());
        if (hit.track == nullptr) return;
        dragSend = nullptr;
        if (hit.output)
        {
            if (hit.bus != hit.track) session.routeToBus (*hit.track, hit.bus);
        }
        else if (hit.bus != nullptr && hit.bus != hit.track)
        {
            if (auto* sp = session.findSend (*hit.track, *hit.bus)) { dragSend = sp; dragStartDb = sp->getGainDb(); dragged = false; }
            else session.setSend (*hit.track, *hit.bus, -6.0f);
        }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragSend == nullptr || e.getDistanceFromDragStart() < 3) return;
        dragged = true;
        dragSend->setGainDb (juce::jlimit (-60.0f, 6.0f, dragStartDb - e.getDistanceFromDragStartY() * 0.25f));
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragSend != nullptr && ! dragged)
        {
            const auto hit = hitCell (e.getPosition());
            if (hit.track != nullptr && hit.bus != nullptr) session.removeSend (*hit.track, *hit.bus);
        }
        dragSend = nullptr; repaint();
    }
private:
    struct Hit { te::AudioTrack* track = nullptr; te::AudioTrack* bus = nullptr; bool output = false; };
    void layout (int nRows, int nOut, int nSend)
    {
        juce::ignoreUnused (nRows, nOut, nSend);
        outX = nameW; sendX = outX + nOut * cell + 14;
    }
    Hit hitCell (juce::Point<int> p)
    {
        const auto rows = session.getTracks (true);
        const auto buses = session.getBuses();
        const int r = (p.y - top) / rowH;
        if (p.y < top || r >= rows.size()) return {};
        Hit h; h.track = rows[r];
        if (p.x >= outX && p.x < outX + (1 + buses.size()) * cell) { const int c = (p.x - outX) / cell; h.output = true; h.bus = c == 0 ? nullptr : buses[c - 1]; return h; }
        if (p.x >= sendX && p.x < sendX + buses.size() * cell) { h.bus = buses[(p.x - sendX) / cell]; return h; }
        return {};
    }
    void drawCell (juce::Graphics& g, int x, int y, int state, bool round)
    {
        auto r = juce::Rectangle<float> ((float) x + 4, (float) y + 4, (float) cell - 8, (float) rowH - 8);
        if (state < 0) { g.setColour (juce::Colour (0xff151517)); g.fillRoundedRectangle (r, 3.0f); return; }
        g.setColour (juce::Colour (0xff2a2b2f)); g.fillRoundedRectangle (r, round ? r.getHeight() * 0.5f : 3.0f);
        if (state > 0)
        {
            const auto c = round ? VegaColours::accent : VegaColours::gold;
            g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (r.expanded (2.0f), 5.0f);
            g.setColour (c); g.fillRoundedRectangle (r.reduced (2.0f), round ? r.getHeight() * 0.5f : 3.0f);
        }
        g.setColour (juce::Colour (0xff3e3f44)); g.drawRoundedRectangle (r, round ? r.getHeight() * 0.5f : 3.0f, 1.0f);
    }
    void timerCallback() override
    {
        const int n = session.getTracks (true).size(), nb = session.getBuses().size();
        const int w = std::max (560, nameW + (1 + nb) * cell + 14 + nb * cell + 30), h = top + n * rowH + 40;
        if (w != getWidth() || h != getHeight()) setSize (w, std::min (h, 900));
        repaint();
    }
    DawSession& session;
    static constexpr int nameW = 170, cell = 44, rowH = 30, top = 110;
    int outX = nameW, sendX = nameW;
    te::AuxSendPlugin* dragSend = nullptr; float dragStartDb = 0; bool dragged = false;
};
