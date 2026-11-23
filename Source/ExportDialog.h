#pragma once
#include "DawUI.h"

/** File > Export: the mixdown or stems, in WAV / AIFF / FLAC / OGG / MP3, with optional normalising. */
class ExportDialog : public juce::Component
{
public:
    ExportDialog (DawSession& s, bool stemsFirst) : session (s)
    {
        auto label = [this] (juce::Label& l, const juce::String& t) { l.setText (t, juce::dontSendNotification); l.setFont (VegaFonts::ui (12.0f, true)); l.setColour (juce::Label::textColourId, VegaColours::textDim); addAndMakeVisible (l); };
        label (whatL, "Export"); label (fmtL, "Format"); label (depthL, "Bit depth"); label (rateL, "Sample rate"); label (normL, "Normalise"); label (rangeL, "Range");
        what.addItemList ({ "Mixdown (stereo file)", "Stems (one file per track, into a folder)" }, 1);
        what.setSelectedItemIndex (stemsFirst ? 1 : 0, juce::dontSendNotification);
        fmt.addItemList ({ "WAV", "AIFF", "FLAC (lossless, smaller)", "OGG Vorbis", "MP3 320 kbps (uses lame)" }, 1);
        fmt.setSelectedItemIndex (0, juce::dontSendNotification);
        depth.addItemList ({ "16-bit (dithered)", "24-bit", "32-bit float" }, 1); depth.setSelectedItemIndex (1, juce::dontSendNotification);
        rate.addItemList ({ "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz" }, 1); rate.setSelectedItemIndex (1, juce::dontSendNotification);
        norm.addItemList ({ "Off", "Peak to -0.3 dBFS", "Loudness -14 LUFS (streaming)", "Loudness -9 LUFS (club)" }, 1); norm.setSelectedItemIndex (0, juce::dontSendNotification);
        range.addItemList ({ "Whole song", "Loop range only" }, 1);
        range.setSelectedItemIndex (session.edit().getTransport().looping.get() ? 1 : 0, juce::dontSendNotification);
        master.setButtonText ("Include the master section (EQ / compressor on the master bus)");
        master.setToggleState (! stemsFirst, juce::dontSendNotification);
        for (auto* c : { &what, &fmt, &depth, &rate, &norm, &range }) { addAndMakeVisible (c); c->onChange = [this] { update(); }; }
        addAndMakeVisible (master);
        exportBtn.setButtonText ("EXPORT..."); cancelBtn.setButtonText ("CANCEL");
        exportBtn.setColour (juce::TextButton::buttonColourId, VegaColours::accentDim);
        addAndMakeVisible (exportBtn); addAndMakeVisible (cancelBtn);
        exportBtn.onClick = [this] { start(); };
        cancelBtn.onClick = [this] { close(); };
        note.setFont (VegaFonts::ui (11.0f)); note.setColour (juce::Label::textColourId, VegaColours::textDim); note.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (note);
        update();
        setSize (520, 380);
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff1b1b1d)); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 14);
        auto row = [&r] (juce::Label& l, juce::Component& c) { auto a = r.removeFromTop (28); l.setBounds (a.removeFromLeft (110)); c.setBounds (a); r.removeFromTop (8); };
        row (whatL, what); row (fmtL, fmt); row (depthL, depth); row (rateL, rate); row (normL, norm); row (rangeL, range);
        master.setBounds (r.removeFromTop (24).withTrimmedLeft (110)); r.removeFromTop (6);
        auto bottom = r.removeFromBottom (30);
        cancelBtn.setBounds (bottom.removeFromRight (100)); bottom.removeFromRight (8); exportBtn.setBounds (bottom.removeFromRight (130));
        note.setBounds (r);
    }

private:
    DawSession::ExportOptions options()
    {
        using O = DawSession::ExportOptions;
        O o;
        static const O::Format f[] { O::Format::wav, O::Format::aiff, O::Format::flac, O::Format::ogg, O::Format::mp3 };
        o.format = f[juce::jlimit (0, 4, fmt.getSelectedItemIndex())];
        static const int d[] { 16, 24, 32 };
        o.bitDepth = d[juce::jlimit (0, 2, depth.getSelectedItemIndex())];
        static const double sr[] { 44100.0, 48000.0, 88200.0, 96000.0 };
        o.sampleRate = sr[juce::jlimit (0, 3, rate.getSelectedItemIndex())];
        const int n = norm.getSelectedItemIndex();
        o.normalise = n == 0 ? O::Normalise::none : n == 1 ? O::Normalise::peak : O::Normalise::lufs;
        o.lufs = n == 3 ? -9.0f : -14.0f;
        auto& tr = session.edit().getTransport();
        if (range.getSelectedItemIndex() == 1 && tr.getLoopRange().getLength() > 0s) o.range = tr.getLoopRange();
        o.includeMaster = master.getToggleState();
        return o;
    }
    void update()
    {
        const int f = fmt.getSelectedItemIndex();
        depth.setEnabled (f <= 2);
        if (f == 2 && depth.getSelectedItemIndex() == 2) depth.setSelectedItemIndex (1, juce::dontSendNotification);
        juce::String t;
        if (what.getSelectedItemIndex() == 1)
            t << "Every track, bus and folder becomes its own file, numbered in track order and starting at the same time, so they line up "
                 "when you drop them into another DAW. A track routed into a bus or folder is exported through it. ";
        if (f == 4)
            t << (DawSession::findLame().existsAsFile() ? "MP3 is encoded with " + DawSession::findLame().getFileName() + ". "
                                                       : juce::String ("MP3 needs the free LAME encoder: put lame.exe next to the DAW, or you'll be asked where it is. "));
        if (norm.getSelectedItemIndex() >= 2) t << "Loudness normalising keeps true peaks under -1 dBTP.";
        note.setText (t, juce::dontSendNotification);
    }
    void start()
    {
        auto o = options();
        if (o.format == DawSession::ExportOptions::Format::mp3 && ! DawSession::findLame().existsAsFile())
        {
            chooser = std::make_unique<juce::FileChooser> ("Where is lame.exe (the LAME MP3 encoder)?", juce::File::getSpecialLocation (juce::File::userHomeDirectory), "lame.exe;lame");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().existsAsFile()) { DawSession::lameExecutable() = fc.getResult(); update(); start(); }
            });
            return;
        }
        const auto base = session.getProjectFile() == juce::File() ? juce::String ("Mixdown") : session.getProjectFile().getFileNameWithoutExtension();
        if (what.getSelectedItemIndex() == 1)
        {
            chooser = std::make_unique<juce::FileChooser> ("Folder for the stems", juce::File::getSpecialLocation (juce::File::userMusicDirectory));
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, o, base] (const juce::FileChooser& fc)
            {
                if (fc.getResult() == juce::File()) return;
                auto folder = fc.getResult().getChildFile (juce::File::createLegalFileName (base) + " stems");
                auto files = session.exportStems (folder, o);
                juce::AlertWindow::showMessageBoxAsync (files.isEmpty() ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon, "Export stems",
                    files.isEmpty() ? juce::String ("Nothing was exported - add some clips first.") : juce::String (files.size()) + " stems exported to:\n" + folder.getFullPathName());
                if (! files.isEmpty()) folder.revealToUser();
                close();
            });
            return;
        }
        const auto ext = DawSession::extensionFor (o.format);
        chooser = std::make_unique<juce::FileChooser> ("Export mixdown", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (base + "." + ext), "*." + ext);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this, o, ext] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File()) return;
            const bool ok = session.exportMix (fc.getResult(), o);
            juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "Export",
                ok ? "Exported " + fc.getResult().withFileExtension (ext).getFileName() + (o.range ? "\n(loop range only)" : "")
                   : juce::String ("The export didn't work - check there are clips to export (and, for MP3, that lame runs)."));
            close();
        });
    }
    void close() { if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState (0); }
    DawSession& session;
    juce::Label whatL, fmtL, depthL, rateL, normL, rangeL, note;
    juce::ComboBox what, fmt, depth, rate, norm, range;
    juce::ToggleButton master;
    juce::TextButton exportBtn, cancelBtn;
    std::unique_ptr<juce::FileChooser> chooser;
};
