#pragma once
#include "VegaUI.h"

/**
    Control Creator: design the knobs ("wheels"), faders ("sliders") and buttons VEGA FL49 uses everywhere.
    Pick a tab, change any setting on the left, see it live in the middle, keep looks you like in Presets
    (History remembers the last ones you made). With LIVE on, the whole DAW changes as you edit.
*/
class ControlCreator : public juce::Component, private juce::Timer
{
public:
    explicit ControlCreator (juce::File presetFolder) : presetDir (presetFolder)
    {
        using namespace vegacontrols;
        for (auto r : { Role::wheel, Role::slider, Role::button }) editing[(size_t) r] = Theme::get().style (r).createCopy();

        auto tab = [this] (juce::TextButton& b, const char* text, Role r)
        {
            b.setButtonText (text); b.setClickingTogglesState (false);
            b.onClick = [this, r] { setRole (r); };
            addAndMakeVisible (b);
        };
        tab (tabWheel, "Wheel", Role::wheel); tab (tabSlider, "Slider", Role::slider); tab (tabButton, "Button", Role::button);
        auto btn = [this] (juce::TextButton& b, const char* text, std::function<void()> f, const char* tip)
        {
            b.setButtonText (text); b.onClick = std::move (f); b.setTooltip (tip); addAndMakeVisible (b);
        };
        btn (savePreset, "Save in presets", [this] { saveAsPreset(); }, "Keep this look in Presets (saved with VEGA FL49's settings)");
        btn (randomize, "Randomize", [this] { juce::Random rnd; setEditing (vegacontrols::randomStyle (role, rnd)); }, "A random look to start from");
        btn (apply, "Apply", [this] { vegacontrols::Theme::get().setStyle (role, edit()); }, "Use this look for every control of this kind in VEGA FL49");
        btn (reset, "Default", [this] { setEditing (vegacontrols::defaultStyle (role)); }, "Back to the VEGA FL49 default for this tab");
        btn (exportBtn, "Export...", [this] { exportLook(); }, "Save the whole look (wheels, sliders, buttons) as a file to share");
        btn (importBtn, "Import...", [this] { importLook(); }, "Load a look file");
        live.setButtonText ("LIVE"); live.setClickingTogglesState (true); live.setToggleState (true, juce::dontSendNotification);
        live.setTooltip ("LIVE: the whole DAW follows your changes as you make them"); addAndMakeVisible (live);
        live.onClick = [this] { if (live.getToggleState()) vegacontrols::Theme::get().setStyle (role, edit()); };
        animate.setButtonText ("Animate"); animate.setClickingTogglesState (true); addAndMakeVisible (animate);
        histTab.setButtonText ("History"); presetTab.setButtonText ("Presets");
        histTab.onClick = [this] { showHistory = true; refreshTabs(); grid.repaint(); };
        presetTab.onClick = [this] { showHistory = false; refreshTabs(); grid.repaint(); };
        addAndMakeVisible (histTab); addAndMakeVisible (presetTab);

        preview.owner = this; addAndMakeVisible (preview);
        grid.owner = this; gridView.setViewedComponent (&grid, false); gridView.setScrollBarsShown (true, false); addAndMakeVisible (gridView);
        addAndMakeVisible (params);
        setRole (Role::wheel);
        setSize (1200, 600);
        startTimerHz (30);
    }
    ~ControlCreator() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff2b3135));
        g.setColour (juce::Colour (0xff1f2427)); g.fillRect (0, 0, getWidth(), 40);
        g.setColour (juce::Colour (0xff353c41)); g.fillRect (previewArea); g.fillRect (gridArea);
        g.setColour (juce::Colour (0xff1b1f22)); g.drawRect (previewArea, 1); g.drawRect (gridArea, 1);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (40).reduced (8, 7);
        for (auto* b : { &tabWheel, &tabSlider, &tabButton }) { b->setBounds (top.removeFromLeft (76)); top.removeFromLeft (4); }
        top.removeFromLeft (18);
        live.setBounds (top.removeFromLeft (60)); top.removeFromLeft (14);
        for (auto* b : { &importBtn, &exportBtn }) { b->setBounds (top.removeFromRight (86)); top.removeFromRight (4); }
        top.removeFromRight (14);
        apply.setBounds (top.removeFromRight (90)); top.removeFromRight (4);
        reset.setBounds (top.removeFromRight (80)); top.removeFromRight (4);
        randomize.setBounds (top.removeFromRight (96)); top.removeFromRight (4);
        savePreset.setBounds (top.removeFromRight (120));

        r.reduce (8, 8);
        gridArea = r.removeFromRight (232);
        r.removeFromRight (8);
        // the settings take the columns they need; the preview gets the rest
        const int paramsW = std::min (r.getWidth() - 300, params.requiredWidth (r.getHeight()));
        params.setBounds (r.removeFromLeft (paramsW));
        r.removeFromLeft (8);
        previewArea = r;
        {
            auto g = gridArea.reduced (1);
            auto tabs = g.removeFromTop (28).reduced (4, 3);
            histTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).reduced (1, 0)); presetTab.setBounds (tabs.reduced (1, 0));
            gridView.setBounds (g);
            grid.setSize (gridView.getWidth() - 8, grid.heightFor (gridView.getWidth() - 8));
        }
        {
            auto p = previewArea.reduced (1);
            animate.setBounds (p.removeFromBottom (28).reduced (6, 3).withWidth (90));
            preview.setBounds (p);
        }
        params.layout();
    }

    vegacontrols::Role currentRole() const { return role; }
    juce::ValueTree& edit() { return editing[(size_t) role]; }

private:
    using Role = vegacontrols::Role;

    //==========================================================================
    void setRole (Role r)
    {
        role = r;
        for (auto* b : { &tabWheel, &tabSlider, &tabButton }) b->setToggleState (false, juce::dontSendNotification);
        (r == Role::wheel ? tabWheel : r == Role::slider ? tabSlider : tabButton).setToggleState (true, juce::dontSendNotification);
        params.build (*this);
        reloadPresets();
        refreshTabs();
        resized();
        grid.setSize (gridView.getWidth() - 8, grid.heightFor (gridView.getWidth() - 8));
        repaint();
    }
    void refreshTabs()
    {
        histTab.setToggleState (showHistory, juce::dontSendNotification);
        presetTab.setToggleState (! showHistory, juce::dontSendNotification);
        grid.setSize (grid.getWidth(), grid.heightFor (grid.getWidth()));
    }
    /** Load a whole style into the editor (preset, history, random) */
    void setEditing (const juce::ValueTree& t)
    {
        edit() = t.createCopy();
        params.refreshValues();
        changed();
    }
    /** Something was edited */
    void changed()
    {
        preview.repaint();
        historyDirty = true; lastEdit = juce::Time::getMillisecondCounter();
        if (live.getToggleState()) vegacontrols::Theme::get().setStyle (role, edit(), false);
        else return;
        saveSoon = true;
    }
    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (historyDirty && now - lastEdit > 700)
        {   // one history entry per pause in editing
            historyDirty = false;
            auto& h = history[(size_t) role];
            if (h.empty() || ! h.front().isEquivalentTo (edit())) h.insert (h.begin(), edit().createCopy());
            if (h.size() > 40) h.pop_back();
            if (showHistory) { grid.setSize (grid.getWidth(), grid.heightFor (grid.getWidth())); grid.repaint(); }
            if (saveSoon) { saveSoon = false; vegacontrols::Theme::get().setStyle (role, edit(), true); }
        }
        if (animate.getToggleState()) { phase += 0.012f; preview.repaint(); }
    }

    // ---- presets
    juce::File roleFolder() const { return presetDir.getChildFile (vegacontrols::roleName (role)); }
    const std::vector<vegacontrols::Preset>& allPresets() const { return presetCache; }
    void reloadPresets()
    {
        presetCache = vegacontrols::builtInPresets (role);
        for (auto& f : roleFolder().findChildFiles (juce::File::findFiles, false, "*.xml"))
            if (auto x = juce::XmlDocument::parse (f))
                presetCache.push_back ({ f.getFileNameWithoutExtension(), juce::ValueTree::fromXml (*x) });
        grid.setSize (grid.getWidth(), grid.heightFor (grid.getWidth()));
        grid.repaint();
    }
    void saveAsPreset()
    {
        auto* w = new juce::AlertWindow ("Save in presets", "Name for this " + juce::String (vegacontrols::roleName (role)).toLowerCase() + " look:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", "My " + juce::String (vegacontrols::roleName (role)) + " " + juce::String (allPresets().size() + 1));
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int res)
        {
            const auto name = juce::File::createLegalFileName (w->getTextEditorContents ("name").trim());
            if (res == 1 && name.isNotEmpty())
            {
                roleFolder().createDirectory();
                if (auto x = edit().createXml()) x->writeTo (roleFolder().getChildFile (name + ".xml"));
                showHistory = false; reloadPresets(); refreshTabs();
            }
        }), true);
    }
    void exportLook()
    {
        chooser = std::make_unique<juce::FileChooser> ("Export look", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("My look.vegalook"), "*.vegalook");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File()) if (auto x = vegacontrols::Theme::get().toTree().createXml()) x->writeTo (f.withFileExtension ("vegalook"));
        });
    }
    void importLook()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import look", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.vegalook;*.xml");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (auto x = juce::XmlDocument::parse (fc.getResult()))
            {
                auto all = juce::ValueTree::fromXml (*x);
                for (auto r : { Role::wheel, Role::slider, Role::button })
                {
                    auto t = all.getChildWithName (vegacontrols::roleName (r));
                    if (t.isValid()) { editing[(size_t) r] = t.createCopy(); vegacontrols::Theme::get().setStyle (r, t, true); }
                }
                setEditing (edit());
            }
        });
    }

    //==========================================================================
    /** Draws one style (the editor's or a preset's) */
    static void drawSample (juce::Graphics& g, Role r, const juce::ValueTree& t, juce::Rectangle<float> area, float value, bool lit, const juce::String& caption = {})
    {
        using namespace vegacontrols;
        if (r == Role::wheel)
        {
            const auto s = WheelStyle::from (t);
            const float d = std::min (area.getWidth(), area.getHeight());
            paintWheel (g, juce::Rectangle<float> (d, d).withCentre (area.getCentre()), wheelStart + value * (wheelEnd - wheelStart), value, false, false, {}, &s);
        }
        else if (r == Role::slider)
        {
            const auto s = SliderStyle::from (t);
            const bool horizontal = area.getWidth() > area.getHeight() * 1.6f;
            if (horizontal)
            {
                const float capW = std::min (s.capWidth, area.getHeight() * 1.8f);
                paintSlider (g, area, area.getX() + capW * 0.5f + (area.getWidth() - capW) * value, false, false, lit, {}, &s, 0, 0);
            }
            else
            {
                const float scale = juce::jlimit (0.45f, 1.0f, area.getWidth() / 60.0f);
                const float capH = s.capHeight * scale, capW = s.capWidth * scale;
                paintSlider (g, area, area.getBottom() - capH * 0.5f - (area.getHeight() - capH) * value, true, false, lit, {}, &s, capW, capH);
            }
        }
        else
        {
            const auto s = ButtonStyle::from (t);
            paintButton (g, area, lit, false, false, {}, &s);
            if (caption.isNotEmpty())
            {
                g.setColour (buttonText (lit, {}, &s)); g.setFont (VegaFonts::ui (std::min (13.0f, area.getHeight() * 0.32f + 4.0f)));
                g.drawFittedText (caption, area.toNearestInt().reduced (4, 2), juce::Justification::centred, 2);
            }
        }
    }

    struct Preview : public juce::Component
    {
        ControlCreator* owner = nullptr;
        float value = 0.62f;
        int dragIndex = -1; float dragStart = 0;
        bool litSamples[6] { false, false, false, true, false, false };
        std::vector<juce::Rectangle<float>> samples() const
        {
            auto b = getLocalBounds().toFloat().reduced (14);
            const float designW = owner->role == Role::slider ? 270.0f : 236.0f;
            if (b.getWidth() > designW) b = b.withSizeKeepingCentre (std::max (designW, std::min (b.getWidth(), owner->role == Role::slider ? 420.0f : 330.0f)), b.getHeight());
            std::vector<juce::Rectangle<float>> v;
            switch (owner->role)
            {
                case Role::wheel:
                    v.push_back ({ b.getX(), b.getY(), 30, 30 });
                    v.push_back ({ b.getX() + 44, b.getY() - 4, 46, 46 });
                    v.push_back ({ b.getX() + 104, b.getY() - 8, 66, 66 });
                    v.push_back ({ b.getX(), b.getY() + 80, 96, 96 });
                    v.push_back ({ b.getX() + 110, b.getY() + 70, 110, 110 });
                    v.push_back (juce::Rectangle<float> (b.getWidth() * 0.62f, b.getWidth() * 0.62f).withCentre ({ b.getCentreX(), b.getBottom() - b.getWidth() * 0.34f }));
                    break;
                case Role::slider:
                    v.push_back ({ b.getX(), b.getY(), 40, 120 });
                    v.push_back ({ b.getX(), b.getY() + 140, 40, 110 });
                    v.push_back ({ b.getX() + 52, b.getY() + 10, 46, 220 });
                    v.push_back ({ b.getX() + 112, b.getY(), 64, 280 });
                    v.push_back ({ b.getX() + 190, b.getY(), 74, 370 });
                    v.push_back ({ b.getX(), b.getBottom() - 34, b.getWidth(), 30 });
                    break;
                case Role::button:
                    v.push_back ({ b.getX(), b.getY(), 50, 30 });
                    v.push_back ({ b.getX() + 120, b.getY(), 90, 30 });
                    v.push_back ({ b.getX(), b.getY() + 44, 160, 30 });
                    v.push_back ({ b.getX(), b.getY() + 92, 120, 110 });
                    v.push_back ({ b.getX() + 140, b.getY() + 92, 80, 80 });
                    v.push_back ({ b.getX() + 10, b.getY() + 220, 210, 190 });
                    break;
            }
            return v;
        }
        void paint (juce::Graphics& g) override
        {
            const auto s = samples();
            static const char* captions[] { "C", "Caption", "Button caption", "Caption", "Caption", "Caption" };
            for (size_t i = 0; i < s.size(); ++i)
            {
                float v = value;
                if (owner->animate.getToggleState()) v = 0.5f + 0.5f * std::sin (owner->phase * 6.28f + (float) i * 0.9f);
                const bool lit = owner->role == Role::button ? (litSamples[i] != (owner->animate.getToggleState() && std::sin (owner->phase * 6.28f + (float) i) > 0.6f)) : false;
                drawSample (g, owner->role, owner->edit(), s[i], v, lit, captions[i]);
            }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto s = samples();
            dragIndex = -1;
            for (size_t i = 0; i < s.size(); ++i)
                if (s[i].contains (e.position))
                {
                    dragIndex = (int) i; dragStart = value;
                    if (owner->role == Role::button) { litSamples[i] = ! litSamples[i]; repaint(); }
                }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragIndex < 0 || owner->role == Role::button) return;
            const bool horizontal = owner->role == Role::slider && dragIndex == 5;
            value = juce::jlimit (0.0f, 1.0f, dragStart + (horizontal ? e.getDistanceFromDragStartX() / 300.0f : -e.getDistanceFromDragStartY() / 200.0f));
            repaint();
        }
    };

    /** History / Presets thumbnails */
    struct Grid : public juce::Component, public juce::TooltipClient
    {
        juce::String getTooltip() override { return getTooltipFor (getMouseXYRelative()); }
        ControlCreator* owner = nullptr;
        static constexpr int cell = 50;
        int count() const { return owner->showHistory ? (int) owner->history[(size_t) owner->role].size() : (int) owner->allPresets().size(); }
        int columns (int w) const { return std::max (1, w / cell); }
        int heightFor (int w) const { return std::max (60, ((count() + columns (w) - 1) / columns (w)) * cell + 4); }
        juce::ValueTree item (int i, juce::String* name = nullptr) const
        {
            if (owner->showHistory) { auto& h = owner->history[(size_t) owner->role]; return i < (int) h.size() ? h[(size_t) i] : juce::ValueTree(); }
            auto p = owner->allPresets();
            if (i >= (int) p.size()) return {};
            if (name != nullptr) *name = p[(size_t) i].name;
            return p[(size_t) i].style;
        }
        juce::Rectangle<float> cellRect (int i) const
        {
            const int c = columns (getWidth());
            return { (float) ((i % c) * cell + 2), (float) ((i / c) * cell + 2), (float) cell - 4, (float) cell - 4 };
        }
        void paint (juce::Graphics& g) override
        {
            if (owner->showHistory && count() == 0)
            {
                g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (11.0f));
                g.drawFittedText ("Looks you make appear here.", getLocalBounds().reduced (8), juce::Justification::centredTop, 2);
                return;
            }
            const auto& presets = owner->allPresets();
            for (int i = 0; i < count(); ++i)
            {
                auto r = cellRect (i);
                const bool hov = r.contains (getMouseXYRelative().toFloat()) && isMouseOver();
                g.setColour (juce::Colour (hov ? 0xff4a5359 : 0xff3c444a)); g.fillRoundedRectangle (r, 3.0f);
                auto t = owner->showHistory ? owner->history[(size_t) owner->role][(size_t) i] : presets[(size_t) i].style;
                const auto role = owner->role;
                if (role == Role::slider) drawSample (g, role, t, r.reduced (12, 3), 0.65f, false);
                else if (role == Role::button) drawSample (g, role, t, r.reduced (7, 13), 0.0f, i % 3 == 0, "");
                else drawSample (g, role, t, r.reduced (3), 0.62f, false);
            }
        }
        void mouseMove (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int i = 0; i < count(); ++i)
                if (cellRect (i).contains (e.position))
                {
                    juce::String name;
                    auto t = item (i, &name);
                    if (! t.isValid()) return;
                    if (e.mods.isPopupMenu() && ! owner->showHistory)
                    {
                        auto f = owner->roleFolder().getChildFile (name + ".xml");
                        if (! f.existsAsFile()) return;     // built-in presets can't be deleted
                        juce::PopupMenu m; m.addItem (1, "Delete \"" + name + "\"");
                        m.showMenuAsync ({}, [this, f] (int res) { if (res == 1) { f.deleteFile(); owner->reloadPresets(); } });
                        return;
                    }
                    owner->setEditing (t);
                    return;
                }
        }
        juce::String getTooltipFor (juce::Point<int> p) const
        {
            for (int i = 0; i < count(); ++i)
                if (cellRect (i).contains (p.toFloat())) { juce::String n; item (i, &n); return n; }
            return {};
        }
    };

    /** The settings on the left, grouped like the hardware panels (Body, Dent, Fill, Rail, Cap ...) */
    struct Params : public juce::Component
    {
        struct Editor
        {
            const vegacontrols::ParamDef* def = nullptr;
            std::unique_ptr<juce::Component> comp;
            juce::Label label, valueLabel;
            juce::String group;
        };
        ControlCreator* owner = nullptr;
        std::vector<std::unique_ptr<Editor>> editors;
        juce::StringArray groups;
        std::vector<juce::Rectangle<int>> groupBoxes;

        struct ColourField : public juce::Component, private juce::ChangeListener
        {
            std::function<void (juce::Colour)> onChange;
            juce::Colour colour;
            juce::TextEditor hex;
            ColourField()
            {
                hex.setFont (VegaFonts::mono (12.0f));
                hex.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1f2427));
                hex.onReturnKey = hex.onFocusLost = [this]
                {
                    auto t = hex.getText().trim().removeCharacters ("#");
                    if (t.length() == 6) t = "ff" + t;
                    if (t.length() == 8 && t.containsOnly ("0123456789abcdefABCDEF")) set (juce::Colour::fromString (t), true);
                    else set (colour, false);
                };
                addAndMakeVisible (hex);
            }
            void set (juce::Colour c, bool notify)
            {
                colour = c;
                hex.setText ("#" + c.toDisplayString (false), false);
                repaint();
                if (notify && onChange) onChange (c);
            }
            void resized() override { hex.setBounds (getLocalBounds().withTrimmedRight (34)); }
            void paint (juce::Graphics& g) override
            {
                auto sw = getLocalBounds().removeFromRight (30).toFloat().reduced (1, 3);
                juce::Path tag;   // a label-tag swatch
                tag.startNewSubPath (sw.getX(), sw.getY()); tag.lineTo (sw.getRight() - 6, sw.getY()); tag.lineTo (sw.getRight(), sw.getCentreY());
                tag.lineTo (sw.getRight() - 6, sw.getBottom()); tag.lineTo (sw.getX(), sw.getBottom()); tag.closeSubPath();
                g.setColour (colour); g.fillPath (tag);
                g.setColour (juce::Colours::black.withAlpha (0.5f)); g.strokePath (tag, juce::PathStrokeType (1.0f));
            }
            void mouseDown (const juce::MouseEvent& e) override
            {
                if (e.x < getWidth() - 34) return;
                auto sel = std::make_unique<juce::ColourSelector> (juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace);
                sel->setCurrentColour (colour);
                sel->setSize (300, 300);
                sel->addChangeListener (this);
                juce::CallOutBox::launchAsynchronously (std::move (sel), getScreenBounds(), nullptr);
            }
            void changeListenerCallback (juce::ChangeBroadcaster* b) override
            {
                if (auto* sel = dynamic_cast<juce::ColourSelector*> (b)) set (sel->getCurrentColour(), true);
            }
        };

        void build (ControlCreator& o)
        {
            owner = &o;
            editors.clear(); groups.clear();
            removeAllChildren();
            const auto role = o.role;
            for (auto& d : vegacontrols::paramDefs (role))
            {
                auto e = std::make_unique<Editor>();
                e->def = &d; e->group = d.group;
                groups.addIfNotAlreadyThere (d.group);
                e->label.setText (d.label, juce::dontSendNotification);
                e->label.setFont (VegaFonts::ui (12.0f)); e->label.setColour (juce::Label::textColourId, VegaColours::text);
                addAndMakeVisible (e->label);
                e->valueLabel.setFont (VegaFonts::mono (11.0f)); e->valueLabel.setColour (juce::Label::textColourId, VegaColours::textDim);
                auto* ed = e.get();
                switch (d.type)
                {
                    case vegacontrols::ParamDef::choice:
                    {
                        auto cb = std::make_unique<juce::ComboBox>();
                        for (size_t i = 0; i < d.options.size(); ++i) cb->addItem (d.options[i], (int) i + 1);
                        cb->onChange = [this, ed, c = cb.get()] { set (*ed, c->getSelectedId() - 1); };
                        e->comp = std::move (cb);
                        break;
                    }
                    case vegacontrols::ParamDef::colour:
                    {
                        auto cf = std::make_unique<ColourField>();
                        cf->onChange = [this, ed] (juce::Colour c) { set (*ed, c.toString()); };
                        e->comp = std::move (cf);
                        break;
                    }
                    case vegacontrols::ParamDef::toggle:
                    {
                        auto tb = std::make_unique<juce::TextButton>();
                        tb->setClickingTogglesState (true);
                        tb->onClick = [this, ed, t = tb.get()] { set (*ed, t->getToggleState() ? 1 : 0); t->setButtonText (t->getToggleState() ? "ON" : "OFF"); };
                        e->comp = std::move (tb);
                        break;
                    }
                    default:
                    {
                        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
                        s->setRange (d.lo, d.hi, d.type == vegacontrols::ParamDef::integer ? 1.0 : 0.0);
                        s->setDoubleClickReturnValue (true, d.def);
                        s->onValueChange = [this, ed, sl = s.get()]
                        {
                            const double v = sl->getValue();
                            set (*ed, ed->def->type == vegacontrols::ParamDef::integer ? juce::var ((int) v) : juce::var (v));
                            ed->valueLabel.setText (valueText (*ed->def, v), juce::dontSendNotification);
                        };
                        addAndMakeVisible (e->valueLabel);
                        e->comp = std::move (s);
                        break;
                    }
                }
                addAndMakeVisible (*e->comp);
                editors.push_back (std::move (e));
            }
            refreshValues();
            layout();
        }
        static juce::String valueText (const vegacontrols::ParamDef& d, double v)
        {
            return d.type == vegacontrols::ParamDef::integer ? juce::String ((int) v) : juce::String (v, 2);
        }
        void set (Editor& e, const juce::var& v)
        {
            if (updating) return;
            owner->edit().setProperty (e.def->id, v, nullptr);
            owner->changed();
        }
        void refreshValues()
        {
            const juce::ScopedValueSetter<bool> svs (updating, true);
            auto& t = owner->edit();
            const auto role = owner->role;
            for (auto& e : editors)
            {
                const auto& d = *e->def;
                if (auto* cb = dynamic_cast<juce::ComboBox*> (e->comp.get())) cb->setSelectedId ((int) vegacontrols::numOf (role, t, d.id) + 1, juce::dontSendNotification);
                else if (auto* cf = dynamic_cast<ColourField*> (e->comp.get())) cf->set (vegacontrols::colOf (role, t, d.id), false);
                else if (auto* tb = dynamic_cast<juce::TextButton*> (e->comp.get()))
                {
                    const bool on = vegacontrols::numOf (role, t, d.id) > 0.5f;
                    tb->setToggleState (on, juce::dontSendNotification); tb->setButtonText (on ? "ON" : "OFF");
                }
                else if (auto* s = dynamic_cast<juce::Slider*> (e->comp.get()))
                {
                    const double v = vegacontrols::numOf (role, t, d.id);
                    s->setValue (v, juce::dontSendNotification);
                    e->valueLabel.setText (valueText (d, v), juce::dontSendNotification);
                }
            }
        }
        int requiredWidth (int height) const
        {
            const int colW = 236, rowH = 34, head = 28, gap = 8;
            int cols = 1, y = 0;
            for (auto& gname : groups)
            {
                int rows = 0;
                for (auto& e : editors) if (e->group == gname) ++rows;
                const int h = head + rows * rowH + 8;
                if (y > 0 && y + h > height) { ++cols; y = 0; }
                y += h + gap;
            }
            return cols * (colW + gap);
        }
        void layout()
        {
            groupBoxes.clear();
            const int colW = 236, rowH = 34, head = 28, gap = 8;
            int x = 0, y = 0;
            for (auto& gname : groups)
            {
                int rows = 0;
                for (auto& e : editors) if (e->group == gname) ++rows;
                const int h = head + rows * rowH + 8;
                if (y > 0 && y + h > getHeight()) { x += colW + gap; y = 0; }
                groupBoxes.push_back ({ x, y, colW, h });
                int ry = y + head;
                for (auto& e : editors)
                {
                    if (e->group != gname) continue;
                    auto row = juce::Rectangle<int> (x + 10, ry, colW - 20, rowH - 4);
                    e->label.setBounds (row.removeFromLeft (96));
                    if (dynamic_cast<juce::Slider*> (e->comp.get()) != nullptr)
                    {
                        e->comp->setBounds (row.removeFromLeft (52).withSizeKeepingCentre (40, 34));
                        e->valueLabel.setBounds (row);
                    }
                    else if (dynamic_cast<juce::TextButton*> (e->comp.get()) != nullptr) e->comp->setBounds (row.withWidth (56).reduced (0, 3));
                    else e->comp->setBounds (row.reduced (0, 2));
                    ry += rowH;
                }
                y += h + gap;
            }
            repaint();
        }
        void resized() override { layout(); }
        void paint (juce::Graphics& g) override
        {
            for (size_t i = 0; i < groupBoxes.size() && i < (size_t) groups.size(); ++i)
            {
                auto b = groupBoxes[i].toFloat();
                g.setColour (juce::Colour (0xff353c41)); g.fillRect (b);
                g.setColour (juce::Colour (0xff1f2427)); g.drawRect (b, 1.0f);
                // the slanted title tab
                juce::Path tab; tab.startNewSubPath (b.getX(), b.getY()); tab.lineTo (b.getX() + 92, b.getY()); tab.lineTo (b.getX() + 80, b.getY() + 20); tab.lineTo (b.getX(), b.getY() + 20); tab.closeSubPath();
                g.setColour (juce::Colour (0xff2b3135)); g.fillPath (tab);
                g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (12.0f, true));
                g.drawText (groups[(int) i], (int) b.getX() + 8, (int) b.getY() + 2, 80, 16, juce::Justification::centredLeft);
            }
        }
        bool updating = false;
    };

    juce::File presetDir;
    Role role = Role::wheel;
    std::array<juce::ValueTree, 3> editing;
    std::array<std::vector<juce::ValueTree>, 3> history;
    std::vector<vegacontrols::Preset> presetCache;
    bool showHistory = false, historyDirty = false, saveSoon = false;
    juce::uint32 lastEdit = 0;
    float phase = 0;
    juce::TextButton tabWheel, tabSlider, tabButton, savePreset, randomize, apply, reset, exportBtn, importBtn, live, animate, histTab, presetTab;
    Preview preview; Grid grid; juce::Viewport gridView; Params params;
    juce::Rectangle<int> previewArea, gridArea;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tips { this };

    friend struct Preview; friend struct Grid; friend struct Params;
};

/** The Control Creator in its own window (Options > Control Creator...) */
class ControlCreatorWindow : public juce::DocumentWindow
{
public:
    explicit ControlCreatorWindow (juce::File presetFolder)
        : DocumentWindow ("Control Creator", juce::Colour (0xff2b3135), DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new ControlCreator (presetFolder), true);
        setResizable (true, false);
        setResizeLimits (980, 520, 2400, 1400);
        centreWithSize (1240, 640);
    }
    void closeButtonPressed() override { setVisible (false); }
};
