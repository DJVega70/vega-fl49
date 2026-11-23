#pragma once
#include "DawUI.h"
#include "StepEditor.h"

/** Note editor for the selected MIDI clip (bottom panel) */
class PianoRoll : public juce::Component, private juce::Timer
{
public:
    explicit PianoRoll (ViewState&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void()> onClose, onDetach;
    void setDetached (bool d) { detachBtn.setButtonText (d ? "DOCK" : "DETACH"); detachBtn.setTooltip (d ? "Put the piano roll back under the arrangement" : "Float the piano roll in its own window, like FL Studio"); }
private:
    juce::TextButton detachBtn;
    void timerCallback() override
    {   // a step clip is edited in the step sequencer instead
        const bool step = dynamic_cast<te::StepClip*> (vs.getSelectedClip()) != nullptr;
        if (stepEditor.isVisible() != step) { stepEditor.setVisible (step); if (step) { stepEditor.toFront (false); detachBtn.toFront (false); } }
        if (! step) repaint();
    }
    te::MidiClip* clip();
    juce::Rectangle<int> gridArea() const { return { keysW, toolbarH, getWidth() - keysW, getHeight() - toolbarH - velH }; }
    juce::Rectangle<int> velArea() const { return { keysW, getHeight() - velH, getWidth() - keysW, velH }; }
    float beatToX (double b) const { return (float) (keysW + (b - scroll) * ppb); }
    double xToBeat (float x) const { return scroll + (x - keysW) / ppb; }
    float noteToY (int n) const { return (float) (toolbarH + (topNote - n) * rowH); }
    int yToNote (float y) const { return topNote - (int) std::floor ((y - toolbarH) / rowH); }
    te::MidiNote* noteAt (juce::Point<float>, bool& onRightEdge);
    double snap (double b, bool bypass) const;
    void audition (int note, int vel);
    ViewState& vs;
    static constexpr int keysW = 64, toolbarH = 32, velH = 70;
    float rowH = 12.0f; int topNote = 84; double scroll = 0, ppb = 80;
    enum class Mode { none, move, resize, create, velocity } mode = Mode::none;
    te::MidiNote* dragNote = nullptr; double grabBeat = 0, origStart = 0, origLen = 0; int grabNote = 0, origNote = 0;
    double lastLen = 0.25;
    juce::ComboBox gridBox, laneBox; juce::TextButton quantBtn, closeBtn, clearBtn; juce::Label title;
    // the bottom lane: velocity, or a controller (CC / pitch bend / aftertouch)
    int laneType() const;                                    // -1 = velocity
    static juce::String laneName (int type);
    float ccToY (int value) const;
    int yToCc (float y) const;
    void drawControllerLane (juce::Graphics&, te::MidiClip&, juce::Colour);
    void paintController (te::MidiClip&, const juce::MouseEvent&, bool erase);
    double lastCcBeat = -1.0;
    StepEditor stepEditor { vs };     // declared after vs
    te::EditItemID lastClip;
};
