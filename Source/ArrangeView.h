#pragma once
#include "DawUI.h"

/** Timeline: ruler, track headers and clip lanes */
class ArrangeView : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget, private juce::Timer, private juce::ValueTree::Listener, private juce::ScrollBar::Listener
{
public:
    explicit ArrangeView (ViewState&);
    ~ArrangeView() override;
    void editChanged();                // the session's Edit object was replaced
    void rebuildHeaders();
    void clearAll();      // drop every reference into the current Edit (call before it's destroyed)
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int x, int y) override;
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("vegaplugin:"); }
    void itemDropped (const SourceDetails& d) override;
    void deleteSelectedClip();
    void duplicateSelectedClip();
    void splitSelectedAtPlayhead();
    static constexpr int headerW = 236, rulerH = 30;
    class Lanes; class Ruler; class TrackHeader;
private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;
    int tick = 0;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { triggerRebuild(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerRebuild(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { triggerRebuild(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { needsRepaint = true; }
    void triggerRebuild() { needsRebuild = true; }
    ViewState& vs;
    std::unique_ptr<Ruler> ruler; std::unique_ptr<Lanes> lanes;
    juce::OwnedArray<TrackHeader> headers;
    juce::ScrollBar hbar { false }, vbar { true };
    juce::ValueTree watched;
    bool needsRebuild = false, needsRepaint = false;
    friend class Lanes; friend class Ruler;
public:
    static constexpr int autoLaneH = 76, takeLaneH = 34;
    int clipHeight (te::Track& t) const { return (int) t.state.getProperty ("vegaHeight", 84); }
    int laneHeight (te::Track& t) const { return (bool) t.state.getProperty ("vegaAutoShown", false) ? autoLaneH : 0; }
    static int maxTakes (te::Track&);                                                   // most takes on any clip of the track
    int takesHeight (te::Track& t) const { return (bool) t.state.getProperty ("vegaTakesShown", false) ? maxTakes (t) * takeLaneH : 0; }
    int trackHeight (te::Track& t) const { return clipHeight (t) + takesHeight (t) + laneHeight (t); }   // clips + take lanes + automation lane
    juce::Array<te::AudioTrack*> visibleTracks() const;                                 // tracks not hidden in a collapsed folder
    std::function<void (te::AudioTrack&, bool bounce)> onRenderTrack;                   // bounce in place / consolidate (needs the loop range UI)
    int totalTracksHeight();
    te::AudioTrack* trackAtY (int yInLanes, int* rowTop = nullptr);
    int trackTop (te::AudioTrack&);
};
