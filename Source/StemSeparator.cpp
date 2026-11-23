#include "StemSeparator.h"
#include "model.hpp"     // demucs.cpp
#include <thread>
#include <mutex>

namespace
{
struct Cancelled {};
constexpr double demucsRate = 44100.0;
}

bool StemSeparator::separateBuffer (const juce::File& modelPath, const juce::AudioBuffer<float>& in,
                                    std::vector<juce::AudioBuffer<float>>& stemsOut, Progress progress,
                                    juce::String& error, int maxThreads)
{
    const int total = in.getNumSamples();
    if (total < 4410) { error = "The audio is too short to separate."; return false; }

    if (progress && ! progress (0.01f, "Loading the separation model...")) return false;
    auto model = std::make_unique<demucscpp::demucs_model>();
    if (! demucscpp::load_demucs_model (modelPath.getFullPathName().toStdString(), model.get()))
    {
        error = "Couldn't load the separation model (" + modelPath.getFileName() + "). Delete it and try again to re-download.";
        return false;
    }
    const int nSources = model->is_4sources ? 4 : 6;

    // split into one segment per thread with 0.75 s of overlap each side, cross-faded back together
    // (same scheme as demucs.cpp's multi-threaded driver)
    const int hw = (int) std::max (1u, std::thread::hardware_concurrency());
    int threads = maxThreads > 0 ? maxThreads : juce::jlimit (1, 4, hw / 2);
    threads = std::max (1, std::min (threads, total / (int) (demucsRate * 15)));   // at least ~15 s per thread
    const int overlap = (int) (demucsRate * 0.75);
    const int segLen = (total + threads - 1) / threads;

    std::vector<Eigen::MatrixXf> segs;
    std::vector<int> segStart;
    for (int i = 0; i < threads; ++i)
    {
        const int start = std::max (0, i * segLen - overlap), end = std::min (total, (i + 1) * segLen + overlap);
        Eigen::MatrixXf m (2, end - start);
        for (int c = 0; c < 2; ++c)
        {
            const float* src = in.getReadPointer (std::min (c, in.getNumChannels() - 1));
            for (int j = start; j < end; ++j) m (c, j - start) = src[j];
        }
        segs.push_back (std::move (m)); segStart.push_back (start);
    }

    std::vector<Eigen::Tensor3dXf> outs ((size_t) threads);
    std::vector<float> threadProgress ((size_t) threads, 0.0f);
    std::atomic<bool> cancelled { false };
    std::mutex errLock; juce::String threadError;
    std::mutex cbLock;
    auto report = [&] (int idx, float p, const std::string& msg)
    {
        std::lock_guard<std::mutex> l (cbLock);
        threadProgress[(size_t) idx] = p;
        float sum = 0; for (auto v : threadProgress) sum += v;
        const float overall = 0.03f + 0.92f * sum / (float) threads;
        if (cancelled.load()) throw Cancelled();
        if (progress && ! progress (overall, juce::String ("Separating: ") + juce::String (juce::roundToInt (overall * 100)) + "%  " + juce::String (msg).upToFirstOccurrenceOf (",", false, false)))
        { cancelled = true; throw Cancelled(); }
    };

    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i)
        pool.emplace_back ([&, i]
        {
            try
            {
                demucscpp::ProgressCallback cb = [&, i] (float p, const std::string& msg) { report (i, p, msg); };
                outs[(size_t) i] = demucscpp::demucs_inference (*model, segs[(size_t) i], cb);
            }
            catch (const Cancelled&) { cancelled = true; }
            catch (const std::exception& e) { std::lock_guard<std::mutex> l (errLock); threadError = e.what(); cancelled = true; }
            catch (...) { std::lock_guard<std::mutex> l (errLock); threadError = "Separation failed."; cancelled = true; }
        });
    for (auto& t : pool) t.join();
    if (threadError.isNotEmpty()) { error = threadError; return false; }
    if (cancelled) return false;

    // cross-fade the segments back together
    stemsOut.assign ((size_t) nSources, juce::AudioBuffer<float> (2, total));
    for (auto& b : stemsOut) b.clear();
    std::vector<float> wsum ((size_t) total, 0.0f);
    for (int i = 0; i < threads; ++i)
    {
        const int start = segStart[(size_t) i], len = (int) segs[(size_t) i].cols();
        const bool fadeIn = i > 0, fadeOut = i < threads - 1;
        for (int j = 0; j < len; ++j)
        {
            float w = 1.0f;
            if (fadeIn && j < 2 * overlap) w = std::min (w, (j + 1.0f) / (2.0f * overlap));
            if (fadeOut && j >= len - 2 * overlap) w = std::min (w, (len - j) / (2.0f * overlap));
            const int g = start + j;
            wsum[(size_t) g] += w;
            for (int t = 0; t < nSources; ++t)
                for (int c = 0; c < 2; ++c)
                    stemsOut[(size_t) t].getWritePointer (c)[g] += w * outs[(size_t) i] (t, c, j);
        }
    }
    for (auto& b : stemsOut)
        for (int c = 0; c < 2; ++c)
        {
            auto* d = b.getWritePointer (c);
            for (int g = 0; g < total; ++g) if (wsum[(size_t) g] > 0) d[g] /= wsum[(size_t) g];
        }
    return true;
}

StemSeparator::Result StemSeparator::separate (Model m, const juce::File& audioFile, const juce::File& outDir,
                                               juce::AudioFormatManager& formats, Progress progress, int maxThreads)
{
    Result r;
    if (! hasModel (m)) { r.error = "The separation model isn't downloaded yet."; return r; }
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (audioFile));
    if (reader == nullptr) { r.error = "Couldn't read " + audioFile.getFileName(); return r; }
    if (reader->lengthInSamples > (juce::int64) (reader->sampleRate * 60 * 20)) { r.error = "Files longer than 20 minutes aren't supported."; return r; }
    if (progress && ! progress (0.0f, "Reading " + audioFile.getFileName() + "...")) return r;

    const int n = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> src (2, n);
    reader->read (&src, 0, n, 0, true, true);
    if (reader->numChannels == 1) src.copyFrom (1, 0, src, 0, 0, n);

    // resample to Demucs' 44.1 kHz
    juce::AudioBuffer<float> in44;
    const double ratio = reader->sampleRate / demucsRate;
    if (std::abs (ratio - 1.0) < 1.0e-9) in44 = std::move (src);
    else
    {
        const int outN = (int) std::ceil (n / ratio);
        in44.setSize (2, outN);
        for (int c = 0; c < 2; ++c)
        {
            juce::WindowedSincInterpolator interp;
            interp.process (ratio, src.getReadPointer (c), in44.getWritePointer (c), outN, n, 0);
        }
    }

    std::vector<juce::AudioBuffer<float>> stems;
    if (! separateBuffer (modelFile (m), in44, stems, progress, r.error, maxThreads)) return r;

    if (progress && ! progress (0.97f, "Writing stems...")) return r;
    outDir.createDirectory();
    juce::WavAudioFormat wav;
    const auto names = stemNames (m);
    for (size_t t = 0; t < stems.size(); ++t)
    {
        auto f = outDir.getChildFile (audioFile.getFileNameWithoutExtension() + " - " + names[(int) t] + ".wav");
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        if (os == nullptr) { r.error = "Couldn't write " + f.getFullPathName(); return r; }
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), demucsRate, 2, 24, {}, 0));
        if (w == nullptr) { r.error = "Couldn't write " + f.getFullPathName(); return r; }
        os.release();
        w->writeFromAudioSampleBuffer (stems[t], 0, stems[t].getNumSamples());
        r.stems.add (f);
    }
    r.ok = true;
    return r;
}
