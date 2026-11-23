#pragma once
#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;

/** Vega Tape 2T: the master bus tape machine shown as the reel-to-reel on the mixer's control panel.
    DRIVE = how hard the tape is hit (saturation + glue), IPS = tape speed (7.5 / 15 / 30: slower is
    darker with a bigger, lower head bump), CAL = output calibration trim (-6..+6 dB).
    Bypass = the plug-in's enabled switch. Unity gain at CAL 0 for normal levels. */
class VegaTapeMachinePlugin : public te::Plugin
{
public:
    static const char* getPluginName() { return "Vega Tape 2T"; }
    static inline const char* xmlTypeName = "vegaTape2T";

    explicit VegaTapeMachinePlugin (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        driveValue.referTo (state, "drive", um, 0.35f);
        ipsValue.referTo (state, "ips", um, 1.0f);
        calValue.referTo (state, "cal", um, 0.0f);
        presetName.referTo (state, "preset", um, "Warm Bus - 15 ips");
        drive = addParam ("drive", "Drive", { 0.0f, 1.0f }, [] (float v) { return juce::String (juce::roundToInt (v * 100)) + "%"; }, [] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
        ips = addParam ("ips", "IPS", { 0.0f, 2.0f }, [] (float v) { return ipsName (v); }, [] (const juce::String& s) { return s.contains ("7") ? 0.0f : s.contains ("30") ? 2.0f : 1.0f; });
        cal = addParam ("cal", "Cal", { -6.0f, 6.0f }, [] (float v) { return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB"; }, [] (const juce::String& s) { return s.getFloatValue(); });
        drive->attachToCurrentValue (driveValue); ips->attachToCurrentValue (ipsValue); cal->attachToCurrentValue (calValue);
    }
    ~VegaTapeMachinePlugin() override
    {
        notifyListenersOfDeletion();
        drive->detachFromCurrentValue(); ips->detachFromCurrentValue(); cal->detachFromCurrentValue();
    }
    static juce::String ipsName (float v) { const int i = juce::jlimit (0, 2, juce::roundToInt (v)); return i == 0 ? "7.5 ips" : i == 1 ? "15 ips" : "30 ips"; }

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName (int) override { return "Tape"; }
    juce::String getSelectableDescription() override { return getPluginName(); }
    int getNumOutputChannelsGivenInputs (int n) override { return juce::jmin (n, 2); }
    BusLayout getBusses() const override { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        for (auto& s : st) s = {};
        driveS.reset (sampleRate, 0.05); calS.reset (sampleRate, 0.05);
        driveS.setCurrentAndTargetValue (drive->getCurrentValue()); calS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (cal->getCurrentValue()));
    }
    void deinitialise() override {}
    void reset() override { for (auto& s : st) s = {}; }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr) return;
        SCOPED_REALTIME_CHECK
        auto& buf = *fc.destBuffer;
        const int nch = std::min (2, buf.getNumChannels()), n = fc.bufferNumSamples, start = fc.bufferStartSample;
        const int speed = juce::jlimit (0, 2, juce::roundToInt (ips->getCurrentValue()));
        static constexpr float bumpHz[] { 70.0f, 95.0f, 120.0f }, lpHz[] { 9000.0f, 13500.0f, 18500.0f }, speedDrive[] { 1.25f, 1.0f, 0.75f };
        const float sr = (float) sampleRate;
        const float kLow = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * bumpHz[speed] / sr);
        driveS.setTargetValue (drive->getCurrentValue());
        calS.setTargetValue (juce::Decibels::decibelsToGain (cal->getCurrentValue()));
        float* d[2] = { buf.getWritePointer (0, start), buf.getWritePointer (nch > 1 ? 1 : 0, start) };
        for (int i = 0; i < n; ++i)
        {
            const float a = driveS.getNextValue() * speedDrive[speed], out = calS.getNextValue();
            const float dr = 1.0f + 1.6f * a, bias = 0.1f * a, tb = std::tanh (bias), norm = 1.0f / (dr * (1.0f - tb * tb));
            const float cut = std::min (lpHz[speed] + (1.0f - a) * 3000.0f, sr * 0.45f);
            const float kLp = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cut / sr);
            for (int c = 0; c < nch; ++c)
            {
                auto& s = st[c];
                float x = d[c][i];
                s.low += kLow * (x - s.low);
                x += s.low * (0.25f + 0.25f * a);                        // head bump
                x = (std::tanh (dr * x + bias) - tb) * norm;              // unity small-signal gain
                s.lp += kLp * (x - s.lp);
                d[c][i] = s.lp * out;
            }
        }
        if (nch == 1 && buf.getNumChannels() > 1) buf.copyFrom (1, start, buf, 0, start, n);
    }

    te::AutomatableParameter::Ptr drive, ips, cal;
    juce::CachedValue<float> driveValue, ipsValue, calValue;
    juce::CachedValue<juce::String> presetName;

    // ------------------------------------------------------------------ presets (built-in + yours)
    struct Preset { juce::String name; float drive, ips, cal; bool builtIn; };
    static juce::File userPresetFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Vega Productions DAW").getChildFile ("tape-presets.xml");
    }
    static juce::Array<Preset> presets()
    {
        juce::Array<Preset> list;
        list.add ({ "Warm Bus - 15 ips", 0.35f, 1.0f, 0.0f, true });
        list.add ({ "Clean Glue - 30 ips", 0.2f, 2.0f, 0.0f, true });
        list.add ({ "Lo-Fi Cassette - 7.5 ips", 0.7f, 0.0f, -1.0f, true });
        list.add ({ "Hot Drive - 15 ips", 0.85f, 1.0f, -2.0f, true });
        list.add ({ "Mastering Sheen - 30 ips", 0.45f, 2.0f, 0.0f, true });
        if (auto x = juce::XmlDocument::parse (userPresetFile()))
            for (auto* e : x->getChildIterator())
                list.add ({ e->getStringAttribute ("name"), (float) e->getDoubleAttribute ("drive"), (float) e->getDoubleAttribute ("ips"), (float) e->getDoubleAttribute ("cal"), false });
        return list;
    }
    static void writeUserPresets (const juce::Array<Preset>& all)
    {
        juce::XmlElement x ("VegaTapePresets");
        for (auto& p : all)
            if (! p.builtIn)
            {
                auto* e = x.createNewChildElement ("Preset");
                e->setAttribute ("name", p.name); e->setAttribute ("drive", p.drive); e->setAttribute ("ips", p.ips); e->setAttribute ("cal", p.cal);
            }
        userPresetFile().getParentDirectory().createDirectory();
        x.writeTo (userPresetFile());
    }
    void apply (const Preset& p)
    {
        drive->setParameter (p.drive, juce::sendNotification); ips->setParameter (p.ips, juce::sendNotification); cal->setParameter (p.cal, juce::sendNotification);
        presetName = p.name;
    }
    void saveAs (const juce::String& name)
    {
        auto all = presets();
        for (int i = all.size(); --i >= 0;) if (! all[i].builtIn && all[i].name == name) all.remove (i);
        all.add ({ name, drive->getCurrentValue(), ips->getCurrentValue(), cal->getCurrentValue(), false });
        writeUserPresets (all);
        presetName = name;
    }

private:
    double sampleRate = 44100.0;
    struct State { float low = 0, lp = 0; } st[2];
    juce::SmoothedValue<float> driveS, calS;
};
