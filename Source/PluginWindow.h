#pragma once
// Adapted from Tracktion Engine examples/common/PluginWindow.h (GPLv3)
#include "DawSession.h"
namespace vegawin {
using namespace juce;
inline std::function<std::unique_ptr<te::Plugin::EditorComponent> (te::Plugin&)> makeGenericEditor;

inline bool isDPIAware (te::Plugin&)
{
	// You should keep a DB of if plugins are DPI aware or not and recall that value
	// here. You should let the user toggle the value if the plugin appears tiny
	return true;
}

//==============================================================================
struct AudioProcessorEditorContentComp : public te::Plugin::EditorComponent
{
    AudioProcessorEditorContentComp (te::ExternalPlugin& plug) : plugin (plug)
    {
        JUCE_AUTORELEASEPOOL
        {
            if (auto pi = plugin.getAudioPluginInstance())
            {
                editor.reset (pi->createEditorAndMakeActive());

                if (editor == nullptr)
                    editor = std::make_unique<GenericAudioProcessorEditor> (*pi);

                addAndMakeVisible (*editor);
            }
        }

        resizeToFitEditor (true);
    }

    bool allowWindowResizing() override { return false; }

    ComponentBoundsConstrainer* getBoundsConstrainer() override
    {
        if (editor == nullptr || allowWindowResizing())
            return {};

        return editor->getConstrainer();
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds (getLocalBounds());
    }

    void childBoundsChanged (Component* c) override
    {
        if (c == editor.get())
        {
            plugin.edit.pluginChanged (plugin);
            resizeToFitEditor();
        }
    }

    void resizeToFitEditor (bool force = false)
    {
        if (force || ! allowWindowResizing())
            setSize (jmax (8, editor != nullptr ? editor->getWidth() : 0), jmax (8, editor != nullptr ? editor->getHeight() : 0));
    }

    te::ExternalPlugin& plugin;
    std::unique_ptr<AudioProcessorEditor> editor;

    AudioProcessorEditorContentComp() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProcessorEditorContentComp)
};

//=============================================================================
class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (te::Plugin&);
    ~PluginWindow() override;

    static std::unique_ptr<Component> create (te::Plugin&);

    void show();

    void setEditor (std::unique_ptr<te::Plugin::EditorComponent>);
    te::Plugin::EditorComponent* getEditor() const         { return editor.get(); }

    void recreateEditor();
    void recreateEditorAsync();

private:
    void moved() override;
    void userTriedToCloseWindow() override          { plugin.windowState->closeWindowExplicitly(); }
    void closeButtonPressed() override              { userTriedToCloseWindow(); }
    float getDesktopScaleFactor() const override    { return 1.0f; }

    std::unique_ptr<te::Plugin::EditorComponent> editor;

    te::Plugin& plugin;
    te::PluginWindowState& windowState;
    bool updateStoredBounds = false;
};

//==============================================================================
#if JUCE_LINUX
 constexpr bool shouldAddPluginWindowToDesktop = false;
#else
 constexpr bool shouldAddPluginWindowToDesktop = true;
#endif

PluginWindow::PluginWindow (te::Plugin& plug)
    : DocumentWindow (plug.getName(), Colours::black, DocumentWindow::closeButton, shouldAddPluginWindowToDesktop),
      plugin (plug), windowState (*plug.windowState)
{
    getConstrainer()->setMinimumOnscreenAmounts (0x10000, 50, 30, 50);
    setResizeLimits (100, 50, 4000, 4000);

    recreateEditor();

    setBoundsConstrained (getLocalBounds() + plugin.windowState->choosePositionForPluginWindow());

    // plug-in windows float above the DAW (like FL Studio) so they never get lost behind it
    setAlwaysOnTop (true);
    #if JUCE_LINUX
     addToDesktop();
    #endif

     updateStoredBounds = true;
}

PluginWindow::~PluginWindow()
{
    updateStoredBounds = false;
    plugin.edit.flushPluginStateIfNeeded (plugin);
    setEditor (nullptr);
}

void PluginWindow::show()
{
    setVisible (true);
    toFront (false);
    setBoundsConstrained (getBounds());
}

void PluginWindow::setEditor (std::unique_ptr<te::Plugin::EditorComponent> newEditor)
{
    JUCE_AUTORELEASEPOOL
    {
        setConstrainer (nullptr);
        editor.reset();

        if (newEditor != nullptr)
        {
            editor = std::move (newEditor);
            setContentNonOwned (editor.get(), true);
        }

        setResizable (editor == nullptr || editor->allowWindowResizing(), false);

        if (editor != nullptr && editor->allowWindowResizing())
            setConstrainer (editor->getBoundsConstrainer());
    }
}

std::unique_ptr<Component> PluginWindow::create (te::Plugin& plugin)
{
    if (auto externalPlugin = dynamic_cast<te::ExternalPlugin*> (&plugin))
        if (externalPlugin->getAudioPluginInstance() == nullptr)
            return nullptr;

    std::unique_ptr<PluginWindow> w;

    {
        struct Blocker : public Component { void inputAttemptWhenModal() override {} };

        Blocker blocker;
        blocker.enterModalState (false);

       #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
        if (! isDPIAware (plugin))
        {
            ScopedDPIAwarenessDisabler disableDPIAwareness;
            w = std::make_unique<PluginWindow> (plugin);
        }
        else
       #endif
        {
            w = std::make_unique<PluginWindow> (plugin);
        }
    }

    if (w == nullptr || w->getEditor() == nullptr)
        return {};

    w->show();

    return w;
}

void PluginWindow::recreateEditor()
{
    setEditor (nullptr);
    auto ed = plugin.createEditor();
    if (ed == nullptr && makeGenericEditor) ed = makeGenericEditor (plugin);
    setEditor (std::move (ed));
}

void PluginWindow::recreateEditorAsync()
{
    setEditor (nullptr);

    Timer::callAfterDelay (50, [this, sp = SafePointer<Component> (this)]
                               {
                                   if (sp != nullptr)
                                       recreateEditor();
                               });
}

void PluginWindow::moved()
{
    if (updateStoredBounds)
    {
        plugin.windowState->lastWindowBounds = getBounds();
        plugin.edit.pluginChanged (plugin);
    }
}

//==============================================================================
class ExtendedUIBehaviour : public te::UIBehaviour
{
public:
    ExtendedUIBehaviour() = default;

    // MIDI controller messages and MIDI learn go to the project that's open
    te::Edit* getCurrentlyFocusedEdit() override { return vegaFocusedEdit; }
    te::Edit* getLastFocusedEdit() override { return vegaFocusedEdit; }

    std::unique_ptr<Component> createPluginWindow (te::PluginWindowState& pws) override
    {
        if (auto ws = dynamic_cast<te::Plugin::WindowState*> (&pws))
        {
            auto w = PluginWindow::create (ws->plugin);
            if (w == nullptr)
            {
                juce::String why;
                if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&ws->plugin))
                {
                    why = ext->getLoadError();
                    if (why.isEmpty() && ext->getAudioPluginInstance() == nullptr)
                    {   // still loading in the background: open it as soon as it's ready
                        retryOpen (te::Plugin::Ptr (&ws->plugin), 40);
                        return {};
                    }
                }
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, ws->plugin.getName(),
                    "Couldn't open this plug-in's window." + (why.isNotEmpty() ? "\n\n" + why : juce::String())
                    + "\n\nIf it's a 32-bit or VST2-only plug-in, it can't be loaded - use its VST3 version.");
            }
            return w;
        }

        return {};
    }

    static void retryOpen (te::Plugin::Ptr p, int triesLeft)
    {
        juce::Timer::callAfterDelay (250, [p, triesLeft]
        {
            auto* ext = dynamic_cast<te::ExternalPlugin*> (p.get());
            if (ext == nullptr || p->windowState == nullptr) return;
            if (ext->getAudioPluginInstance() != nullptr) { p->showWindowExplicitly(); return; }
            if (triesLeft > 0 && ext->getLoadError().isEmpty()) retryOpen (p, triesLeft - 1);
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, p->getName(),
                     "This plug-in didn't finish loading." + (ext->getLoadError().isNotEmpty() ? "\n\n" + ext->getLoadError() : juce::String()));
        });
    }

    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& job) override
    {
        struct Runner : public juce::ThreadWithProgressWindow
        {
            Runner (te::ThreadPoolJobWithProgress& j) : ThreadWithProgressWindow (j.getJobName(), true, false), job (j) {}
            void run() override
            {
                while (! threadShouldExit())
                {
                    if (job.runJob() != juce::ThreadPoolJob::jobNeedsRunningAgain) break;
                    setProgress (job.getCurrentTaskProgress());
                }
            }
            te::ThreadPoolJobWithProgress& job;
        } runner (job);
        runner.runThread();
    }

    void recreatePluginWindowContentAsync (te::Plugin& p) override
    {
        if (auto* w = dynamic_cast<PluginWindow*> (p.windowState->pluginWindow.get()))
            return w->recreateEditorAsync();

        UIBehaviour::recreatePluginWindowContentAsync (p);
    }
};
} // namespace vegawin
