#pragma once
// =====================================================================
//  VEGA FL49 - keyboard brain (pure logic, no JUCE): scales, scale snap,
//  diatonic chords and the FLkey pad map. Runs on the MIDI thread, so it
//  never allocates after construction and never locks.
// =====================================================================
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace fl49
{
struct Scale { const char* name; std::array<int8_t, 12> steps; int size; };

/** The scales the FLkey 2 offers first, in the same names (Snap / Filter work with all of them). */
inline const std::array<Scale, 14>& scales()
{
    static const std::array<Scale, 14> s {{
        { "Major",            { 0, 2, 4, 5, 7, 9, 11 }, 7 },
        { "Minor",            { 0, 2, 3, 5, 7, 8, 10 }, 7 },
        { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 }, 7 },
        { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 }, 7 },
        { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 }, 7 },
        { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 }, 7 },
        { "Locrian",          { 0, 1, 3, 5, 6, 8, 10 }, 7 },
        { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 }, 7 },
        { "Melodic Minor",    { 0, 2, 3, 5, 7, 9, 11 }, 7 },
        { "Phrygian Dominant",{ 0, 1, 4, 5, 7, 8, 10 }, 7 },
        { "Minor Pentatonic", { 0, 3, 5, 7, 10 }, 5 },
        { "Major Pentatonic", { 0, 2, 4, 7, 9 }, 5 },
        { "Blues",            { 0, 3, 5, 6, 7, 10 }, 6 },
        { "Chromatic",        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 12 },
    }};
    return s;
}
inline const char* noteName (int pc) { static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[((pc % 12) + 12) % 12]; }

enum class KeyMode { off, snap, filter };            // like the FLkey: Snap to Scale / Filter out of Scale
enum class ChordMode { off, triad, seventh, ninth, soul };   // diatonic stacks; soul = 9th voicing with the 3rd on top (Soulful House keys)

struct Settings
{
    int root = 0;                 // 0 = C
    int scale = 0;                // index into scales()
    KeyMode keys = KeyMode::off;
    ChordMode chord = ChordMode::off;
};

/** Scale snap + chords with correct note-offs: every key remembers exactly which notes it started,
    and a shared count per output note means overlapping chords never cut each other off. */
class NoteProcessor
{
public:
    struct Out { uint8_t channel, note, velocity; bool on; };   // channel 1-16; velocity 0 for note-offs
    static constexpr int maxPerKey = 6;

    void setSettings (const Settings& s) { packed.store (pack (s)); }
    Settings getSettings() const { return unpack (packed.load()); }

    bool inScale (int note, const Settings& s) const
    {
        const auto& sc = scales()[(size_t) s.scale];
        const int pc = ((note - s.root) % 12 + 12) % 12;
        for (int i = 0; i < sc.size; ++i) if (sc.steps[(size_t) i] == pc) return true;
        return false;
    }
    /** Nearest note in the scale (ties go down, like the FLkey's Snap). */
    int snap (int note, const Settings& s) const
    {
        for (int d = 0; d < 12; ++d)
        {
            if (note - d >= 0 && inScale (note - d, s)) return note - d;
            if (note + d <= 127 && inScale (note + d, s)) return note + d;
        }
        return note;
    }
    /** The chord a key plays: stacked scale thirds from the (snapped) root. */
    int chordNotes (int note, const Settings& s, int* dest) const
    {
        const auto& sc = scales()[(size_t) s.scale];
        int n = 0;
        if (s.chord == ChordMode::off || sc.size < 5 || sc.size > 8) { dest[n++] = note; return n; }
        const int base = s.keys == KeyMode::off && ! inScale (note, s) ? snap (note, s) : note;
        // scale degree of the root
        const int rel = ((base - s.root) % 12 + 12) % 12;
        int degree = 0; for (int i = 0; i < sc.size; ++i) if (sc.steps[(size_t) i] == rel) degree = i;
        const int octaveBase = base - rel;
        auto deg = [&] (int k)    // the k-th scale step above the root's degree, as a MIDI note
        {
            const int d = degree + k, oct = d / sc.size;
            return octaveBase + 12 * oct + sc.steps[(size_t) (d % sc.size)];
        };
        const int stacks = s.chord == ChordMode::triad ? 3 : s.chord == ChordMode::seventh ? 4 : 5;
        if (s.chord == ChordMode::soul)
        {   // root, 5th, 7th, 9th, 3rd up an octave: the open "deep house" voicing
            for (int v : { deg (0), deg (4), deg (6), deg (8), deg (2) + 12 }) if (v >= 0 && v <= 127) dest[n++] = v;
            return n;
        }
        for (int k = 0; k < stacks; ++k) { const int v = deg (2 * k); if (v >= 0 && v <= 127) dest[n++] = v; }
        return n;
    }

    /** Feed one note message; writes the messages to send into `out` and returns how many. */
    int process (int channel, int note, int velocity, bool isOn, Out* out)
    {
        const int ch = (channel - 1) & 15;
        int n = 0;
        if (isOn && velocity > 0)
        {
            if (held[(size_t) ch][(size_t) note].count > 0) n += release (ch, note, out + n);   // retrigger: end the old one first
            const auto s = getSettings();
            int notes[maxPerKey]; int count = 0;
            if (s.keys == KeyMode::filter && ! inScale (note, s)) return n;                     // dropped (and its note-off too)
            const int root = s.keys == KeyMode::snap ? snap (note, s) : note;
            count = s.chord == ChordMode::off ? (notes[0] = root, 1) : chordNotes (root, s, notes);
            auto& h = held[(size_t) ch][(size_t) note];
            h.count = (uint8_t) count;
            for (int i = 0; i < count; ++i)
            {
                h.notes[(size_t) i] = (uint8_t) notes[i];
                auto& c = sounding[(size_t) ch][(size_t) notes[i]];
                if (c > 0) out[n++] = { (uint8_t) channel, (uint8_t) notes[i], 0, false };      // already sounding: restart it
                ++c;
                out[n++] = { (uint8_t) channel, (uint8_t) notes[i], (uint8_t) velocity, true };
            }
            return n;
        }
        return release (ch, note, out);
    }
    /** All notes this processor started (for panic / mode changes while keys are down). */
    int releaseAll (Out* out, int maxOut)
    {
        int n = 0;
        for (int ch = 0; ch < 16; ++ch)
            for (int note = 0; note < 128; ++note)
                if (held[(size_t) ch][(size_t) note].count > 0 && n + maxPerKey * 2 <= maxOut) n += release (ch, note, out + n);
        return n;
    }

private:
    int release (int ch, int note, Out* out)
    {
        auto& h = held[(size_t) ch][(size_t) note];
        int n = 0;
        for (int i = 0; i < h.count; ++i)
        {
            auto& c = sounding[(size_t) ch][h.notes[(size_t) i]];
            if (c > 0 && --c == 0) out[n++] = { (uint8_t) (ch + 1), h.notes[(size_t) i], 0, false };
        }
        h.count = 0;
        return n;
    }
    static uint32_t pack (const Settings& s) { return (uint32_t) (s.root & 15) | ((uint32_t) (s.scale & 31) << 4) | ((uint32_t) s.keys << 9) | ((uint32_t) s.chord << 11); }
    static Settings unpack (uint32_t v) { Settings s; s.root = (int) (v & 15) % 12; s.scale = std::min ((int) ((v >> 4) & 31), (int) scales().size() - 1); s.keys = (KeyMode) ((v >> 9) & 3); s.chord = (ChordMode) ((v >> 11) & 7); return s; }

    struct Held { std::array<uint8_t, maxPerKey> notes {}; uint8_t count = 0; };
    std::array<std::array<Held, 128>, 16> held {};
    std::array<std::array<uint8_t, 128>, 16> sounding {};
    std::atomic<uint32_t> packed { 0 };
};

/** Which (channel, note) each of the 16 pads sends. Pads are numbered like the FLkey's screen:
    0-7 = top row left to right, 8-15 = bottom row. Default = Drum mode on channel 10, GM notes from 36
    on the bottom-left pad (the layout the FLkey 2 uses for other DAWs). TEACH overwrites it. */
struct PadMap
{
    std::array<uint8_t, 16> channel {}, note {};
    PadMap() { setDefault(); }
    void setDefault()
    {
        for (int i = 0; i < 8; ++i) { note[(size_t) (8 + i)] = (uint8_t) (36 + i); note[(size_t) i] = (uint8_t) (44 + i); }
        channel.fill (10);
    }
    int padFor (int ch, int n) const
    {
        for (int i = 0; i < 16; ++i) if (channel[(size_t) i] == ch && note[(size_t) i] == n) return i;
        return -1;
    }
};
} // namespace fl49
