#pragma once
#include "StemSeparator.h"

/** DAW side of stem separation: runs VegaStemHelper (next to the DAW executable) and reads its progress.
    If the helper dies, the DAW keeps running and reports it; a crash in the AVX2 build is retried once with
    the generic build. */
struct StemClient
{
    static juce::File helper (bool avx2)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
       #if JUCE_WINDOWS
        return dir.getChildFile (avx2 ? "VegaStemHelperAVX2.exe" : "VegaStemHelper.exe");
       #else
        return dir.getChildFile ("VegaStemHelper");
       #endif
    }

    static StemSeparator::Result separate (StemSeparator::Model m, const juce::File& source, const juce::File& outDir, const StemSeparator::Progress& progress)
    {
        const bool fast = juce::SystemStats::hasAVX2() && helper (true).existsAsFile();
        bool crashed = false, cancelled = false;
        auto r = runOnce (helper (fast), m, source, outDir, progress, crashed, cancelled);
        if (crashed && fast && ! cancelled && helper (false).existsAsFile())
        {
            if (progress && ! progress (0.0f, "Retrying in compatibility mode...")) return {};
            r = runOnce (helper (false), m, source, outDir, progress, crashed, cancelled);
        }
        return r;
    }

    static StemSeparator::Result runOnce (const juce::File& exe, StemSeparator::Model m, const juce::File& source, const juce::File& outDir,
                                          const StemSeparator::Progress& progress, bool& crashed, bool& cancelled)
    {
        StemSeparator::Result r; crashed = cancelled = false;
        if (! exe.existsAsFile()) { r.error = "The stem separation engine is missing:\n" + exe.getFullPathName() + "\n\nPlease re-download the DAW zip and keep all its files together."; return r; }
        juce::ChildProcess cp;
        if (! cp.start (juce::StringArray { exe.getFullPathName(), m == StemSeparator::Model::sixStems ? "6" : "4", source.getFullPathName(), outDir.getFullPathName() },
                        juce::ChildProcess::wantStdOut))
        { r.error = "Couldn't start the stem separation engine (" + exe.getFileName() + ")."; return r; }

        std::string line; bool done = false; juce::String err;
        auto handle = [&] (const juce::String& l)
        {
            if (l.startsWith ("PROGRESS "))
            {
                const auto rest = l.substring (9);
                if (progress && ! progress (rest.upToFirstOccurrenceOf (" ", false, false).getFloatValue(), rest.fromFirstOccurrenceOf (" ", false, false)))
                    cancelled = true;
            }
            else if (l.startsWith ("STEM ")) r.stems.add (juce::File (l.substring (5)));
            else if (l.startsWith ("ERROR ")) err = l.substring (6);
            else if (l.startsWith ("DONE")) done = true;
        };
        char ch = 0;
        while (! cancelled)
        {
            if (progress && ! progress (-1.0f, {})) { cancelled = true; break; }    // -1: just checking for cancel
            if (cp.readProcessOutput (&ch, 1) <= 0) break;
            if (ch == '\n') { handle (juce::String::fromUTF8 (line.data(), (int) line.size()).trim()); line.clear(); }
            else if (ch != '\r') line += ch;
        }
        if (cancelled) { cp.kill(); r.stems.clear(); return r; }
        cp.waitForProcessToFinish (10000);
        const auto code = cp.getExitCode();
        if (done && r.stems.size() > 0) { r.ok = true; return r; }
        r.stems.clear();
        if (err.isNotEmpty()) { r.error = err; return r; }
        crashed = true;
        r.error = "The stem separation engine stopped unexpectedly" + (code != 0 ? " (code 0x" + juce::String::toHexString ((int) code).toUpperCase() + ")" : juce::String()) + ".\n\n"
                  "If this keeps happening, try a shorter section of the song, or close other programs to free up memory.";
        return r;
    }
};
