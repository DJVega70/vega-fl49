#pragma once
#include "DawUI.h"

/** Side panel listing every plugin (Vega Productions, instruments, effects, built-in) with search,
    a background scanner, custom folders, and double-click / drag-to-track to load. */
class PluginBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer, private juce::ChangeListener
{
public:
    struct Item
    {
        juce::String name, info, builtInType;
        std::optional<juce::PluginDescription> desc;
        bool instrument = false, header = false, vega = false;
    };

    explicit PluginBrowser (ViewState& v) : vs (v), pm (v.edit().engine.getPluginManager())
    {
        title.setText ("PLUGINS", juce::dontSendNotification); title.setFont (VegaFonts::ui (12.0f, true));
        title.setColour (juce::Label::textColourId, VegaColours::text); addAndMakeVisible (title);
        search.setTextToShowWhenEmpty ("Search plugins...", VegaColours::textDim);
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141414));
        search.setColour (juce::TextEditor::outlineColourId, VegaColours::line);
        search.onTextChange = [this] { rebuildItems(); };
        addAndMakeVisible (search);
        filter.addItemList ({ "All", "Instruments", "Effects", "Vega Productions" }, 1);
        filter.setSelectedItemIndex (0, juce::dontSendNotification);
        filter.onChange = [this] { rebuildItems(); };
        addAndMakeVisible (filter);
        scanBtn.setButtonText ("SCAN"); scanBtn.setTooltip ("Look for new or updated VST3" + juce::String (isMac() ? " / AU" : "") + " plug-ins");
        scanBtn.onClick = [this] { if (scanning) stopScan (true); else startScan (false); };
        foldersBtn.setButtonText ("FOLDERS"); foldersBtn.onClick = [this] { editFolders(); };
        moreBtn.setButtonText ("..."); moreBtn.onClick = [this] { moreMenu(); };
        for (auto* b : { &scanBtn, &foldersBtn, &moreBtn }) addAndMakeVisible (b);
        list.setModel (this); list.setRowHeight (24);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff161616));
        list.setMultipleSelectionEnabled (false);
        addAndMakeVisible (list);
        status.setFont (VegaFonts::ui (10.0f)); status.setColour (juce::Label::textColourId, VegaColours::textDim);
        status.setJustificationType (juce::Justification::centredLeft); status.setMinimumHorizontalScale (0.6f);
        addAndMakeVisible (status);
        pm.knownPluginList.addChangeListener (this);
        rebuildItems();
    }
    ~PluginBrowser() override { pm.knownPluginList.removeChangeListener (this); }

    /** Called with the chosen item and the track it was dropped on (nullptr = selected track / new track) */
    std::function<void (const Item&, te::AudioTrack*)> onLoad;

    const Item* itemForDragDescription (const juce::var& d) const
    {
        const auto s = d.toString();
        if (! s.startsWith ("vegaplugin:")) return nullptr;
        const int i = s.fromFirstOccurrenceOf (":", false, false).getIntValue();
        return juce::isPositiveAndBelow (i, (int) items.size()) && ! items[(size_t) i].header ? &items[(size_t) i] : nullptr;
    }

    /** Quick background check for new plug-ins (already-known files are skipped, so this is fast) */
    void startScan (bool quiet)
    {
        if (scanning) return;
        scanning = true; quietScan = quiet; formatIndex = -1; filesDone = 0; failed.clear(); before = pm.knownPluginList.getNumTypes();
        scanBtn.setButtonText ("STOP");
        nextFormat();
        startTimer (5);
    }
    bool isScanning() const { return scanning; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1b1b));
        g.setColour (VegaColours::line); g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
        if (scanning)
        {
            auto bar = status.getBounds().toFloat().removeFromBottom (3).withTrimmedRight (4);
            g.setColour (juce::Colour (0xff0e0e0e)); g.fillRect (bar);
            g.setColour (VegaColours::accent); g.fillRect (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, progress)));
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6).withTrimmedRight (1);
        title.setBounds (r.removeFromTop (22));
        search.setBounds (r.removeFromTop (26)); r.removeFromTop (5);
        filter.setBounds (r.removeFromTop (24)); r.removeFromTop (5);
        auto b = r.removeFromTop (24);
        scanBtn.setBounds (b.removeFromLeft (64)); b.removeFromLeft (4);
        moreBtn.setBounds (b.removeFromRight (30)); b.removeFromRight (4);
        foldersBtn.setBounds (b);
        r.removeFromTop (5);
        status.setBounds (r.removeFromBottom (34));
        list.setBounds (r);
    }

private:
    static bool isMac() { return (juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0; }
    juce::File foldersFile() const { return vs.edit().engine.getPropertyStorage().getAppPrefsFolder().getChildFile ("VegaPluginFolders.txt"); }
    juce::File deadMansPedal() const { return vs.edit().engine.getPropertyStorage().getAppCacheFolder().getChildFile ("PluginScanDeadMansPedal"); }
    juce::StringArray customFolders() const
    {
        juce::StringArray a; a.addLines (foldersFile().loadFileAsString()); a.removeEmptyStrings(); a.trim(); return a;
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { rebuildItems(); }

    void rebuildItems()
    {
        items.clear();
        const auto q = search.getText().trim();
        const int f = filter.getSelectedItemIndex();
        auto matches = [&q] (const juce::String& a, const juce::String& b) { return q.isEmpty() || a.containsIgnoreCase (q) || b.containsIgnoreCase (q); };
        std::vector<Item> vega, inst, fx, builtIn;
        auto types = pm.knownPluginList.getTypes();
        std::sort (types.begin(), types.end(), [] (auto& a, auto& b) { return a.name.compareIgnoreCase (b.name) < 0; });
        for (auto& d : types)
        {
            if (! matches (d.name, d.manufacturerName)) continue;
            Item it; it.name = d.name; it.info = d.manufacturerName + "  " + d.pluginFormatName; it.desc = d; it.instrument = d.isInstrument;
            it.vega = d.manufacturerName.containsIgnoreCase ("Vega Productions");
            (it.vega ? vega : it.instrument ? inst : fx).push_back (it);
        }
        auto addBuiltIn = [&] (const juce::String& type, bool instrument)
        {
            const auto n = DawSession::builtInDisplayName (type);
            if (! matches (n, "Built-in")) return;
            Item it; it.name = n; it.info = "Built-in"; it.builtInType = type; it.instrument = instrument; builtIn.push_back (it);
        };
        addBuiltIn (te::FourOscPlugin::xmlTypeName, true); addBuiltIn (te::SamplerPlugin::xmlTypeName, true);
        for (auto& t : DawSession::builtInEffectTypes()) addBuiltIn (t, false);

        auto section = [this] (const juce::String& name, std::vector<Item>& v, int want)   // want: 0 all, 1 instruments, 2 effects
        {
            std::vector<Item> keep;
            for (auto& i : v) if (want == 0 || (want == 1) == i.instrument) keep.push_back (i);
            if (keep.empty()) return;
            Item h; h.header = true; h.name = name + "  (" + juce::String ((int) keep.size()) + ")"; items.push_back (h);
            for (auto& i : keep) items.push_back (i);
        };
        if (f == 3) section ("VEGA PRODUCTIONS", vega, 0);
        else
        {
            section ("VEGA PRODUCTIONS", vega, f);
            if (f != 2) section ("INSTRUMENTS", inst, 0);
            if (f != 1) section ("EFFECTS", fx, 0);
            section ("BUILT-IN", builtIn, f);
        }
        list.updateContent(); list.repaint();
        if (! scanning) updateStatusIdle();
    }

    void updateStatusIdle()
    {
        const int n = pm.knownPluginList.getNumTypes();
        status.setText (n == 0 ? "No plug-ins found yet. Press SCAN.\nVST3 folders: " + defaultPathText()
                               : juce::String (n) + " plug-ins. Double-click or drag onto a track.", juce::dontSendNotification);
    }
    juce::String defaultPathText() const
    {
        for (auto* fmt : pm.pluginFormatManager.getFormats())
            if (fmt->getName() == "VST3") return fmt->getDefaultLocationsToSearch().toString();
        return {};
    }

    // ---- ListBoxModel
    int getNumRows() override { return (int) items.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) items.size())) return;
        auto& it = items[(size_t) row];
        if (it.header)
        {
            g.setColour (juce::Colour (0xff222222)); g.fillRect (0, 0, w, h);
            g.setColour (it.name.startsWith ("VEGA") ? VegaColours::accent : VegaColours::textDim); g.setFont (VegaFonts::ui (9.5f, true));
            g.drawText (it.name, 8, 0, w - 12, h, juce::Justification::centredLeft);
            return;
        }
        if (selected) { g.setColour (VegaColours::accent.withAlpha (0.22f)); g.fillRect (0, 0, w, h); }
        g.setColour (it.instrument ? VegaColours::gold : VegaColours::accent); g.fillEllipse (10.0f, h * 0.5f - 3.0f, 6.0f, 6.0f);
        g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (11.5f));
        g.drawText (it.name, 22, 0, w - 28, h, juce::Justification::centredLeft, true);
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (8.5f));
        g.drawText (it.desc ? it.desc->pluginFormatName : juce::String ("TE"), w - 40, 0, 34, h, juce::Justification::centredRight);
    }
    juce::String getTooltipForRow (int row) override
    {
        if (! juce::isPositiveAndBelow (row, (int) items.size()) || items[(size_t) row].header) return {};
        auto& it = items[(size_t) row];
        return it.name + "\n" + it.info + (it.instrument ? "\nInstrument" : "\nEffect") + "\nDouble-click to load, or drag onto a track";
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { load (row, nullptr); }
    void returnKeyPressed (int row) override { load (row, nullptr); }
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() || ! juce::isPositiveAndBelow (row, (int) items.size()) || items[(size_t) row].header) return;
        auto& it = items[(size_t) row];
        juce::PopupMenu m;
        m.addItem (1, it.instrument ? "Load on selected instrument track" : "Add to selected track");
        if (it.instrument) m.addItem (2, "New instrument track with " + it.name);
        if (it.desc) { m.addSeparator(); m.addItem (3, "Remove from list (rescan to bring back)"); m.addItem (4, "Show file"); }
        m.showMenuAsync ({}, [this, row] (int r)
        {
            if (! juce::isPositiveAndBelow (row, (int) items.size())) return;
            auto it = items[(size_t) row];
            if (r == 1) load (row, nullptr);
            else if (r == 2) { if (auto* t = vs.session.addTrack (DawSession::TrackKind::midi, it.name)) { vs.selectTrack (t); if (onLoad) onLoad (it, t); } }
            else if (r == 3 && it.desc) pm.knownPluginList.removeType (*it.desc);
            else if (r == 4 && it.desc) juce::File (it.desc->fileOrIdentifier).revealToUser();
        });
    }
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (rows.isEmpty() || ! juce::isPositiveAndBelow (rows[0], (int) items.size()) || items[(size_t) rows[0]].header) return {};
        return "vegaplugin:" + juce::String (rows[0]);
    }
    void load (int row, te::AudioTrack* t)
    {
        if (juce::isPositiveAndBelow (row, (int) items.size()) && ! items[(size_t) row].header && onLoad) onLoad (items[(size_t) row], t);
    }

    // ---- scanning (runs on the message thread one file at a time, so the UI stays responsive
    //      and a plug-in that crashes is skipped next time via the dead-man's-pedal file)
    void nextFormat()
    {
        scanner.reset();
        auto formats = pm.pluginFormatManager.getFormats();
        while (++formatIndex < formats.size())
        {
            auto* fmt = formats[formatIndex];
            if (fmt->getName() != "VST3" && fmt->getName() != "AudioUnit") continue;
            juce::FileSearchPath path = fmt->getDefaultLocationsToSearch();
            if (fmt->getName() == "VST3") for (auto& c : customFolders()) if (juce::File::isAbsolutePath (c) && juce::File (c).isDirectory()) path.addIfNotAlreadyThere (juce::File (c));
            if (fmt->getName() == "VST3") path.removeNonExistentPaths();
            scanner = std::make_unique<juce::PluginDirectoryScanner> (pm.knownPluginList, *fmt, path, true, deadMansPedal(), false);
            formatName = fmt->getName();
            return;
        }
        finishScan();
    }
    void timerCallback() override
    {
        if (scanner == nullptr) return;
        const auto t0 = juce::Time::getMillisecondCounter();
        // do as many files as fit in ~30 ms per tick
        while (scanner != nullptr && juce::Time::getMillisecondCounter() - t0 < 30)
        {
            const auto next = scanner->getNextPluginFileThatWillBeScanned();
            if (! quietScan) status.setText ("Scanning " + formatName + ":  " + next, juce::dontSendNotification);
            juce::String name;
            const bool more = scanner->scanNextFile (true, name);
            ++filesDone; progress = scanner->getProgress();
            if (! more)
            {
                for (auto& f : scanner->getFailedFiles()) failed.addIfNotAlreadyThere (f);
                nextFormat();
            }
        }
        repaint();
    }
    void finishScan()
    {
        stopTimer(); scanning = false; scanBtn.setButtonText ("SCAN"); progress = 0;
        const int added = pm.knownPluginList.getNumTypes() - before;
        rebuildItems();
        if (! quietScan || added > 0)
            status.setText ("Scan done: " + juce::String (pm.knownPluginList.getNumTypes()) + " plug-ins"
                             + (added > 0 ? " (" + juce::String (added) + " new)" : juce::String())
                             + (failed.isEmpty() ? juce::String() : ", " + juce::String (failed.size()) + " couldn't load"), juce::dontSendNotification);
        if (! failed.isEmpty() && ! quietScan) status.setTooltip ("Couldn't load:\n" + failed.joinIntoString ("\n"));
        repaint();
    }
    void stopScan (bool userStopped)
    {
        scanner.reset(); stopTimer(); scanning = false; scanBtn.setButtonText ("SCAN"); progress = 0;
        if (userStopped) status.setText ("Scan stopped.", juce::dontSendNotification);
        rebuildItems(); repaint();
    }

    void editFolders()
    {
        auto* ed = new juce::TextEditor();
        ed->setMultiLine (true); ed->setReturnKeyStartsNewLine (true); ed->setSize (560, 220);
        ed->setText (customFolders().joinIntoString ("\n"));
        ed->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141414));
        auto* aw = new juce::AlertWindow ("Extra plug-in folders",
                                          "VST3 plug-ins in the standard folder are always found:\n" + defaultPathText()
                                          + "\n\nAdd any other folders that contain .vst3 files, one per line:", juce::MessageBoxIconType::NoIcon);
        aw->addCustomComponent (ed);
        aw->addButton ("Save and scan", 1, juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0));
        aw->addButton ("Add folder...", 2); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, ed] (int r)
        {
            const auto text = ed->getText();
            std::unique_ptr<juce::AlertWindow> owner (aw); std::unique_ptr<juce::TextEditor> edOwner (ed);
            if (r == 1) { foldersFile().replaceWithText (text); startScan (false); }
            else if (r == 2)
            {
                chooser = std::make_unique<juce::FileChooser> ("Choose a folder with VST3 plug-ins", juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory));
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, text] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() == juce::File()) return;
                    foldersFile().replaceWithText (text.trimEnd() + (text.trim().isEmpty() ? "" : "\n") + fc.getResult().getFullPathName());
                    startScan (false);
                });
            }
        }), false);
    }

    void moreMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Rescan everything (slow)");
        m.addItem (2, "Clear list");
        m.addItem (3, "Advanced plug-in list...");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreBtn), [this] (int r)
        {
            if (r == 1) { pm.knownPluginList.clear(); deadMansPedal().deleteFile(); startScan (false); }
            else if (r == 2) pm.knownPluginList.clear();
            else if (r == 3)
            {
                auto* lc = new juce::PluginListComponent (pm.pluginFormatManager, pm.knownPluginList, deadMansPedal(), nullptr, true);
                lc->setSize (760, 560);
                juce::DialogWindow::LaunchOptions o;
                o.dialogTitle = "Plug-in list"; o.content.setOwned (lc); o.dialogBackgroundColour = juce::Colour (0xff1e1e1e);
                o.resizable = true; o.useNativeTitleBar = true; o.launchAsync();
            }
        });
    }

    ViewState& vs;
    te::PluginManager& pm;
    juce::Label title, status;
    juce::TextEditor search;
    juce::ComboBox filter;
    juce::TextButton scanBtn, foldersBtn, moreBtn;
    juce::ListBox list { "plugins" };
    std::vector<Item> items;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::StringArray failed; juce::String formatName;
    bool scanning = false, quietScan = false; int formatIndex = -1, filesDone = 0, before = 0; double progress = 0;
};
