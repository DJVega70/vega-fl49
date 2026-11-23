#pragma once
#include "DawUI.h"

/** Console styled after a hardware control surface (Launch Control XL look):
    three knob rows with LED strips (send A / send B / pan), long-throw faders with scale ticks,
    two rows of lit pads (Solo/Arm, Mute/Select), and a left control column with a display,
    Page / Track buttons, Record / Play and Shift / Mode. */
class MasterSection;
class MixerView : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
{
public:
    explicit MixerView (ViewState&);
    ~MixerView() override;
    void rebuild();
    void clearAll();      // call before the Edit is destroyed
    void resized() override;
    void paint (juce::Graphics&) override;
    class Strip; class ControlPanel;
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("vegaplugin:"); }
    void itemDropped (const SourceDetails& d) override;

    // shared console state
    bool shift = false;          // pads: Solo -> Arm, Mute -> Select
    bool showEdit = true;        // Mode: show / hide the instrument + inserts section
    void touched (const juce::String& title, const juce::String& value);
    void pageBy (int dir);
    void scrollStrips (int n);
    void selectNeighbour (int dir);
    void layoutChanged() { resized(); }
    /** DETACH / DOCK: the mixer can float in its own window */
    std::function<void()> onDetach;
    void setDetached (bool d) { detachBtn.setButtonText (d ? "DOCK" : "DETACH"); detachBtn.setTooltip (d ? "Put the mixer back in the main window" : "Float the mixer in its own window (e.g. on a second screen)"); }
private:
    juce::TextButton detachBtn;
    static constexpr int headerH = 30;
    void timerCallback() override;
    ViewState& vs;
    juce::Viewport viewport; juce::Component content;
    juce::OwnedArray<Strip> strips; std::unique_ptr<MasterSection> master;
    std::unique_ptr<ControlPanel> panel;
    juce::String lastLayout; te::Edit* lastEdit = nullptr;
};
