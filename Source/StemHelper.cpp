// VegaStemHelper: runs Demucs stem separation in its own process for the DAW.
//   VegaStemHelper <4|6> <audio file> <output folder> [threads]
//   VegaStemHelper --selftest <model file> [threads]
// Reports on stdout, one line each: PROGRESS <0..1> <text> | STEM <path> | ERROR <text> | DONE
#include "StemSeparator.h"
#include <cstdio>

static void say (const juce::String& s)
{
    auto u = s.replaceCharacters ("\r\n", "  ").toStdString() + "\n";
    std::fwrite (u.data(), 1, u.size(), stdout);
    std::fflush (stdout);
}

static int run (const juce::StringArray& a)
{
    auto progress = [] (float p, const juce::String& m) { say ("PROGRESS " + juce::String (p, 4) + " " + m); return true; };
    if (a[0] == "--selftest")
    {
        // 20 s of a synthetic band (kick, bass, chord, noise hats) through the real network
        const double fs = 44100; const int n = (int) (20 * fs);
        juce::AudioBuffer<float> mix (2, n); juce::Random r (1);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs, beat = std::fmod (t, 0.5);
            float v = (float) (0.6 * std::exp (-beat * 20) * std::sin (2 * juce::MathConstants<double>::pi * 55 * beat));
            v += 0.2f * (float) std::sin (2 * juce::MathConstants<double>::pi * 110 * t);
            for (double f : { 261.6, 329.6, 392.0 }) v += 0.06f * (float) std::sin (2 * juce::MathConstants<double>::pi * f * t);
            v += (std::fmod (t, 0.25) < 0.03 ? 0.1f : 0.0f) * (r.nextFloat() - 0.5f);
            mix.setSample (0, i, v); mix.setSample (1, i, v * 0.9f);
        }
        std::vector<juce::AudioBuffer<float>> stems; juce::String err;
        if (! StemSeparator::separateBuffer (juce::File (a[1]), mix, stems, progress, err, a[2].getIntValue()))
        { say ("ERROR " + (err.isEmpty() ? juce::String ("failed") : err)); return 2; }
        bool finite = stems.size() >= 4;
        for (auto& s : stems) for (int c = 0; c < 2; ++c) for (int i = 0; i < s.getNumSamples(); ++i) finite &= std::isfinite (s.getSample (c, i));
        if (! finite) { say ("ERROR bad output"); return 3; }
        say ("DONE " + juce::String ((int) stems.size()) + " stems");
        return 0;
    }
    if (a.size() < 3) { say ("ERROR usage: VegaStemHelper <4|6> <audio file> <output folder> [threads]"); return 1; }
    const auto model = a[0] == "6" ? StemSeparator::Model::sixStems : StemSeparator::Model::fourStems;
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto res = StemSeparator::separate (model, juce::File (a[1]), juce::File (a[2]), formats, progress, a[3].getIntValue());
    if (! res.ok) { say ("ERROR " + (res.error.isEmpty() ? juce::String ("Separation failed.") : res.error)); return 2; }
    for (auto& f : res.stems) say ("STEM " + f.getFullPathName());
    say ("DONE");
    return 0;
}

#if JUCE_WINDOWS
int wmain (int argc, wchar_t** argv)     // wide arguments: file names with accents etc. survive
{
    juce::StringArray a; for (int i = 1; i < argc; ++i) a.add (juce::String (argv[i]));
    try { return run (a); } catch (const std::exception& e) { say ("ERROR " + juce::String (e.what())); return 4; }
}
#else
int main (int argc, char** argv)
{
    juce::StringArray a; for (int i = 1; i < argc; ++i) a.add (juce::String::fromUTF8 (argv[i]));
    try { return run (a); } catch (const std::exception& e) { say ("ERROR " + juce::String (e.what())); return 4; }
}
#endif
