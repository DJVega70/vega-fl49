#pragma once
#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

/** Master bus channel strip: input trim, HP/LP filters, 4-band EQ (HF / HMF / LMF / LF, shelving or bell
    on the outer bands), compressor (auto make-up, peak/RMS, fast attack) and gate/expander (range, hold),
    polarity, output trim and cut. Order: input -> filters -> EQ -> gate -> compressor -> output. */
class VegaMasterStripPlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Master Strip"; }
    static inline const char* xmlTypeName = "vegaMasterStrip";

    enum P { inGain, outGain, hpf, lpf, eqOn, hfGain, hfFreq, hfBell, hmfGain, hmfFreq, hmfQ, lmfGain, lmfFreq, lmfQ, lfGain, lfFreq, lfBell,
             dynOn, compRatio, compThresh, compRelease, compFast, compPeak, gateRange, gateThresh, gateRelease, gateHold, gateExpand, gateFast,
             polarity, cut, numParams };
    struct Def { const char* id; const char* name; float lo, hi, def, mid; };   // mid: value at 12 o'clock (0 = linear)
    static const Def& def (int p)
    {
        static const Def d[numParams] = {
            { "msIn", "Input", -20, 20, 0, 0 },              { "msOut", "Output", -20, 20, 0, 0 },
            { "msHpf", "HP Filter", 16, 500, 16, 80 },       { "msLpf", "LP Filter", 3000, 22000, 22000, 9000 },
            { "msEqOn", "EQ In", 0, 1, 1, 0 },
            { "msHfG", "HF Gain", -15, 15, 0, 0 },           { "msHfF", "HF Freq", 1500, 22000, 8000, 6000 },   { "msHfBell", "HF Bell", 0, 1, 0, 0 },
            { "msHmfG", "HMF Gain", -15, 15, 0, 0 },         { "msHmfF", "HMF Freq", 600, 7000, 2500, 2000 },  { "msHmfQ", "HMF Q", 0.3f, 4, 0.8f, 1.0f },
            { "msLmfG", "LMF Gain", -15, 15, 0, 0 },         { "msLmfF", "LMF Freq", 200, 2000, 600, 600 },    { "msLmfQ", "LMF Q", 0.3f, 4, 0.8f, 1.0f },
            { "msLfG", "LF Gain", -15, 15, 0, 0 },           { "msLfF", "LF Freq", 40, 600, 100, 150 },        { "msLfBell", "LF Bell", 0, 1, 0, 0 },
            { "msDynOn", "Dynamics In", 0, 1, 1, 0 },
            { "msRatio", "Ratio", 1, 20, 1, 4 },             { "msThr", "Comp Threshold", -30, 10, 0, 0 },     { "msRel", "Comp Release", 0.05f, 4, 0.3f, 0.5f },
            { "msFast", "Comp Fast Attack", 0, 1, 0, 0 },    { "msPeak", "Comp Peak", 0, 1, 0, 0 },
            { "msGRange", "Gate Range", 0, 40, 0, 0 },       { "msGThr", "Gate Threshold", -80, 0, -60, -40 }, { "msGRel", "Gate Release", 0.05f, 4, 0.3f, 0.5f },
            { "msGHold", "Gate Hold", 0, 4, 0.05f, 0.5f },   { "msExp", "Expand", 0, 1, 0, 0 },                { "msGFast", "Gate Fast Attack", 0, 1, 0, 0 },
            { "msPol", "Polarity", 0, 1, 0, 0 },             { "msCut", "Cut", 0, 1, 0, 0 } };
        return d[p];
    }
    static bool isSwitch (int p) { return def (p).hi == 1 && def (p).lo == 0 && p != hpf; }
    static juce::String text (int p, float v)
    {
        auto hz = [] (float f) { return f >= 1000 ? juce::String (f / 1000.0f, f >= 10000 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; };
        auto db = [] (float x) { return (x > 0 ? "+" : "") + juce::String (x, 1) + " dB"; };
        switch (p)
        {
            case hpf: return v <= 16.5f ? juce::String ("HP Out") : "HP " + hz (v);
            case lpf: return v >= 21500 ? juce::String ("LP Out") : "LP " + hz (v);
            case hfFreq: case hmfFreq: case lmfFreq: case lfFreq: return hz (v);
            case hmfQ: case lmfQ: return "Q " + juce::String (v, 2);
            case compRatio: return v >= 19.9f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e : 1")) : juce::String (v, 1) + " : 1";
            case compRelease: case gateRelease: case gateHold: return v < 1 ? juce::String (juce::roundToInt (v * 1000)) + " ms" : juce::String (v, 2) + " s";
            case gateRange: return juce::String (v, 1) + " dB";
            case gateThresh: case compThresh: return juce::String (v, 1) + " dB";
            default: return isSwitch (p) ? (v > 0.5f ? "On" : "Off") : db (v);
        }
    }

    explicit VegaMasterStripPlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        for (int i = 0; i < numParams; ++i)
        {
            const auto& d = def (i);
            values[i].referTo (state, juce::Identifier (d.id), um, d.def);
            params[i] = addParam (d.id, d.name, { d.lo, d.hi }, [i] (float v) { return text (i, v); }, [] (const juce::String& s) { return s.getFloatValue(); });
            params[i]->attachToCurrentValue (values[i]);
        }
    }
    ~VegaMasterStripPlugin() override
    {
        notifyListenersOfDeletion();
        for (auto& p : params) p->detachFromCurrentValue();
    }

    juce::String getName() const override { return "Master Strip"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Master"; }
    juce::String getSelectableDescription() override { return "Master Strip"; }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override { return BusLayout::singleStereoInOut(); }

    float get (int p) const { return params[p]->getCurrentValue(); }
    void set (int p, float v) { params[p]->setParameter (juce::jlimit (def (p).lo, def (p).hi, v), juce::sendNotification); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        for (auto& l : last) l = -1.0e9f;
        resetState();
    }
    void deinitialise() override {}
    void reset() override { resetState(); }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer;
        const int n = fc.bufferNumSamples, start = fc.bufferStartSample, nch = std::min (2, buf.getNumChannels());
        float v[numParams]; for (int i = 0; i < numParams; ++i) v[i] = get (i);
        updateFilters (v);
        const float inG = juce::Decibels::decibelsToGain (v[inGain]);
        const float outG = (v[polarity] > 0.5f ? -1.0f : 1.0f) * juce::Decibels::decibelsToGain (v[outGain]);
        const bool hpOn = v[hpf] > 16.5f, lpOn = v[lpf] < 21500.0f, eq = v[eqOn] > 0.5f, dyn = v[dynOn] > 0.5f, cutOn = v[cut] > 0.5f;
        const bool compOn = dyn && v[compRatio] > 1.01f, gateOn = dyn && v[gateRange] > 0.1f;
        const float sr = (float) sampleRate;
        // compressor timing
        const float cAtt = coef (v[compFast] > 0.5f ? 0.0003f : 0.010f, sr), cRel = coef (v[compRelease], sr), rmsC = coef (0.005f, sr);
        const float ratio = v[compRatio] >= 19.9f ? 1000.0f : v[compRatio];
        const float makeup = compOn ? (1.0f - 1.0f / ratio) * std::max (0.0f, -v[compThresh]) * 0.5f : 0.0f;
        // gate timing
        const float gAtt = coef (v[gateFast] > 0.5f ? 0.0001f : 0.0015f, sr), gRel = coef (v[gateRelease] * 0.35f, sr), envRel = coef (0.005f, sr);
        const int holdSamples = (int) (v[gateHold] * sr);
        float peakIn[2] {}, peakOut[2] {}, maxGr = 0, maxGate = 0;
        float* d[2] = { buf.getWritePointer (0, start), buf.getWritePointer (nch > 1 ? 1 : 0, start) };
        for (int s = 0; s < n; ++s)
        {
            float x[2];
            for (int c = 0; c < 2; ++c)
            {
                float y = d[c < nch ? c : 0][s] * inG;
                peakIn[c] = std::max (peakIn[c], std::abs (y));
                if (hpOn) { y = hp2[c].processSingleSampleRaw (y); y = hp1[c].process (y); }
                if (lpOn) y = lp[c].processSingleSampleRaw (y);
                if (eq)
                {
                    if (std::abs (v[lfGain]) > 0.05f) y = lf[c].processSingleSampleRaw (y);
                    if (std::abs (v[lmfGain]) > 0.05f) y = lmf[c].processSingleSampleRaw (y);
                    if (std::abs (v[hmfGain]) > 0.05f) y = hmf[c].processSingleSampleRaw (y);
                    if (std::abs (v[hfGain]) > 0.05f) y = hf[c].processSingleSampleRaw (y);
                }
                x[c] = y;
            }
            const float link = std::max (std::abs (x[0]), std::abs (x[1]));
            float g = 1.0f;
            if (gateOn)
            {
                gateEnv = link > gateEnv ? link : gateEnv * envRel + link * (1.0f - envRel);
                const float lv = juce::Decibels::gainToDecibels (gateEnv, -120.0f);
                float target = 0.0f;
                if (lv >= v[gateThresh]) holdLeft = holdSamples;
                else if (holdLeft > 0) --holdLeft;
                else target = v[gateExpand] > 0.5f ? std::min (v[gateRange], v[gateThresh] - lv) : v[gateRange];
                gateAtt = target < gateAtt ? gateAtt * gAtt + target * (1.0f - gAtt) : gateAtt * gRel + target * (1.0f - gRel);
                g *= juce::Decibels::decibelsToGain (-gateAtt);
                maxGate = std::max (maxGate, gateAtt);
            }
            else gateAtt = 0;
            if (compOn)
            {
                float lvl;
                if (v[compPeak] > 0.5f) lvl = link * g;
                else { rmsState = rmsState * rmsC + (link * g) * (link * g) * (1.0f - rmsC); lvl = std::sqrt (rmsState) * 1.414f; }
                const float lv = juce::Decibels::gainToDecibels (lvl, -120.0f), over = lv - v[compThresh];
                constexpr float knee = 3.0f;
                float target = 0;
                if (over > knee * 0.5f) target = over * (1.0f - 1.0f / ratio);
                else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; target = (1.0f - 1.0f / ratio) * k * k / (2.0f * knee); }
                compGr = target > compGr ? compGr * cAtt + target * (1.0f - cAtt) : compGr * cRel + target * (1.0f - cRel);
                g *= juce::Decibels::decibelsToGain (makeup - compGr);
                maxGr = std::max (maxGr, compGr);
            }
            else compGr = 0;
            const float og = cutOn ? 0.0f : outG * g;
            for (int c = 0; c < nch; ++c) { const float y = x[c] * og; d[c][s] = y; peakOut[c] = std::max (peakOut[c], std::abs (y)); }
        }
        for (int c = 0; c < 2; ++c) { bump (meterIn[c], peakIn[c]); bump (meterOut[c], cutOn ? 0.0f : peakOut[nch > 1 ? c : 0]); }
        bump (grComp, maxGr); bump (grGate, maxGate);
    }

    // meters for the UI (peak since last read; the UI exchanges them with 0)
    std::atomic<float> meterIn[2] {}, meterOut[2] {}, grComp { 0 }, grGate { 0 };

    te::AutomatableParameter::Ptr params[numParams];
    juce::CachedValue<float> values[numParams];

private:
    struct OnePoleHP
    {
        float a = 0, x1 = 0, y1 = 0;
        void set (float f, double sr) { a = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * f / sr); }
        float process (float x) { const float y = a * (y1 + x - x1); x1 = x; y1 = y; return y; }
        void reset() { x1 = y1 = 0; }
    };
    static float coef (float seconds, float sr) { return std::exp (-1.0f / std::max (1.0f, seconds * sr)); }
    static void bump (std::atomic<float>& a, float v) { float cur = a.load(); while (v > cur && ! a.compare_exchange_weak (cur, v)) {} }
    void resetState()
    {
        for (int c = 0; c < 2; ++c) { hp2[c].reset(); hp1[c].reset(); lp[c].reset(); lf[c].reset(); lmf[c].reset(); hmf[c].reset(); hf[c].reset(); }
        gateEnv = gateAtt = compGr = rmsState = 0; holdLeft = 0;
    }
    void updateFilters (const float* v)
    {
        const double sr = sampleRate; const float ny = (float) sr * 0.45f;
        auto changed = [&] (std::initializer_list<int> ids) { bool c = false; for (int i : ids) if (v[i] != last[i]) { c = true; last[i] = v[i]; } return c; };
        if (changed ({ hpf })) { const float f = std::min (v[hpf], ny); for (int c = 0; c < 2; ++c) { hp2[c].setCoefficients (juce::IIRCoefficients::makeHighPass (sr, f, 1.0)); hp1[c].set (f, sr); } }
        if (changed ({ lpf })) { const auto co = juce::IIRCoefficients::makeLowPass (sr, std::min (v[lpf], ny), 0.707); for (auto& f : lp) f.setCoefficients (co); }
        if (changed ({ hfGain, hfFreq, hfBell }))
        {
            const float f = std::min (v[hfFreq], ny), g = juce::Decibels::decibelsToGain (v[hfGain]);
            const auto co = v[hfBell] > 0.5f ? juce::IIRCoefficients::makePeakFilter (sr, f, 0.7, g) : juce::IIRCoefficients::makeHighShelf (sr, f, 0.6, g);
            for (auto& x : hf) x.setCoefficients (co);
        }
        if (changed ({ hmfGain, hmfFreq, hmfQ })) { const auto co = juce::IIRCoefficients::makePeakFilter (sr, std::min (v[hmfFreq], ny), v[hmfQ], juce::Decibels::decibelsToGain (v[hmfGain])); for (auto& x : hmf) x.setCoefficients (co); }
        if (changed ({ lmfGain, lmfFreq, lmfQ })) { const auto co = juce::IIRCoefficients::makePeakFilter (sr, v[lmfFreq], v[lmfQ], juce::Decibels::decibelsToGain (v[lmfGain])); for (auto& x : lmf) x.setCoefficients (co); }
        if (changed ({ lfGain, lfFreq, lfBell }))
        {
            const float g = juce::Decibels::decibelsToGain (v[lfGain]);
            const auto co = v[lfBell] > 0.5f ? juce::IIRCoefficients::makePeakFilter (sr, v[lfFreq], 0.7, g) : juce::IIRCoefficients::makeLowShelf (sr, v[lfFreq], 0.6, g);
            for (auto& x : lf) x.setCoefficients (co);
        }
    }
    juce::IIRFilter hp2[2], lp[2], lf[2], lmf[2], hmf[2], hf[2];
    OnePoleHP hp1[2];
    float last[numParams] {};
    float gateEnv = 0, gateAtt = 0, compGr = 0, rmsState = 0; int holdLeft = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VegaMasterStripPlugin)
};
