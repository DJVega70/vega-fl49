#pragma once
#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

/** Per-channel DJ section: 3-band isolator EQ (HI / MID / LOW, full kill at the bottom of
    each knob) and a one-knob filter (left = low-pass, right = high-pass, centre = off).
    The isolator uses Linkwitz-Riley crossovers at 250 Hz and 2.5 kHz with the low band
    phase-matched, so with all bands at 0 dB the channel is flat. */
class VegaDJEQPlugin : public te::Plugin
{
public:
    static constexpr float killDb = -40.0f, maxDb = 6.0f;
    static const char* getPluginName() { return "DJ EQ"; }
    static inline const char* xmlTypeName = "vegaDjEq";

    explicit VegaDJEQPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        auto dbText = [] (float v) { return v <= killDb + 0.05f ? juce::String ("KILL") : (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
        auto dbParse = [] (const juce::String& s) { return s.containsIgnoreCase ("kill") ? killDb : s.getFloatValue(); };
        const char* ids[] { "hi", "mid", "low" };
        const char* names[] { "Hi", "Mid", "Low" };
        for (int i = 0; i < 3; ++i)
        {
            bandValue[i].referTo (state, juce::Identifier (ids[i]), um, 0.0f);
            band[i] = addParam (ids[i], names[i], { killDb, maxDb }, dbText, dbParse);
            band[i]->attachToCurrentValue (bandValue[i]);
        }
        filterValue.referTo (state, juce::Identifier ("djFilter"), um, 0.0f);
        filter = addParam ("djFilter", "Filter", { -1.0f, 1.0f }, [] (float v) { return describeFilter (v); }, [] (const juce::String& s) { return s.getFloatValue(); });
        filter->attachToCurrentValue (filterValue);
        tapeValue.referTo (state, juce::Identifier ("tape"), um, 0);
    }
    ~VegaDJEQPlugin() override
    {
        notifyListenersOfDeletion();
        for (auto& b : band) b->detachFromCurrentValue();
        filter->detachFromCurrentValue();
    }

    static juce::String describeFilter (float v)
    {
        if (std::abs (v) < 0.03f) return "OFF";
        const float hz = cutoffFor (v);
        return juce::String (v < 0 ? "LPF " : "HPF ") + (hz >= 1000 ? juce::String (hz / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (hz)));
    }
    static float cutoffFor (float v)
    {
        const float a = juce::jlimit (0.0f, 1.0f, (std::abs (v) - 0.03f) / 0.97f);
        return v < 0 ? 20000.0f * std::pow (60.0f / 20000.0f, a)      // low-pass sweeps down
                     : 18.0f * std::pow (9000.0f / 18.0f, a);         // high-pass sweeps up
    }

    juce::String getName() const override { return "DJ EQ"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "DJ EQ"; }
    juce::String getSelectableDescription() override { return "DJ EQ"; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        juce::dsp::ProcessSpec spec { info.sampleRate, (juce::uint32) std::max (1, info.blockSizeSamples), 2 };
        lp1.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);  hp1.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        lp2.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);  hp2.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        ap2.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        for (auto* f : { &lp1, &hp1 }) { f->prepare (spec); f->setCutoffFrequency (250.0f); }
        for (auto* f : { &lp2, &hp2, &ap2 }) { f->prepare (spec); f->setCutoffFrequency (2500.0f); }
        svf.prepare (spec); svf.setResonance (1.05f);
        for (int i = 0; i < 3; ++i) gain[i].reset (info.sampleRate, 0.02);
        mix.reset (info.sampleRate, 0.03);
        tapeAmt.reset (info.sampleRate, 0.05); tapeAmt.setCurrentAndTargetValue (tapeAmount (tapeValue.get()));
        for (auto& t : tapeState) t = {};
        for (int i = 0; i < 3; ++i) gain[i].setCurrentAndTargetValue (dbToGain (band[2 - i]->getCurrentValue()));
        isHighPass = false; svf.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        cutoff = cutoffFor (filter->getCurrentValue());
        svf.setCutoffFrequency (cutoff);
    }
    void deinitialise() override {}
    void reset() override { lp1.reset(); hp1.reset(); lp2.reset(); hp2.reset(); ap2.reset(); svf.reset(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer;
        const int nch = std::min (2, buf.getNumChannels()), n = fc.bufferNumSamples, start = fc.bufferStartSample;
        for (int i = 0; i < 3; ++i) gain[i].setTargetValue (dbToGain (band[2 - i]->getCurrentValue()));
        const float fv = filter->getCurrentValue();
        const bool hp = fv > 0;
        const float wantMix = juce::jlimit (0.0f, 1.0f, (std::abs (fv) - 0.02f) / 0.06f);
        if (hp != isHighPass && wantMix > 0.0f)
        {
            if (mix.getCurrentValue() < 0.001f)
            {   // faded out: now it's safe to flip between low-pass and high-pass
                isHighPass = hp; svf.setType (hp ? juce::dsp::StateVariableTPTFilterType::highpass : juce::dsp::StateVariableTPTFilterType::lowpass);
                svf.reset(); cutoff = cutoffFor (fv);
                mix.setTargetValue (wantMix);
            }
            else mix.setTargetValue (0.0f);   // fade the old filter out first (~30 ms)
        }
        else mix.setTargetValue (wantMix);
        const float targetCut = (hp == isHighPass) ? cutoffFor (fv) : cutoff;
        float* d[2] = { buf.getWritePointer (0, start), buf.getWritePointer (nch > 1 ? 1 : 0, start) };
        tapeAmt.setTargetValue (tapeAmount (tapeValue.get()));
        if (tapeAmt.isSmoothing() || tapeAmt.getTargetValue() > 0.0f)
            for (int s = 0; s < n; ++s)
            {
                const float a = tapeAmt.getNextValue();
                for (int c = 0; c < nch; ++c) d[c][s] = tapeSample (tapeState[c], d[c][s], a);
            }
        for (int s = 0; s < n; ++s)
        {
            if ((s & 31) == 0)
            {   // glide the cutoff in the log domain every 32 samples
                cutoff = std::exp (std::log (cutoff) + 0.12f * (std::log (targetCut) - std::log (cutoff)));
                svf.setCutoffFrequency (juce::jlimit (16.0f, (float) sampleRate * 0.45f, cutoff));
            }
            const float gl = gain[0].getNextValue(), gm = gain[1].getNextValue(), gh = gain[2].getNextValue(), m = mix.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                const float x = d[c][s];
                const float low = ap2.processSample (c, lp1.processSample (c, x));
                const float rest = hp1.processSample (c, x);
                const float mid = lp2.processSample (c, rest), high = hp2.processSample (c, rest);
                // band order in the knobs is HI / MID / LOW, gain[] is LOW / MID / HI
                float y = gh * high + gm * mid + gl * low;
                if (m > 0.0001f) y = (1.0f - m) * y + m * svf.processSample (c, y);
                else svf.processSample (c, y);   // keep the filter state warm
                d[c][s] = y;
            }
        }
        if (nch == 1 && buf.getNumChannels() > 1) buf.copyFrom (1, start, buf, 0, start, n);
    }

    // band[0] = hi, band[1] = mid, band[2] = low (as shown on the knobs)
    te::AutomatableParameter::Ptr band[3], filter;
    juce::CachedValue<float> bandValue[3], filterValue;
    /** Tape stage before the EQ: 0 = off, 1 = 15 ips (warmer: more drive, softer top, bigger head bump),
        2 = 30 ips (cleaner: a little drive, highs mostly kept). Unity gain. */
    juce::CachedValue<int> tapeValue;
    static juce::String tapeName (int t) { return t == 1 ? "15 ips" : t == 2 ? "30 ips" : "Off"; }

private:
    static float dbToGain (float db) { return db <= killDb + 0.05f ? 0.0f : juce::Decibels::decibelsToGain (db); }
    static float tapeAmount (int t) { return t == 1 ? 0.65f : t == 2 ? 0.32f : 0.0f; }
    struct TapeState { float low = 0, lp = 0; };
    float tapeSample (TapeState& st, float x, float a)
    {
        if (a <= 0.0f) return x;
        const float sr = (float) sampleRate;
        const float kLow = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 110.0f / sr);
        st.low += kLow * (x - st.low);
        x += st.low * 0.35f * a;                                        // head bump
        const float drive = 1.0f + 0.9f * a, bias = 0.1f * a, tb = std::tanh (bias);
        x = (std::tanh (drive * x + bias) - tb) / (drive * (1.0f - tb * tb));   // unity small-signal gain
        const float cut = 20000.0f - 12500.0f * a;
        const float kLp = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * std::min (cut, sr * 0.45f) / sr);
        st.lp += kLp * (x - st.lp);
        return st.lp;
    }
    TapeState tapeState[2];
    juce::SmoothedValue<float> tapeAmt;
    juce::dsp::LinkwitzRileyFilter<float> lp1, hp1, lp2, hp2, ap2;
    juce::dsp::StateVariableTPTFilter<float> svf;
    juce::SmoothedValue<float> gain[3], mix;   // gain[0] low, [1] mid, [2] high
    float cutoff = 1000.0f; bool isHighPass = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VegaDJEQPlugin)
};
