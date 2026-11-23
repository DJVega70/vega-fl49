#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <functional>

/** AI stem separation with Demucs v4 (hybrid transformer) via demucs.cpp (MIT, github.com/sevagh/demucs.cpp).
    The network weights are not shipped with the DAW: they're downloaded once, on request, into the
    user's application-data folder. */
class StemSeparator
{
public:
    enum class Model { fourStems, sixStems };

    static juce::String modelFileName (Model m) { return m == Model::sixStems ? "ggml-model-htdemucs-6s-f16.bin" : "ggml-model-htdemucs-4s-f16.bin"; }
    static juce::URL modelUrl (Model m) { return juce::URL ("https://huggingface.co/datasets/Retrobear/demucs.cpp/resolve/main/" + modelFileName (m)); }
    static juce::String modelSizeText (Model m) { return m == Model::sixStems ? "55 MB" : "84 MB"; }
    static juce::File modelsFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Vega Productions").getChildFile ("Models");
    }
    static juce::File modelFile (Model m) { return modelsFolder().getChildFile (modelFileName (m)); }
    static bool hasModel (Model m) { return modelFile (m).getSize() > 10 * 1024 * 1024; }
    static juce::StringArray stemNames (Model m)
    {
        juce::StringArray s { "Drums", "Bass", "Other", "Vocals" };
        if (m == Model::sixStems) s.addArray ({ "Guitar", "Piano" });
        return s;
    }

    /** Progress: return false to cancel. fraction 0..1 */
    using Progress = std::function<bool (float fraction, const juce::String& status)>;

    struct Result
    {
        bool ok = false; juce::String error;
        juce::Array<juce::File> stems;      // in stemNames() order
    };

    /** Separates any audio file JUCE can read. Blocking: call from a background thread.
        Writes 24-bit 44.1 kHz WAV stems "<name> - <Stem>.wav" into outDir. */
    static Result separate (Model model, const juce::File& audioFile, const juce::File& outDir,
                            juce::AudioFormatManager& formats, Progress progress, int maxThreads = 0);

    /** Separates an in-memory stereo buffer that's already at 44.1 kHz (used by separate() and the tests). */
    static bool separateBuffer (const juce::File& modelPath, const juce::AudioBuffer<float>& stereo44k,
                                std::vector<juce::AudioBuffer<float>>& stemsOut, Progress progress,
                                juce::String& error, int maxThreads = 0);
};
