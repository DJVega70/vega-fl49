#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// =====================================================================
//  VEGA FL49 - the control look: every knob ("wheel"), fader ("slider") and button in the DAW is
//  painted from one of three styles. The Control Creator edits them; the theme is saved in the
//  app's settings and can be shared as preset files.
// =====================================================================
namespace vegacontrols
{
enum class Role { wheel, slider, button };

/** One adjustable setting of a style (drives the Control Creator's editors and the saved XML). */
struct ParamDef
{
    enum Type { choice, colour, number, integer, toggle };
    const char* id; const char* label; const char* group; Type type;
    float def = 0, lo = 0, hi = 1;
    juce::uint32 colourDef = 0;
    std::vector<const char*> options {};
};

inline const std::vector<ParamDef>& paramDefs (Role r)
{
    using P = ParamDef;
    static const std::vector<ParamDef> wheel {
        { "body", "Style", "Body", P::choice, 0, 0, 3, 0, { "Plain", "Gear", "Knurled", "Scalloped" } },
        { "texture", "Texture", "Body", P::choice, 1, 0, 3, 0, { "Matte", "MidGlossy", "Glossy", "Metal" } },
        { "bodyColour", "Color", "Body", P::colour, 0, 0, 1, 0xff3f484d },
        { "size", "Size", "Body", P::number, 0.84f, 0.5f, 1.0f },
        { "shadow", "Shadow size", "Body", P::number, 0.7f, 0.0f, 1.0f },
        { "dents", "Count", "Dent", P::integer, 0, 0, 32 },
        { "dentDepth", "Depth", "Dent", P::number, 0.08f, -0.3f, 0.3f },
        { "dentWidth", "Size", "Dent", P::number, 0.4f, 0.05f, 0.95f },
        { "fill", "Fill color", "Fill", P::colour, 0, 0, 1, 0xff8cc8ee },
        { "pressed", "Pressed", "Fill", P::colour, 0, 0, 1, 0xffffffff },
        { "pointer", "Pointer", "Various", P::choice, 1, 0, 4, 0, { "None", "ShortLine", "LongLine", "Dot", "Triangle" } },
        { "pointerWidth", "Pointer width", "Various", P::number, 1.0f, 0.4f, 3.0f },
        { "rail", "Style", "Rail", P::choice, 1, 0, 4, 0, { "None", "Arc", "RulerOut", "RulerIn", "Dots" } },
        { "railColour", "Color", "Rail", P::colour, 0, 0, 1, 0xff1b1f22 },
        { "bigSteps", "Ruler big steps", "Rail", P::integer, 4, 1, 16 },
        { "smallSteps", "Ruler small steps", "Rail", P::integer, 2, 0, 10 },
        { "pointerOnCap", "Pointer on cap", "Options", P::toggle, 0 },
        { "fillRail", "Fill rail", "Options", P::toggle, 1 },
        { "fillRuler", "Fill ruler", "Options", P::toggle, 1 },
        { "reflection", "Reflection", "Options", P::toggle, 1 },
        { "cap", "Style", "Cap", P::choice, 2, 0, 4, 0, { "None", "Flat", "Dome", "Concave", "Chrome" } },
        { "capColour", "Color", "Cap", P::colour, 0, 0, 1, 0xff3f484d },
        { "capSize", "Size", "Cap", P::number, 0.74f, 0.25f, 1.0f },
        { "capShadow", "Shadow size", "Cap", P::number, 0.35f, 0.0f, 1.0f },
    };
    static const std::vector<ParamDef> slider {
        { "railColour", "Color", "Rail", P::colour, 0, 0, 1, 0xff15191b },
        { "railWidth", "Width", "Rail", P::number, 4.0f, 1.0f, 12.0f },
        { "ruler", "Ruler", "Rail", P::choice, 1, 0, 3, 0, { "None", "Both sides", "Left", "Right" } },
        { "rulerColour", "Ruler color", "Rail", P::colour, 0, 0, 1, 0xff8d979c },
        { "bigSteps", "Ruler big steps", "Rail", P::integer, 4, 1, 16 },
        { "smallSteps", "Ruler small steps", "Rail", P::integer, 3, 0, 10 },
        { "fillRail", "Fill rail", "Fill", P::toggle, 0 },
        { "fill", "Fill color", "Fill", P::colour, 0, 0, 1, 0xff8cc8ee },
        { "cap", "Style", "Cap", P::choice, 0, 0, 4, 0, { "Classic", "Flat", "Grip", "Lit", "Hardware" } },
        { "capColour", "Color", "Cap", P::colour, 0, 0, 1, 0xff4a545a },
        { "lineColour", "Line color", "Cap", P::colour, 0, 0, 1, 0xffffffff },
        { "capWidth", "Width", "Cap", P::number, 30.0f, 12.0f, 52.0f },
        { "capHeight", "Height", "Cap", P::number, 18.0f, 8.0f, 52.0f },
        { "gloss", "Gloss", "Cap", P::number, 0.55f, 0.0f, 1.0f },
        { "shadow", "Shadow size", "Cap", P::number, 0.7f, 0.0f, 1.0f },
    };
    static const std::vector<ParamDef> button {
        { "shape", "Shape", "Shape", P::choice, 1, 0, 2, 0, { "Rounded", "Pillow", "Square" } },
        { "corner", "Corner", "Shape", P::number, 6.0f, 0.0f, 20.0f },
        { "roundness", "Pillow curve", "Shape", P::number, 4.5f, 2.2f, 10.0f },
        { "colour", "Color", "Fill", P::colour, 0, 0, 1, 0xff3f484d },
        { "onColour", "Lit color", "Fill", P::colour, 0, 0, 1, 0xffa9d8f5 },
        { "border", "Border", "Fill", P::colour, 0, 0, 1, 0xff1b2023 },
        { "text", "Text", "Text", P::colour, 0, 0, 1, 0xffdde2e5 },
        { "textOn", "Text when lit", "Text", P::colour, 0, 0, 1, 0xff16202a },
        { "texture", "Texture", "Look", P::choice, 1, 0, 2, 0, { "Matte", "MidGlossy", "Glossy" } },
        { "bevel", "Bevel", "Look", P::number, 0.55f, 0.0f, 1.0f },
        { "glow", "Glow", "Look", P::number, 0.75f, 0.0f, 1.0f },
        { "shadow", "Shadow size", "Look", P::number, 0.6f, 0.0f, 1.0f },
    };
    return r == Role::wheel ? wheel : r == Role::slider ? slider : button;
}

inline const char* roleName (Role r) { return r == Role::wheel ? "Wheel" : r == Role::slider ? "Slider" : "Button"; }

/** A style = a ValueTree of the role's settings (missing ones = defaults) */
inline juce::ValueTree defaultStyle (Role r)
{
    juce::ValueTree t (roleName (r));
    for (auto& d : paramDefs (r))
        t.setProperty (d.id, d.type == ParamDef::colour ? juce::var (juce::Colour (d.colourDef).toString()) : juce::var (d.def), nullptr);
    return t;
}
inline float num (const juce::ValueTree& t, const char* id, float def) { auto v = t.getProperty (id); return v.isVoid() ? def : (float) v; }
inline juce::Colour col (const juce::ValueTree& t, const char* id, juce::uint32 def)
{
    auto v = t.getProperty (id);
    return v.isVoid() ? juce::Colour (def) : juce::Colour::fromString (v.toString());
}
inline const ParamDef* findDef (Role r, const juce::String& id)
{
    for (auto& d : paramDefs (r)) if (id == d.id) return &d;
    return nullptr;
}
inline float numOf (Role r, const juce::ValueTree& t, const char* id) { auto* d = findDef (r, id); return num (t, id, d != nullptr ? d->def : 0.0f); }
inline juce::Colour colOf (Role r, const juce::ValueTree& t, const char* id) { auto* d = findDef (r, id); return col (t, id, d != nullptr ? d->colourDef : 0xff808080); }

//==============================================================================
struct WheelStyle
{
    int body, texture, dents, pointer, rail, bigSteps, smallSteps, cap;
    juce::Colour bodyColour, fill, pressed, railColour, capColour;
    float size, shadow, dentDepth, dentWidth, pointerWidth, capSize, capShadow;
    bool pointerOnCap, fillRail, fillRuler, reflection;
    static WheelStyle from (const juce::ValueTree& t)
    {
        const auto R = Role::wheel; WheelStyle s;
        s.body = (int) numOf (R, t, "body"); s.texture = (int) numOf (R, t, "texture"); s.dents = (int) numOf (R, t, "dents");
        s.pointer = (int) numOf (R, t, "pointer"); s.rail = (int) numOf (R, t, "rail"); s.bigSteps = std::max (1, (int) numOf (R, t, "bigSteps"));
        s.smallSteps = std::max (0, (int) numOf (R, t, "smallSteps")); s.cap = (int) numOf (R, t, "cap");
        s.bodyColour = colOf (R, t, "bodyColour"); s.fill = colOf (R, t, "fill"); s.pressed = colOf (R, t, "pressed");
        s.railColour = colOf (R, t, "railColour"); s.capColour = colOf (R, t, "capColour");
        s.size = numOf (R, t, "size"); s.shadow = numOf (R, t, "shadow"); s.dentDepth = numOf (R, t, "dentDepth"); s.dentWidth = numOf (R, t, "dentWidth");
        s.pointerWidth = numOf (R, t, "pointerWidth"); s.capSize = numOf (R, t, "capSize"); s.capShadow = numOf (R, t, "capShadow");
        s.pointerOnCap = numOf (R, t, "pointerOnCap") > 0.5f; s.fillRail = numOf (R, t, "fillRail") > 0.5f;
        s.fillRuler = numOf (R, t, "fillRuler") > 0.5f; s.reflection = numOf (R, t, "reflection") > 0.5f;
        return s;
    }
};
struct SliderStyle
{
    int ruler, bigSteps, smallSteps, cap;
    juce::Colour railColour, rulerColour, fill, capColour, lineColour;
    float railWidth, capWidth, capHeight, gloss, shadow;
    bool fillRail;
    static SliderStyle from (const juce::ValueTree& t)
    {
        const auto R = Role::slider; SliderStyle s;
        s.ruler = (int) numOf (R, t, "ruler"); s.bigSteps = std::max (1, (int) numOf (R, t, "bigSteps")); s.smallSteps = std::max (0, (int) numOf (R, t, "smallSteps"));
        s.cap = (int) numOf (R, t, "cap");
        s.railColour = colOf (R, t, "railColour"); s.rulerColour = colOf (R, t, "rulerColour"); s.fill = colOf (R, t, "fill");
        s.capColour = colOf (R, t, "capColour"); s.lineColour = colOf (R, t, "lineColour");
        s.railWidth = numOf (R, t, "railWidth"); s.capWidth = numOf (R, t, "capWidth"); s.capHeight = numOf (R, t, "capHeight");
        s.gloss = numOf (R, t, "gloss"); s.shadow = numOf (R, t, "shadow"); s.fillRail = numOf (R, t, "fillRail") > 0.5f;
        return s;
    }
};
struct ButtonStyle
{
    int shape, texture;
    juce::Colour colour, onColour, border, text, textOn;
    float corner, roundness, bevel, glow, shadow;
    static ButtonStyle from (const juce::ValueTree& t)
    {
        const auto R = Role::button; ButtonStyle s;
        s.shape = (int) numOf (R, t, "shape"); s.texture = (int) numOf (R, t, "texture");
        s.colour = colOf (R, t, "colour"); s.onColour = colOf (R, t, "onColour"); s.border = colOf (R, t, "border");
        s.text = colOf (R, t, "text"); s.textOn = colOf (R, t, "textOn");
        s.corner = numOf (R, t, "corner"); s.roundness = numOf (R, t, "roundness"); s.bevel = numOf (R, t, "bevel");
        s.glow = numOf (R, t, "glow"); s.shadow = numOf (R, t, "shadow");
        return s;
    }
};

//==============================================================================
/** The DAW's current look (one style per role). Changing it broadcasts, and the main window repaints everything. */
class Theme : public juce::ChangeBroadcaster
{
public:
    static Theme& get() { static Theme t; return t; }

    const WheelStyle& wheel() const { return wheelS; }
    const SliderStyle& slider() const { return sliderS; }
    const ButtonStyle& button() const { return buttonS; }
    juce::ValueTree style (Role r) const { return r == Role::wheel ? wheelT : r == Role::slider ? sliderT : buttonT; }

    void setStyle (Role r, const juce::ValueTree& t, bool save = true)
    {
        auto c = t.createCopy();
        if (r == Role::wheel) { wheelT = c; wheelS = WheelStyle::from (c); }
        else if (r == Role::slider) { sliderT = c; sliderS = SliderStyle::from (c); }
        else { buttonT = c; buttonS = ButtonStyle::from (c); }
        if (save) saveToFile();
        sendChangeMessage();
    }
    void resetAll() { for (auto r : { Role::wheel, Role::slider, Role::button }) setStyle (r, defaultStyle (r), false); saveToFile(); }

    void setFile (const juce::File& f)
    {
        file = f;
        if (auto x = juce::XmlDocument::parse (file))
        {
            auto all = juce::ValueTree::fromXml (*x);
            for (auto r : { Role::wheel, Role::slider, Role::button })
            {
                auto t = all.getChildWithName (roleName (r));
                if (t.isValid()) setStyle (r, t, false);
            }
        }
    }
    juce::ValueTree toTree() const
    {
        juce::ValueTree all ("VegaLook");
        all.appendChild (wheelT.createCopy(), nullptr); all.appendChild (sliderT.createCopy(), nullptr); all.appendChild (buttonT.createCopy(), nullptr);
        return all;
    }

private:
    Theme() { for (auto r : { Role::wheel, Role::slider, Role::button }) setStyle (r, defaultStyle (r), false); }
    void saveToFile() { if (file != juce::File()) if (auto x = toTree().createXml()) x->writeTo (file); }
    juce::ValueTree wheelT, sliderT, buttonT;
    WheelStyle wheelS; SliderStyle sliderS; ButtonStyle buttonS;
    juce::File file;
};

//==============================================================================
//  Painters
//==============================================================================
static constexpr float wheelStart = -2.36f, wheelEnd = 2.36f;   // the arc every wheel uses (7 o'clock to 5 o'clock)

inline void dropShadow (juce::Graphics& g, const juce::Path& p, float amount, float size)
{
    if (amount <= 0.01f) return;
    juce::DropShadow (juce::Colours::black.withAlpha (juce::jlimit (0.0f, 0.9f, 0.85f * amount)),
                      std::max (1, juce::roundToInt (size * amount)), { 0, juce::roundToInt (size * 0.28f * amount) }).drawForPath (g, p);
}

/** The body outline of a wheel: a circle, or with dents / teeth / knurling cut into its edge */
inline juce::Path wheelBodyPath (juce::Point<float> c, float r, const WheelStyle& s)
{
    int n = s.dents; float depth = s.dentDepth, width = s.dentWidth; bool square = false;
    if (s.body == 1) { n = std::max (n, 12); square = true; depth = depth == 0 ? 0.08f : depth; }    // gear
    else if (s.body == 2) { n = std::max (n, 36); depth = 0.035f; width = 0.5f; }                     // knurled
    else if (s.body == 3) { n = std::max (n, 8); depth = std::abs (depth) < 0.06f ? 0.12f : depth; width = 0.75f; }   // scalloped
    juce::Path p;
    if (n <= 0 || std::abs (depth) < 0.001f) { p.addEllipse (c.x - r, c.y - r, r * 2, r * 2); return p; }
    const int steps = std::max (180, n * 12);
    for (int i = 0; i <= steps; ++i)
    {
        const float th = juce::MathConstants<float>::twoPi * (float) i / (float) steps;
        const float phase = std::fmod (th * (float) n / juce::MathConstants<float>::twoPi + 0.5f, 1.0f);
        const float d = std::abs (phase - 0.5f) / std::max (0.01f, width * 0.5f);
        const float bump = d < 1.0f ? depth * (square ? 1.0f : (1.0f - d * d)) : 0.0f;
        const float rr = r * (1.0f - bump);
        const juce::Point<float> pt (c.x + rr * std::sin (th), c.y - rr * std::cos (th));
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    p.closeSubPath();
    return p;
}

/** A wheel (knob). `angle` is the pointer angle (radians, 0 = up); `value` (0-1, or negative = derive from the angle)
    fills the rail / ruler. `tint` overrides the fill colour (e.g. EQ band colours). */
inline void paintWheel (juce::Graphics& g, juce::Rectangle<float> area, float angle, float value = -1.0f,
                        bool hover = false, bool pressed = false, juce::Colour tint = {}, const WheelStyle* style = nullptr)
{
    const auto& s = style != nullptr ? *style : Theme::get().wheel();
    const auto c = area.getCentre();
    const float R = std::min (area.getWidth(), area.getHeight()) * 0.5f;
    if (R < 2.0f) return;
    const float r = R * s.size;
    if (value < 0.0f) value = juce::jlimit (0.0f, 1.0f, (angle - wheelStart) / (wheelEnd - wheelStart));
    const float vAngle = wheelStart + value * (wheelEnd - wheelStart);
    const auto fill = ! tint.isTransparent() ? tint : (pressed ? s.pressed : s.fill);
    const float gap = std::max (2.5f, R - r);
    auto polar = [c] (float rad, float a) { return juce::Point<float> (c.x + rad * std::sin (a), c.y - rad * std::cos (a)); };

    // ---- rail / ruler outside the body
    const int total = s.bigSteps * (s.smallSteps + 1);
    auto tickAngle = [&] (int i) { return wheelStart + (float) i / (float) total * (wheelEnd - wheelStart); };
    if (s.rail == 1)            // arc
    {
        const float rr = r + gap * 0.55f, w = std::max (1.5f, gap * 0.38f);
        juce::Path arc; arc.addCentredArc (c.x, c.y, rr, rr, 0, wheelStart, wheelEnd, true);
        g.setColour (s.railColour); g.strokePath (arc, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (s.fillRail && value > 0.002f)
        {
            juce::Path on; on.addCentredArc (c.x, c.y, rr, rr, 0, wheelStart, vAngle, true);
            g.setColour (fill); g.strokePath (on, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else if (s.rail == 2 || s.rail == 4)    // ruler outside / dots
    {
        for (int i = 0; i <= total; ++i)
        {
            const float a = tickAngle (i);
            const bool big = i % (s.smallSteps + 1) == 0;
            const bool lit = s.fillRuler && a <= vAngle + 0.001f && value > 0.002f;
            g.setColour (lit ? fill : s.railColour.brighter (0.9f).withAlpha (big ? 0.9f : 0.55f));
            if (s.rail == 4)
            {
                const float dr = std::max (0.8f, gap * (big ? 0.16f : 0.1f));
                g.fillEllipse (juce::Rectangle<float> (dr * 2, dr * 2).withCentre (polar (r + gap * 0.55f, a)));
            }
            else
                g.drawLine (juce::Line<float> (polar (r + gap * 0.22f, a), polar (big ? R : r + gap * 0.65f, a)), big ? 1.4f : 1.0f);
        }
    }

    // ---- body
    auto body = wheelBodyPath (c, r, s);
    dropShadow (g, body, s.shadow, r * 0.55f);
    {
        const auto base = s.bodyColour;
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (body);
        if (s.texture == 0)
            g.setGradientFill (juce::ColourGradient (base.brighter (0.25f), c.x - r * 0.35f, c.y - r * 0.4f, base.darker (0.45f), c.x + r * 0.6f, c.y + r * 0.8f, true));
        else
            g.setGradientFill (juce::ColourGradient (base.brighter (s.texture == 2 ? 0.55f : 0.35f), c.x, c.y - r, base.darker (0.55f), c.x, c.y + r, false));
        g.fillPath (body);
        if (s.texture == 3)
        {   // brushed metal: fine radial streaks
            for (int k = 0; k < 90; ++k)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) k / 90.0f;
                g.setColour ((k % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (k % 3 == 0 ? 0.06f : 0.03f));
                g.drawLine (juce::Line<float> (c, polar (r, a)), 1.0f);
            }
        }
        if (s.texture >= 1 && s.texture <= 2)
        {   // gloss: a soft light band across the top
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (s.texture == 2 ? 0.22f : 0.09f), c.x, c.y - r, juce::Colours::transparentWhite, c.x, c.y, false));
            g.fillEllipse (c.x - r * 0.92f, c.y - r * 0.98f, r * 1.84f, r * 1.2f);
        }
    }
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.strokePath (body, juce::PathStrokeType (1.0f));
    {
        juce::Path rim; rim.addCentredArc (c.x, c.y, r - 1.0f, r - 1.0f, 0, -1.2f, 1.2f, true);
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.strokePath (rim, juce::PathStrokeType (1.0f));
    }
    if (s.rail == 3)    // ruler printed on the body's edge
        for (int i = 0; i <= total; ++i)
        {
            const float a = tickAngle (i);
            const bool big = i % (s.smallSteps + 1) == 0;
            const bool lit = s.fillRuler && a <= vAngle + 0.001f && value > 0.002f;
            g.setColour (lit ? fill : juce::Colours::white.withAlpha (big ? 0.55f : 0.3f));
            g.drawLine (juce::Line<float> (polar (r * (big ? 0.76f : 0.84f), a), polar (r * 0.93f, a)), big ? 1.3f : 0.9f);
        }

    // ---- cap
    const float rc = r * s.capSize;
    if (s.cap != 0)
    {
        juce::Path capP; capP.addEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
        dropShadow (g, capP, s.capShadow, rc * 0.6f);
        const auto cc = s.capColour;
        switch (s.cap)
        {
            case 1: g.setColour (cc); break;
            case 2: g.setGradientFill (juce::ColourGradient (cc.brighter (0.55f), c.x - rc * 0.35f, c.y - rc * 0.45f, cc.darker (0.5f), c.x + rc * 0.5f, c.y + rc * 0.75f, true)); break;
            case 3: g.setGradientFill (juce::ColourGradient (cc.darker (0.45f), c.x, c.y - rc, cc.brighter (0.4f), c.x, c.y + rc, false)); break;
            default: g.setGradientFill (juce::ColourGradient (cc.brighter (0.6f), c.x, c.y - rc, cc.darker (0.3f), c.x, c.y + rc, false)); break;
        }
        g.fillPath (capP);
        if (s.cap == 4)
        {   // chrome: alternating light / dark sectors like a machined cap
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (capP);
            for (int k = 0; k < 16; ++k)
            {
                juce::Path w;
                w.addPieSegment (c.x - rc, c.y - rc, rc * 2, rc * 2, k * juce::MathConstants<float>::pi / 8.0f, (k + 1) * juce::MathConstants<float>::pi / 8.0f, 0.0f);
                const float t = std::sin ((float) k * 0.7854f) * 0.5f + 0.5f;
                g.setColour ((t > 0.5f ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f + 0.12f * std::abs (t - 0.5f)));
                g.fillPath (w);
            }
        }
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (s.cap == 3 ? 0.18f : 0.08f)); g.drawEllipse (c.x - rc + 1, c.y - rc + 1, rc * 2 - 2, rc * 2 - 2, 0.8f);
    }
    if (s.reflection)
    {
        const float rr = s.cap != 0 ? rc : r * 0.85f;
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), c.x, c.y - rr, juce::Colours::transparentWhite, c.x, c.y - rr * 0.1f, false));
        g.fillEllipse (c.x - rr * 0.62f, c.y - rr * 0.92f, rr * 1.24f, rr * 0.8f);
    }

    // ---- pointer
    const float pw = std::max (1.2f, r * 0.075f * s.pointerWidth);
    g.setColour (fill);
    const bool onCap = s.pointerOnCap && s.cap != 0;
    switch (s.pointer)
    {
        case 1:  // short line
            if (onCap) g.drawLine (juce::Line<float> (polar (rc * 0.35f, angle), polar (rc * 0.88f, angle)), pw);
            else g.drawLine (juce::Line<float> (polar (s.cap != 0 ? std::min (r * 0.9f, rc * 1.06f) : r * 0.45f, angle), polar (r * 0.93f, angle)), pw);
            break;
        case 2:  // long line
            g.drawLine (juce::Line<float> (polar (onCap ? rc * 0.05f : r * 0.08f, angle), polar (onCap ? rc * 0.9f : r * 0.93f, angle)), pw);
            break;
        case 3:  // dot
        {
            const float dr = std::max (1.6f, r * 0.09f * s.pointerWidth);
            g.fillEllipse (juce::Rectangle<float> (dr * 2, dr * 2).withCentre (polar (onCap ? rc * 0.62f : (s.cap != 0 ? (rc + r) * 0.5f : r * 0.7f), angle)));
            break;
        }
        case 4:  // triangle
        {
            const float tip = onCap ? rc * 0.92f : r * 0.95f, base = tip - std::max (4.0f, r * 0.22f), hw = std::max (2.0f, r * 0.09f * s.pointerWidth);
            juce::Path t;
            t.addTriangle (polar (tip, angle), polar (base, angle) + juce::Point<float> (std::cos (angle) * hw, std::sin (angle) * hw),
                           polar (base, angle) - juce::Point<float> (std::cos (angle) * hw, std::sin (angle) * hw));
            g.fillPath (t);
            break;
        }
        default: break;
    }
    if (hover) { g.setColour (fill.withAlpha (0.22f)); g.strokePath (body, juce::PathStrokeType (1.6f)); }
}

/** A fader. `capPos` is the cap's centre (y for vertical, x for horizontal) in the same coordinates as `area`;
    the slot runs the full length of `area`. */
inline void paintSlider (juce::Graphics& g, juce::Rectangle<float> area, float capPos, bool vertical = true,
                         bool hover = false, bool selected = false, juce::Colour tint = {}, const SliderStyle* style = nullptr,
                         float capWOverride = 0.0f, float capHOverride = 0.0f)
{
    const auto& s = style != nullptr ? *style : Theme::get().slider();
    juce::Graphics::ScopedSaveState ss (g);
    if (! vertical)
    {   // draw it as a vertical fader turned clockwise: the right end is the top
        const auto c = area.getCentre();
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi, c.x, c.y));
        area = juce::Rectangle<float> (area.getHeight(), area.getWidth()).withCentre (c);
        capPos = c.y + c.x - capPos;
    }
    const auto accent = ! tint.isTransparent() ? tint : s.fill;
    const float cx = area.getCentreX();
    const float capW = std::min (capWOverride > 0 ? capWOverride : s.capWidth, area.getWidth() - 2.0f);
    const float capH = capHOverride > 0 ? capHOverride : s.capHeight;
    const float top = area.getY() + capH * 0.5f, bottom = area.getBottom() - capH * 0.5f;
    // ruler
    if (s.ruler != 0)
    {
        const int total = s.bigSteps * (s.smallSteps + 1);
        const float half = capW * 0.5f + 3.0f;
        for (int i = 0; i <= total; ++i)
        {
            const float y = top + (bottom - top) * (float) i / (float) total;
            const bool big = i % (s.smallSteps + 1) == 0;
            const float len = big ? 8.0f : 4.5f;
            g.setColour (s.rulerColour.withAlpha (big ? 0.85f : 0.45f));
            if (s.ruler == 1 || s.ruler == 2) g.drawLine (cx - half - len, y, cx - half, y, 1.0f);
            if (s.ruler == 1 || s.ruler == 3) g.drawLine (cx + half, y, cx + half + len, y, 1.0f);
        }
    }
    // rail
    auto rail = juce::Rectangle<float> (s.railWidth, area.getHeight() - 4.0f).withCentre ({ cx, area.getCentreY() });
    g.setColour (s.railColour); g.fillRoundedRectangle (rail, s.railWidth * 0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (rail.expanded (0.5f), s.railWidth * 0.5f, 0.8f);
    if (s.fillRail || selected)
    {
        auto filled = rail.withTop (juce::jlimit (rail.getY(), rail.getBottom(), capPos));
        g.setColour (accent.withAlpha (s.fillRail ? 0.9f : 0.35f)); g.fillRoundedRectangle (filled.reduced (std::max (0.0f, s.railWidth * 0.15f), 0), s.railWidth * 0.4f);
    }
    // cap
    auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ cx, capPos });
    const float corner = std::min (3.0f, capH * 0.25f);
    juce::Path capP; capP.addRoundedRectangle (cap, corner);
    dropShadow (g, capP, s.shadow, capH * 0.6f);
    const auto cc = s.capColour;
    const auto lineCol = selected ? accent : hover ? accent.withAlpha (0.85f) : s.lineColour;
    if (s.cap == 3 || (selected && s.cap == 3))
    {   // lit cap
        g.setColour (accent.withAlpha (0.3f)); g.fillRoundedRectangle (cap.expanded (3.0f), corner + 2);
        g.setGradientFill (juce::ColourGradient (accent.brighter (0.4f), cx, cap.getY(), accent.darker (0.5f), cx, cap.getBottom(), false));
        g.fillPath (capP);
        g.setColour (accent.darker (0.9f)); g.drawLine (cap.getX() + 3, cap.getCentreY(), cap.getRight() - 3, cap.getCentreY(), 1.6f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.strokePath (capP, juce::PathStrokeType (1.0f));
        return;
    }
    if (s.cap == 4)
    {   // hardware: scooped silver cap with grip ridges
        juce::ColourGradient body (cc.brighter (0.6f), cap.getX(), cap.getY(), cc.darker (0.6f), cap.getX(), cap.getBottom(), false);
        body.addColour (0.5, cc);
        g.setGradientFill (body); g.fillPath (capP);
        auto scoop = cap.withHeight (capH * 0.42f).reduced (3.0f, 2.0f);
        g.setGradientFill (juce::ColourGradient (cc.darker (0.5f), 0, scoop.getY(), cc.brighter (0.25f), 0, scoop.getBottom(), false));
        g.fillRoundedRectangle (scoop, 2.0f);
        for (int k = 0; k < 3; ++k)
        {
            const float y = cap.getY() + capH * (0.66f + k * 0.1f);
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawLine (cap.getX() + 4, y, cap.getRight() - 4, y, 1.0f);
        }
    }
    else
    {
        if (s.cap == 1) g.setColour (cc);
        else g.setGradientFill (juce::ColourGradient (cc.brighter (0.15f + 0.55f * s.gloss), cx, cap.getY(), cc.darker (0.25f + 0.35f * s.gloss), cx, cap.getBottom(), false));
        g.fillPath (capP);
        if (s.cap != 1 && s.gloss > 0.05f)
        {   // glossy top half
            g.setColour (juce::Colours::white.withAlpha (0.12f * s.gloss));
            g.fillRoundedRectangle (cap.withHeight (capH * 0.45f).reduced (1.5f, 1.0f), corner);
        }
        if (s.cap == 2)
            for (int k = -2; k <= 2; ++k)
            {
                if (k == 0) continue;
                const float y = cap.getCentreY() + k * capH * 0.16f;
                g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawLine (cap.getX() + 4, y, cap.getRight() - 4, y, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawLine (cap.getX() + 4, y + 1, cap.getRight() - 4, y + 1, 0.8f);
            }
    }
    if (selected) { g.setColour (accent.withAlpha (0.25f)); g.drawRoundedRectangle (cap.expanded (2.0f), corner + 1.5f, 2.0f); }
    // the value line(s): classic caps have a double line like a real fader cap
    g.setColour (lineCol);
    if (s.cap == 0) { g.drawLine (cap.getX() + 3, cap.getCentreY() - 1.5f, cap.getRight() - 3, cap.getCentreY() - 1.5f, 1.2f); g.drawLine (cap.getX() + 3, cap.getCentreY() + 1.5f, cap.getRight() - 3, cap.getCentreY() + 1.5f, 1.2f); }
    else g.fillRect (cap.getX() + 2.5f, cap.getCentreY() - 1.0f, capW - 5.0f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (capP, juce::PathStrokeType (1.0f));
}

/** Button outline: rounded, pillow (a soft "squircle", like hardware rubber caps) or square */
inline juce::Path buttonPath (juce::Rectangle<float> r, const ButtonStyle& s)
{
    juce::Path p;
    if (s.shape == 2) { p.addRoundedRectangle (r, 1.5f); return p; }
    if (s.shape == 0) { p.addRoundedRectangle (r, std::min (s.corner, std::min (r.getWidth(), r.getHeight()) * 0.5f)); return p; }
    // superellipse: |x/a|^n + |y/b|^n = 1, with straight sides on long buttons
    const float n = s.roundness;
    const float h = r.getHeight(), w = r.getWidth();
    const float a = std::min (w, h) * 0.5f;
    const float extraX = std::max (0.0f, w * 0.5f - a), extraY = std::max (0.0f, h * 0.5f - a);
    const auto c = r.getCentre();
    const int steps = 96;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = juce::MathConstants<float>::twoPi * (float) i / (float) steps;
        const float ct = std::cos (t), st = std::sin (t);
        float x = a * (ct < 0 ? -1.0f : 1.0f) * std::pow (std::abs (ct), 2.0f / n);
        float y = a * (st < 0 ? -1.0f : 1.0f) * std::pow (std::abs (st), 2.0f / n);
        x += (x < 0 ? -extraX : extraX); y += (y < 0 ? -extraY : extraY);
        if (i == 0) p.startNewSubPath (c.x + x, c.y + y); else p.lineTo (c.x + x, c.y + y);
    }
    p.closeSubPath();
    return p;
}

/** A push button / switch. `lit` overrides the style's lit colour. Returns the face (for icons / text). */
inline juce::Rectangle<float> paintButton (juce::Graphics& g, juce::Rectangle<float> area, bool on, bool hover = false, bool down = false,
                                          juce::Colour lit = {}, const ButtonStyle* style = nullptr)
{
    const auto& s = style != nullptr ? *style : Theme::get().button();
    auto r = area.reduced (1.0f);
    if (down) r = r.translated (0, 0.6f);
    auto p = buttonPath (r, s);
    const auto litC = ! lit.isTransparent() ? lit : s.onColour;
    if (on && s.glow > 0.01f)
        juce::DropShadow (litC.withAlpha (0.55f * s.glow), juce::roundToInt (4 + 10 * s.glow), {}).drawForPath (g, p);
    else
        dropShadow (g, p, s.shadow, std::min (r.getHeight(), 14.0f));
    const auto c = r.getCentre();
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (litC.brighter (0.65f), c.x, c.y - r.getHeight() * 0.12f, litC.darker (0.12f), r.getX(), r.getBottom(), true));
        g.fillPath (p);
        g.setColour (litC.darker (0.45f)); g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    else
    {
        auto base = s.colour;
        if (hover) base = base.brighter (0.1f);
        if (down) base = base.darker (0.2f);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.12f + 0.3f * s.bevel), c.x, r.getY(), base.darker (0.1f + 0.35f * s.bevel), c.x, r.getBottom(), false));
        g.fillPath (p);
        if (s.texture > 0)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (p);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (s.texture == 2 ? 0.16f : 0.07f), c.x, r.getY(), juce::Colours::transparentWhite, c.x, c.y + 1, false));
            g.fillRect (r.withHeight (r.getHeight() * 0.5f));
        }
        g.setColour (s.border); g.strokePath (p, juce::PathStrokeType (1.0f));
        if (s.bevel > 0.05f)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (p);
            g.setColour (juce::Colours::white.withAlpha (0.10f * s.bevel));
            g.strokePath (p, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0, 1.0f));
        }
    }
    return r.reduced (std::min (4.0f, std::min (r.getWidth(), r.getHeight()) * 0.14f));
}

/** Text colour for a button's label */
inline juce::Colour buttonText (bool on, juce::Colour lit = {}, const ButtonStyle* style = nullptr)
{
    const auto& s = style != nullptr ? *style : Theme::get().button();
    if (! on) return s.text;
    const auto litC = ! lit.isTransparent() ? lit : s.onColour;
    return litC.getPerceivedBrightness() > 0.5f ? s.textOn : juce::Colours::white;
}

//==============================================================================
//  Presets (built in)
//==============================================================================
struct Preset { juce::String name; juce::ValueTree style; };

inline juce::ValueTree with (Role r, std::initializer_list<std::pair<const char*, juce::var>> changes)
{
    auto t = defaultStyle (r);
    for (auto& [k, v] : changes) t.setProperty (k, v, nullptr);
    return t;
}

inline std::vector<Preset> builtInPresets (Role r)
{
    std::vector<Preset> p;
    auto add = [&] (const char* n, std::initializer_list<std::pair<const char*, juce::var>> ch) { p.push_back ({ n, with (r, ch) }); };
    if (r == Role::wheel)
    {
        add ("Vega Slate", {});
        add ("Orange Ruler", { { "rail", 2 }, { "fill", "ffe8913a" }, { "pointer", 1 }, { "cap", 4 }, { "body", 0 } });
        add ("Gear Chrome", { { "body", 1 }, { "dents", 16 }, { "cap", 4 }, { "capColour", "ff6c7479" }, { "rail", 2 }, { "fill", "ff5cb6c9" } });
        add ("Studio Black", { { "bodyColour", "ff1c1c1c" }, { "capColour", "ff262626" }, { "cap", 1 }, { "fill", "ffffffff" }, { "rail", 0 }, { "reflection", 0 } });
        add ("Mint Arc", { { "fill", "ff3ee0b0" }, { "pointer", 3 }, { "cap", 2 }, { "capColour", "ff2a2f33" } });
        add ("Hot Red", { { "fill", "ffff4a3a" }, { "bodyColour", "ff2a2a2a" }, { "rail", 1 }, { "cap", 3 }, { "capColour", "ff333333" } });
        add ("Purple Glow", { { "fill", "ffb46cff" }, { "texture", 2 }, { "cap", 2 }, { "capColour", "ff3a3340" } });
        add ("Gold Knurl", { { "body", 2 }, { "texture", 3 }, { "bodyColour", "ff6a6a6a" }, { "fill", "ffd9b45a" }, { "cap", 1 }, { "capColour", "ff2a2a2a" } });
        add ("White Dome", { { "bodyColour", "ffcfd4d8" }, { "capColour", "ffe8ecef" }, { "fill", "ff3f484d" }, { "texture", 2 }, { "pressed", "ff1a1a1a" } });
        add ("Dots Lime", { { "rail", 4 }, { "fillRuler", 1 }, { "fill", "ff9be35a" }, { "smallSteps", 1 }, { "bigSteps", 8 } });
        add ("Scalloped Blue", { { "body", 3 }, { "fill", "ff3c8dff" }, { "cap", 2 }, { "capColour", "ff2b3138" } });
        add ("Tiny Dot", { { "pointer", 3 }, { "cap", 0 }, { "rail", 0 }, { "fill", "ffffc23a" } });
        add ("Inner Ruler", { { "rail", 3 }, { "cap", 2 }, { "capSize", 0.55f }, { "fill", "ff8cc8ee" } });
        add ("Triangle Teal", { { "pointer", 4 }, { "pointerOnCap", 1 }, { "fill", "ff2ed4c4" }, { "cap", 2 } });
        add ("Metal Flat", { { "texture", 3 }, { "bodyColour", "ff7a8287" }, { "cap", 0 }, { "pointer", 2 }, { "fill", "ff111111" }, { "rail", 2 } });
        add ("Pink Long", { { "pointer", 2 }, { "fill", "ffff5fa2" }, { "cap", 0 }, { "texture", 2 } });
    }
    else if (r == Role::slider)
    {
        add ("Vega Slate", {});
        add ("Flat Line", { { "cap", 1 }, { "capColour", "ff2c3236" }, { "ruler", 2 } });
        add ("Grip", { { "cap", 2 }, { "capHeight", 26.0f } });
        add ("Lit Blue", { { "cap", 3 }, { "fill", "ff8cc8ee" } });
        add ("Hardware", { { "cap", 4 }, { "capColour", "ff8a8f93" }, { "capWidth", 34.0f }, { "capHeight", 40.0f } });
        add ("Filled Rail", { { "fillRail", 1 }, { "fill", "ff3ee0b0" }, { "railWidth", 6.0f } });
        add ("Slim", { { "railWidth", 2.0f }, { "capWidth", 22.0f }, { "capHeight", 12.0f }, { "ruler", 0 } });
        add ("Orange", { { "fillRail", 1 }, { "fill", "ffe8913a" }, { "cap", 0 }, { "capColour", "ff3a3f43" } });
    }
    else
    {
        add ("Vega Slate", {});
        add ("Rounded", { { "shape", 0 }, { "corner", 6.0f } });
        add ("Square", { { "shape", 2 }, { "texture", 0 } });
        add ("Mint", { { "onColour", "ff3ee0b0" } });
        add ("Orange", { { "onColour", "ffffa33a" } });
        add ("Dark Gloss", { { "colour", "ff222629" }, { "texture", 2 } });
        add ("Soft Pillow", { { "roundness", 3.0f }, { "bevel", 0.8f } });
        add ("Flat", { { "texture", 0 }, { "bevel", 0.0f }, { "shadow", 0.0f }, { "glow", 0.3f } });
    }
    return p;
}

/** A random but sensible style (Randomize) */
inline juce::ValueTree randomStyle (Role r, juce::Random& rnd)
{
    auto t = defaultStyle (r);
    for (auto& d : paramDefs (r))
    {
        switch (d.type)
        {
            case ParamDef::choice:  t.setProperty (d.id, rnd.nextInt ((int) d.options.size()), nullptr); break;
            case ParamDef::integer: t.setProperty (d.id, (int) d.lo + rnd.nextInt ((int) (d.hi - d.lo) + 1), nullptr); break;
            case ParamDef::toggle:  t.setProperty (d.id, rnd.nextBool() ? 1 : 0, nullptr); break;
            case ParamDef::number:  t.setProperty (d.id, d.lo + rnd.nextFloat() * (d.hi - d.lo), nullptr); break;
            case ParamDef::colour:
            {
                const juce::String id (d.id);
                const bool accent = id.containsIgnoreCase ("fill") || id.containsIgnoreCase ("onColour") || id == "pressed";
                const auto c = accent ? juce::Colour::fromHSV (rnd.nextFloat(), 0.55f + rnd.nextFloat() * 0.4f, 0.85f + rnd.nextFloat() * 0.15f, 1.0f)
                                      : juce::Colour::fromHSV (rnd.nextFloat(), rnd.nextFloat() * 0.2f, 0.12f + rnd.nextFloat() * 0.35f, 1.0f);
                if (id == "text" || id == "lineColour") break;     // keep labels readable
                t.setProperty (d.id, c.toString(), nullptr);
                break;
            }
        }
    }
    if (r == Role::wheel) { t.setProperty ("size", 0.7f + rnd.nextFloat() * 0.25f, nullptr); t.setProperty ("dents", rnd.nextBool() ? 0 : rnd.nextInt (24), nullptr); }
    return t;
}
} // namespace vegacontrols
