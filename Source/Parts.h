#pragma once
#include <JuceHeader.h>
#include "Ui.h"
#include "FxCatalog.h"

// Procedural machine-part foundry.
// Every effect you place becomes ONE recognisable hardware part in the Viewer, chosen
// deterministically from its FX id (so the same effect always grows the same part and
// people can learn "these three build that machine"). The five numeric values on each
// effect (Amount / Tone / Motion / Shape / Mix) decide what detail the part can grow:
// how many lights light, how fast it moves, which vents/coils/pipes/decals get added.
// Live audio level animates screens, speakers, needles and lamps in real time.
namespace parts
{
// --------------------------------------------------------------------- deterministic rng
struct Rng
{
    juce::uint32 s;
    explicit Rng (juce::uint32 seed) : s (seed ? seed : 0x9e3779b9u) {}
    juce::uint32 u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (float) (u() >> 8) * (1.0f / 16777216.0f); }
    float range (float a, float b) { return a + (b - a) * f(); }
    int ri (int a, int b) { return a + (int) (u() % (juce::uint32) juce::jmax (1, b - a + 1)); }
    bool chance (float p) { return f() < p; }
};

inline juce::uint32 hashString (const juce::String& s)
{
    juce::uint32 h = 2166136261u;
    for (auto c : s) { h ^= (juce::uint32) c; h *= 16777619u; }
    return h;
}

// The hero part an effect becomes.
enum Hero { Scope, Spectrum, Speaker, ReelDeck, CoilRack, CrystalTube, VuGauge, LampBank, Switchboard, PipeWerk, Grille, DataScreen, NumHeroes };

inline const char* heroName (int h)
{
    static const char* n[] = { "SCOPE", "SPECTRUM", "SPEAKER", "TAPE DECK", "COIL RACK", "VALVE", "VU GAUGE",
                               "LAMP BANK", "SWITCHBOARD", "PIPEWERK", "GRILLE", "DATA SCREEN" };
    return (h >= 0 && h < NumHeroes) ? n[h] : "PART";
}

// FX family -> candidate hero parts. The exact one is fixed per fx id (deterministic).
inline Hero heroFor (int fxIndex)
{
    const int fam = (fxIndex >= 0 && fxIndex < kt::kFxCount) ? kt::kFx[fxIndex].family : 7;
    static const std::vector<Hero> byFam[8] = {
        { ReelDeck, Scope, DataScreen },         // 0 Delay
        { Grille, Spectrum, Speaker },           // 1 Reverb
        { VuGauge, Scope, Switchboard },         // 2 Stereo
        { CoilRack, Scope, LampBank },           // 3 Modulation
        { Spectrum, Switchboard, DataScreen },   // 4 Filter/EQ
        { CrystalTube, LampBank, Grille },       // 5 Drive
        { VuGauge, LampBank, Speaker },          // 6 Dynamics
        { DataScreen, CrystalTube, PipeWerk }    // 7 Character
    };
    auto& v = byFam[juce::jlimit (0, 7, fam)];
    return v[(size_t) (((juce::uint32) (fxIndex + 1) * 2654435761u) % v.size())];
}

// --------------------------------------------------------------------- small helpers
inline void screw (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour edge)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (edge.withAlpha (0.8f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (c.x - r * 0.6f, c.y - r * 0.6f, c.x + r * 0.6f, c.y + r * 0.6f, 1.0f);
}

inline void engrave (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, juce::Colour c, float size, juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.setFont (ui::hud (size, false));
    g.drawText (t, r.translated (0.6f, 0.6f), j, true);
    g.setColour (c); g.drawText (t, r, j, true);
}

inline void recessed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour tint)
{
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(),
                                             tint.withAlpha (0.10f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.4f);
    g.setColour (juce::Colours::white.withAlpha (0.05f)); g.drawLine (r.getX() + 2, r.getBottom() - 1, r.getRight() - 2, r.getBottom() - 1, 1.0f);
}

// =====================================================================================
// DECAL LIBRARY — a big bank of little vector shapes stamped onto the chassis so each
// machine looks built, not flat. Grouped by "slot" so the generator can pick decals that
// suit the part. Hundreds of named variants resolve to a compact set of primitive drawers.
// =====================================================================================
enum Prim { PScrew, PBolt, PVent, PGrilleDots, PLed, PLabel, PPort, PChip, PCoilLine, PPipe,
            PBadge, PHazard, PRivetRow, PFuse, PToggle, PBarcode, PGaugeTick, PAntenna,
            PHeatsink, PWaveEtch, PNumPrims };

struct Decal { const char* name; juce::uint8 prim; juce::uint8 variant; juce::uint8 slot; };
// slot: 0 mount(corners)  1 vent/structure  2 label/badge  3 light  4 control  5 damage/flavour

inline constexpr Decal kDecals[] = {
    // --- mounts (0) ---
    {"screw.hex.s",PScrew,0,0},{"screw.hex.m",PScrew,1,0},{"screw.hex.l",PScrew,2,0},
    {"screw.phillips.s",PScrew,3,0},{"screw.phillips.m",PScrew,4,0},{"screw.flat.s",PScrew,5,0},
    {"bolt.hex.s",PBolt,0,0},{"bolt.hex.m",PBolt,1,0},{"bolt.square.m",PBolt,2,0},
    {"rivet.row.3",PRivetRow,0,0},{"rivet.row.4",PRivetRow,1,0},{"rivet.row.6",PRivetRow,2,0},
    {"rivet.col.3",PRivetRow,3,0},{"rivet.col.5",PRivetRow,4,0},
    // --- vents / structure (1) ---
    {"vent.slot.h",PVent,0,1},{"vent.slot.v",PVent,1,1},{"vent.slot.wide",PVent,2,1},
    {"vent.louvre",PVent,3,1},{"vent.round",PVent,4,1},{"vent.mesh",PVent,5,1},
    {"grille.dots.fine",PGrilleDots,0,1},{"grille.dots.med",PGrilleDots,1,1},{"grille.dots.coarse",PGrilleDots,2,1},
    {"grille.hex",PGrilleDots,3,1},{"grille.diag",PGrilleDots,4,1},
    {"heatsink.fins.s",PHeatsink,0,1},{"heatsink.fins.m",PHeatsink,1,1},{"heatsink.fins.l",PHeatsink,2,1},
    {"pipe.straight",PPipe,0,1},{"pipe.elbow",PPipe,1,1},{"pipe.twin",PPipe,2,1},{"pipe.valve",PPipe,3,1},
    {"coil.flat",PCoilLine,0,1},{"coil.tight",PCoilLine,1,1},{"coil.loose",PCoilLine,2,1},
    {"antenna.rod",PAntenna,0,1},{"antenna.dish",PAntenna,1,1},{"antenna.whip",PAntenna,2,1},
    // --- labels / badges (2) ---
    {"label.model",PLabel,0,2},{"label.serial",PLabel,1,2},{"label.rev",PLabel,2,2},
    {"label.caution",PLabel,3,2},{"label.input",PLabel,4,2},{"label.output",PLabel,5,2},
    {"label.gain",PLabel,6,2},{"label.tone",PLabel,7,2},{"label.mix",PLabel,8,2},
    {"badge.round",PBadge,0,2},{"badge.shield",PBadge,1,2},{"badge.star",PBadge,2,2},
    {"badge.plate",PBadge,3,2},{"badge.wing",PBadge,4,2},
    {"barcode.s",PBarcode,0,2},{"barcode.m",PBarcode,1,2},{"barcode.block",PBarcode,2,2},
    // --- lights (3) ---
    {"led.single",PLed,0,3},{"led.pair",PLed,1,3},{"led.tri",PLed,2,3},
    {"led.strip.4",PLed,3,3},{"led.strip.6",PLed,4,3},{"led.ring",PLed,5,3},
    {"fuse.glass",PFuse,0,3},{"fuse.ceramic",PFuse,1,3},
    // --- controls (4) ---
    {"port.jack",PPort,0,4},{"port.xlr",PPort,1,4},{"port.banana",PPort,2,4},{"port.din",PPort,3,4},
    {"chip.dip8",PChip,0,4},{"chip.dip14",PChip,1,4},{"chip.smd",PChip,2,4},
    {"toggle.up",PToggle,0,4},{"toggle.down",PToggle,1,4},{"toggle.guard",PToggle,2,4},
    {"gauge.ticks.s",PGaugeTick,0,4},{"gauge.ticks.m",PGaugeTick,1,4},{"gauge.ticks.l",PGaugeTick,2,4},
    // --- flavour / damage (5) ---
    {"hazard.stripe",PHazard,0,5},{"hazard.corner",PHazard,1,5},{"hazard.band",PHazard,2,5},
    {"wave.etch.sine",PWaveEtch,0,5},{"wave.etch.saw",PWaveEtch,1,5},{"wave.etch.sq",PWaveEtch,2,5},
};
inline constexpr int kDecalCount = (int) (sizeof (kDecals) / sizeof (kDecals[0]));

inline int decalsInSlot (int slot, int* out, int maxOut)
{
    int n = 0;
    for (int i = 0; i < kDecalCount && n < maxOut; ++i) if (kDecals[i].slot == slot) out[n++] = i;
    return n;
}

// -------- primitive decal drawer --------
inline void drawDecal (juce::Graphics& g, int idx, juce::Rectangle<float> r, juce::Colour col, float lit = 0.0f)
{
    if (idx < 0 || idx >= kDecalCount) return;
    const auto& d = kDecals[idx];
    const float v = (float) d.variant;
    switch (d.prim)
    {
        case PScrew: screw (g, r.getCentre(), juce::jmin (r.getWidth(), r.getHeight()) * 0.5f, col); break;
        case PBolt:
        {
            auto c = r.getCentre(); const float rr = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
            juce::Path hex; for (int i = 0; i < 6; ++i) { const float a = (float) i / 6.0f * juce::MathConstants<float>::twoPi; auto p = c.getPointOnCircumference (rr, a); (i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p)); }
            hex.closeSubPath(); g.setColour (col.withAlpha (0.6f)); g.strokePath (hex, juce::PathStrokeType (1.2f));
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (c.x - rr * 0.4f, c.y - rr * 0.4f, rr * 0.8f, rr * 0.8f);
            break;
        }
        case PVent:
        {
            const int lines = 3 + (int) v;
            const bool vert = ((int) v % 2) == 1;
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            for (int i = 0; i < lines; ++i)
            {
                if (vert) { const float x = r.getX() + r.getWidth() * (i + 0.5f) / lines; g.fillRect (x - 0.8f, r.getY(), 1.6f, r.getHeight()); }
                else      { const float y = r.getY() + r.getHeight() * (i + 0.5f) / lines; g.fillRect (r.getX(), y - 0.8f, r.getWidth(), 1.6f); }
            }
            g.setColour (col.withAlpha (0.15f)); g.drawRoundedRectangle (r, 2.0f, 1.0f);
            break;
        }
        case PGrilleDots:
        {
            const float step = 4.0f + v; g.setColour (juce::Colours::black.withAlpha (0.4f));
            for (float y = r.getY() + 2; y < r.getBottom() - 1; y += step)
                for (float x = r.getX() + 2 + (((int) (y) & 1) ? step * 0.5f : 0.0f); x < r.getRight() - 1; x += step)
                    g.fillEllipse (x, y, 1.6f, 1.6f);
            break;
        }
        case PLed:
        {
            const int cnt = d.variant <= 2 ? d.variant + 1 : (d.variant == 3 ? 4 : d.variant == 4 ? 6 : 8);
            const bool ring = d.variant == 5;
            if (ring) { g.setColour (col.withAlpha (0.3f + 0.6f * lit)); g.drawEllipse (r.reduced (1.0f), 1.6f); break; }
            const float w = r.getWidth() / cnt;
            for (int i = 0; i < cnt; ++i)
            {
                auto cell = juce::Rectangle<float> (r.getX() + i * w, r.getCentreY() - 2.0f, w - 2.0f, 4.0f);
                const bool on = ((float) (i + 1) / cnt) <= (0.25f + 0.75f * lit);
                g.setColour (on ? col.brighter (0.4f) : col.withAlpha (0.2f)); g.fillRoundedRectangle (cell, 1.5f);
            }
            break;
        }
        case PLabel:
        {
            static const char* words[] = { "MODEL KV-%d", "SER %04X", "REV %d", "! CAUTION", "IN", "OUT", "GAIN", "TONE", "MIX" };
            juce::String t = words[juce::jmin ((int) d.variant, 8)];
            if (t.contains ("%d")) t = t.replace ("%d", juce::String (10 + (int) col.getRed() % 90));
            if (t.contains ("%04X")) t = t.replace ("%04X", juce::String::toHexString ((int) col.getARGB() & 0xffff).paddedLeft ('0', 4).toUpperCase());
            engrave (g, r, t, col.withAlpha (0.8f), juce::jlimit (7.0f, 10.0f, r.getHeight() * 0.7f));
            break;
        }
        case PPort:
        {
            auto c = r.getCentre(); const float rr = juce::jmin (r.getWidth(), r.getHeight()) * 0.42f;
            g.setColour (juce::Colours::black); g.fillEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2);
            g.setColour (col.withAlpha (0.7f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.4f);
            if (d.variant == 1) { g.setColour (col.withAlpha (0.6f)); for (int i = 0; i < 3; ++i) { auto p = c.getPointOnCircumference (rr * 0.5f, i / 3.0f * juce::MathConstants<float>::twoPi); g.fillEllipse (p.x - 1, p.y - 1, 2, 2); } }
            break;
        }
        case PChip:
        {
            g.setColour (juce::Colours::black.withAlpha (0.65f)); g.fillRoundedRectangle (r, 2.0f);
            g.setColour (col.withAlpha (0.4f)); g.drawRoundedRectangle (r, 2.0f, 1.0f);
            const int pins = d.variant == 0 ? 4 : d.variant == 1 ? 7 : 5;
            g.setColour (col.withAlpha (0.5f));
            for (int i = 0; i < pins; ++i) { const float x = r.getX() + r.getWidth() * (i + 0.5f) / pins; g.fillRect (x - 0.8f, r.getY() - 2, 1.6f, 2.0f); g.fillRect (x - 0.8f, r.getBottom(), 1.6f, 2.0f); }
            break;
        }
        case PCoilLine:
        {
            const float turns = 5.0f + v * 2.0f; juce::Path p; const float w = r.getWidth();
            p.startNewSubPath (r.getX(), r.getCentreY());
            for (float t = 0; t <= 1.0f; t += 0.02f) p.lineTo (r.getX() + t * w, r.getCentreY() + std::sin (t * turns * juce::MathConstants<float>::twoPi) * r.getHeight() * 0.4f);
            g.setColour (col.withAlpha (0.5f)); g.strokePath (p, juce::PathStrokeType (1.4f));
            break;
        }
        case PPipe:
        {
            g.setColour (col.withAlpha (0.35f));
            if (d.variant == 1) { juce::Path p; p.startNewSubPath (r.getX(), r.getBottom()); p.lineTo (r.getX(), r.getCentreY()); p.quadraticTo (r.getX(), r.getY(), r.getCentreX(), r.getY()); p.lineTo (r.getRight(), r.getY()); g.strokePath (p, juce::PathStrokeType (4.0f)); }
            else { const int cnt = d.variant == 2 ? 2 : 1; for (int i = 0; i < cnt; ++i) { const float y = r.getCentreY() + (i - (cnt - 1) * 0.5f) * 6.0f; g.drawLine (r.getX(), y, r.getRight(), y, 4.0f); } }
            if (d.variant == 3) { auto c = r.getCentre(); g.setColour (col.withAlpha (0.5f)); g.drawEllipse (c.x - 4, c.y - 4, 8, 8, 1.4f); }
            break;
        }
        case PBadge:
        {
            auto c = r.getCentre(); const float rr = juce::jmin (r.getWidth(), r.getHeight()) * 0.46f;
            g.setColour (col.withAlpha (0.18f)); g.fillEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2);
            g.setColour (col.withAlpha (0.7f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.2f);
            if (d.variant == 2) { juce::Path star; for (int i = 0; i < 10; ++i) { const float a = i / 10.0f * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi; const float rad = (i & 1) ? rr * 0.4f : rr * 0.8f; auto p = c.getPointOnCircumference (rad, a); (i == 0 ? star.startNewSubPath (p) : star.lineTo (p)); } star.closeSubPath(); g.fillPath (star); }
            break;
        }
        case PHazard:
        {
            g.saveState(); g.reduceClipRegion (r.toNearestInt());
            const float step = 8.0f; g.setColour (col.withAlpha (0.5f));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += step * 2)
            { juce::Path p; p.startNewSubPath (x, r.getBottom()); p.lineTo (x + r.getHeight(), r.getY()); p.lineTo (x + r.getHeight() + step, r.getY()); p.lineTo (x + step, r.getBottom()); p.closeSubPath(); g.fillPath (p); }
            g.restoreState();
            break;
        }
        case PRivetRow:
        {
            const bool col5 = d.variant >= 3; const int cnt = d.variant == 0 ? 3 : d.variant == 1 ? 4 : d.variant == 2 ? 6 : d.variant == 3 ? 3 : 5;
            for (int i = 0; i < cnt; ++i)
            {
                auto c = col5 ? juce::Point<float> (r.getCentreX(), r.getY() + r.getHeight() * (i + 0.5f) / cnt)
                              : juce::Point<float> (r.getX() + r.getWidth() * (i + 0.5f) / cnt, r.getCentreY());
                screw (g, c, 2.0f, col);
            }
            break;
        }
        case PFuse:
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 2.0f);
            g.setColour (col.withAlpha (0.4f + 0.5f * lit)); g.drawLine (r.getX() + 3, r.getCentreY(), r.getRight() - 3, r.getCentreY(), 1.2f);
            g.setColour (col.withAlpha (0.7f)); g.fillRect (r.getX(), r.getY() + 1, 2.0f, r.getHeight() - 2); g.fillRect (r.getRight() - 2, r.getY() + 1, 2.0f, r.getHeight() - 2);
            break;
        }
        case PToggle:
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 3.0f);
            auto c = r.getCentre(); const bool up = d.variant == 0;
            g.setColour (col.withAlpha (0.8f)); g.fillRoundedRectangle (juce::Rectangle<float> (6.0f, 10.0f).withCentre ({ c.x, c.y + (up ? -3.0f : 3.0f) }), 2.0f);
            if (d.variant == 2) { g.setColour (col.withAlpha (0.3f)); g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 1.0f); }
            break;
        }
        case PBarcode:
        {
            Rng rng ((juce::uint32) (idx * 77 + 13)); g.setColour (col.withAlpha (0.6f));
            for (float x = r.getX(); x < r.getRight(); )
            { const float w = rng.range (1.0f, 3.0f); if (rng.chance (0.6f)) g.fillRect (x, r.getY(), w, r.getHeight()); x += w + rng.range (1.0f, 2.0f); }
            break;
        }
        case PGaugeTick:
        {
            auto c = juce::Point<float> (r.getCentreX(), r.getBottom()); const float rr = r.getHeight() * 0.9f;
            const int ticks = 6 + (int) v * 3; g.setColour (col.withAlpha (0.6f));
            for (int i = 0; i <= ticks; ++i) { const float a = juce::MathConstants<float>::pi * (0.15f + 0.7f * i / ticks); auto p1 = c.getPointOnCircumference (rr, -a); auto p2 = c.getPointOnCircumference (rr - 3.0f, -a); g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.0f); }
            break;
        }
        case PAntenna:
        {
            auto base = juce::Point<float> (r.getCentreX(), r.getBottom()); g.setColour (col.withAlpha (0.6f));
            if (d.variant == 1) { g.drawEllipse (r.getCentreX() - r.getWidth() * 0.4f, r.getY(), r.getWidth() * 0.8f, r.getHeight() * 0.5f, 1.4f); g.drawLine (base.x, base.y, base.x, r.getCentreY(), 1.4f); }
            else { g.drawLine (base.x, base.y, base.x, r.getY(), 1.6f); g.fillEllipse (base.x - 2, r.getY() - 2, 4, 4); }
            break;
        }
        case PHeatsink:
        {
            const int fins = 4 + (int) v * 2; g.setColour (col.withAlpha (0.3f));
            for (int i = 0; i < fins; ++i) { const float x = r.getX() + r.getWidth() * (i + 0.5f) / fins; g.fillRect (x - 1.2f, r.getY(), 2.4f, r.getHeight()); }
            g.setColour (juce::Colours::black.withAlpha (0.3f)); g.drawRect (r, 1.0f);
            break;
        }
        case PWaveEtch:
        {
            juce::Path p; const float w = r.getWidth(); p.startNewSubPath (r.getX(), r.getCentreY());
            for (float t = 0; t <= 1.0f; t += 0.02f)
            {
                float y;
                if (d.variant == 0) y = std::sin (t * juce::MathConstants<float>::twoPi * 2);
                else if (d.variant == 1) y = std::fmod (t * 2.0f, 1.0f) * 2 - 1;
                else y = (std::fmod (t * 2.0f, 1.0f) < 0.5f) ? -1.0f : 1.0f;
                p.lineTo (r.getX() + t * w, r.getCentreY() + y * r.getHeight() * 0.35f);
            }
            g.setColour (col.withAlpha (0.4f)); g.strokePath (p, juce::PathStrokeType (1.2f));
            break;
        }
        default: break;
    }
}

// =====================================================================================
// HERO PART RENDERERS — the big, live, functional-looking centrepiece of each effect.
// p[0..4] = Amount, Tone, Motion, Shape, Mix.  level = live audio 0..1.  t = seconds.
// =====================================================================================
inline void drawHero (juce::Graphics& g, Hero hero, juce::Rectangle<float> r, juce::Colour col,
                      const float p[5], float level, double t, juce::uint32 seed)
{
    recessed (g, r, col);
    auto in = r.reduced (6.0f);
    const float amount = p[0], tone = p[1], motion = p[2], shape = p[3], mix = p[4];
    const float lvl = juce::jlimit (0.0f, 1.0f, level);
    const float speed = 0.4f + motion * 3.0f;
    auto glow = col.brighter (0.2f + 0.5f * tone);

    switch (hero)
    {
        case Scope:
        {
            g.setColour (col.withAlpha (0.08f)); for (float y = in.getY(); y < in.getBottom(); y += 6) g.fillRect (in.getX(), y, in.getWidth(), 1.0f);
            juce::Path w; const float A = in.getHeight() * 0.42f * (0.2f + 0.8f * (amount * 0.5f + lvl));
            const float cy = in.getCentreY(); w.startNewSubPath (in.getX(), cy);
            for (float x = 0; x <= in.getWidth(); x += 2.0f)
            {
                const float u = x / in.getWidth();
                float y = std::sin (u * (3.0f + shape * 9.0f) * juce::MathConstants<float>::pi + (float) t * speed);
                y += 0.4f * std::sin (u * 17.0f + (float) t * speed * 1.7f) * mix;
                w.lineTo (in.getX() + x, cy + y * A);
            }
            g.setColour (glow.withAlpha (0.25f)); g.strokePath (w, juce::PathStrokeType (4.0f));
            g.setColour (glow); g.strokePath (w, juce::PathStrokeType (1.6f));
            break;
        }
        case Spectrum:
        {
            const int bars = 10 + (int) (shape * 14.0f); const float bw = in.getWidth() / bars;
            Rng rng (seed);
            for (int i = 0; i < bars; ++i)
            {
                const float base = std::pow (1.0f - (float) i / bars, 0.6f + tone);
                float h = base * (0.3f + 0.7f * lvl) * (0.6f + 0.4f * std::sin ((float) t * speed + i * 0.6f));
                h = juce::jlimit (0.04f, 1.0f, h * (0.5f + amount));
                auto bar = juce::Rectangle<float> (in.getX() + i * bw + 1.0f, in.getBottom() - in.getHeight() * h, bw - 2.0f, in.getHeight() * h);
                g.setColour (glow.withAlpha (0.35f + 0.5f * h)); g.fillRoundedRectangle (bar, 1.0f);
                g.setColour (glow.brighter (0.4f)); g.fillRect (bar.withHeight (2.0f));
            }
            break;
        }
        case Speaker:
        {
            auto c = in.getCentre(); const float R = juce::jmin (in.getWidth(), in.getHeight()) * 0.5f;
            const float push = 1.0f + lvl * 0.12f * std::sin ((float) t * speed * 2.0f);
            for (int i = 6; i >= 1; --i)
            {
                const float rr = R * i / 6.0f * (i <= 2 ? push : 1.0f);
                g.setColour ((i <= 2 ? glow : col).withAlpha (i <= 2 ? 0.25f + 0.4f * lvl : 0.18f));
                g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, i <= 2 ? 2.0f : 1.2f);
            }
            g.setColour (col.darker (0.4f)); g.fillEllipse (c.x - R * 0.28f, c.y - R * 0.28f, R * 0.56f, R * 0.56f);
            g.setColour (glow.withAlpha (0.4f + 0.6f * lvl)); g.fillEllipse (c.x - R * 0.12f, c.y - R * 0.12f, R * 0.24f, R * 0.24f);
            break;
        }
        case ReelDeck:
        {
            const float R = juce::jmin (in.getWidth() * 0.28f, in.getHeight() * 0.42f);
            const float ang = (float) t * speed * (0.6f + amount);
            for (int s = 0; s < 2; ++s)
            {
                auto c = juce::Point<float> (in.getX() + R + 4 + s * (in.getWidth() - 2 * R - 8), in.getCentreY());
                g.setColour (col.darker (0.3f)); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
                g.setColour (glow.withAlpha (0.8f)); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.6f);
                for (int i = 0; i < 3; ++i) { const float a = ang + i * juce::MathConstants<float>::twoPi / 3.0f; auto p = c.getPointOnCircumference (R * 0.7f, a); g.drawLine (c.x, c.y, p.x, p.y, 2.0f); }
                g.setColour (glow); g.fillEllipse (c.x - 3, c.y - 3, 6, 6);
            }
            g.setColour (glow.withAlpha (0.3f)); g.drawLine (in.getX() + R + 4, in.getCentreY() - R, in.getRight() - R - 4, in.getCentreY() - R, 1.4f);
            break;
        }
        case CoilRack:
        {
            const int coils = 2 + (int) (shape * 3.0f); const float ch = in.getHeight() / coils;
            for (int cI = 0; cI < coils; ++cI)
            {
                const float y = in.getY() + ch * (cI + 0.5f); juce::Path p; p.startNewSubPath (in.getX(), y);
                const float turns = 6.0f + motion * 10.0f;
                for (float x = 0; x <= in.getWidth(); x += 2.0f)
                {
                    const float u = x / in.getWidth();
                    p.lineTo (in.getX() + x, y + std::sin (u * turns * juce::MathConstants<float>::twoPi + (float) t * speed + cI) * ch * 0.3f * (0.4f + lvl));
                }
                g.setColour (glow.withAlpha (0.4f + 0.4f * lvl)); g.strokePath (p, juce::PathStrokeType (1.5f));
            }
            break;
        }
        case CrystalTube:
        {
            auto c = in.getCentre(); const float w = in.getWidth() * 0.34f, h = in.getHeight() * 0.8f;
            auto tube = juce::Rectangle<float> (w, h).withCentre (c);
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (tube, w * 0.5f);
            const float heat = 0.3f + 0.7f * (amount * 0.5f + lvl * 0.6f);
            g.setGradientFill (juce::ColourGradient (glow.withAlpha (heat), c.x, c.getY() + c.y, col.withAlpha (0.05f), c.x, tube.getY(), false));
            g.fillRoundedRectangle (tube.reduced (3.0f), w * 0.4f);
            g.setColour (glow.withAlpha (0.2f + 0.6f * heat)); g.fillEllipse (c.x - 4, c.y - 4 + std::sin ((float) t * speed) * 4.0f, 8, 8);
            g.setColour (col.withAlpha (0.7f)); g.drawRoundedRectangle (tube, w * 0.5f, 1.4f);
            break;
        }
        case VuGauge:
        {
            auto c = juce::Point<float> (in.getCentreX(), in.getBottom() - 4.0f); const float R = juce::jmin (in.getWidth() * 0.46f, in.getHeight() * 0.9f);
            g.setColour (col.withAlpha (0.5f));
            for (int i = 0; i <= 10; ++i) { const float a = juce::MathConstants<float>::pi * (0.1f + 0.8f * i / 10.0f); auto p1 = c.getPointOnCircumference (R, -a); auto p2 = c.getPointOnCircumference (R - 4, -a); g.setColour ((i > 7 ? ui::hot() : col).withAlpha (0.6f)); g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.2f); }
            const float val = juce::jlimit (0.0f, 1.0f, lvl * 0.7f + amount * 0.3f);
            const float a = juce::MathConstants<float>::pi * (0.1f + 0.8f * val); auto nd = c.getPointOnCircumference (R - 2, -a);
            g.setColour (glow.brighter (0.3f)); g.drawLine (c.x, c.y, nd.x, nd.y, 2.0f); g.fillEllipse (c.x - 3, c.y - 3, 6, 6);
            break;
        }
        case LampBank:
        {
            const int cols = 4 + (int) (shape * 4.0f), rows = 2 + (int) (amount * 2.0f);
            const float cw = in.getWidth() / cols, chh = in.getHeight() / rows; Rng rng (seed);
            for (int y = 0; y < rows; ++y) for (int x = 0; x < cols; ++x)
            {
                auto cell = juce::Rectangle<float> (in.getX() + x * cw, in.getY() + y * chh, cw, chh).reduced (3.0f);
                const float ph = rng.f() * 6.28f; const float on = 0.3f + 0.7f * std::max (0.0f, std::sin ((float) t * speed + ph)) * (0.3f + lvl);
                g.setColour (col.withAlpha (0.15f)); g.fillEllipse (cell);
                g.setColour (glow.withAlpha (on)); g.fillEllipse (cell.reduced (cell.getWidth() * 0.18f));
            }
            break;
        }
        case Switchboard:
        {
            const int n = 3 + (int) (shape * 5.0f); const float sw = in.getWidth() / n;
            for (int i = 0; i < n; ++i)
            {
                auto slot = juce::Rectangle<float> (in.getX() + i * sw + 2, in.getY() + 2, sw - 4, in.getHeight() - 4);
                g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (slot, 2.0f);
                const float v = 0.5f + 0.5f * std::sin (i * 1.3f + tone * 4.0f);
                auto handle = juce::Rectangle<float> (slot.getWidth() - 2, 8.0f).withCentre ({ slot.getCentreX(), slot.getBottom() - slot.getHeight() * v });
                g.setColour (glow.withAlpha (0.7f)); g.fillRoundedRectangle (handle, 2.0f);
            }
            break;
        }
        case PipeWerk:
        {
            g.setColour (col.withAlpha (0.4f)); const int pipes = 2 + (int) (shape * 3.0f);
            for (int i = 0; i < pipes; ++i)
            {
                const float y = in.getY() + in.getHeight() * (i + 0.5f) / pipes;
                g.setColour (col.withAlpha (0.25f)); g.drawLine (in.getX(), y, in.getRight(), y, 5.0f);
                const float fx = in.getX() + std::fmod ((float) t * speed * 30.0f + i * 20.0f, in.getWidth());
                g.setColour (glow.withAlpha (0.5f + 0.4f * lvl)); g.fillEllipse (fx - 3, y - 3, 6, 6);
            }
            break;
        }
        case Grille:
        {
            const float step = 5.0f + (1.0f - shape) * 4.0f; g.setColour (col.withAlpha (0.35f));
            for (float y = in.getY() + 2; y < in.getBottom(); y += step)
                for (float x = in.getX() + 2 + (((int) y & 1) ? step * 0.5f : 0); x < in.getRight(); x += step)
                    g.fillEllipse (x, y, 2.0f, 2.0f);
            g.setColour (glow.withAlpha (0.1f + 0.5f * lvl)); g.fillRoundedRectangle (in, 3.0f);
            break;
        }
        case DataScreen:
        default:
        {
            g.setColour (col.withAlpha (0.10f)); g.fillRoundedRectangle (in, 3.0f);
            Rng rng (seed); g.setFont (ui::hud (8.0f, false));
            const int lines = juce::jmax (2, (int) (in.getHeight() / 11.0f));
            for (int i = 0; i < lines; ++i)
            {
                const float y = in.getY() + i * 11.0f + 2.0f; if (y > in.getBottom() - 8) break;
                juce::String s; const int cells = 6 + rng.ri (0, 6);
                for (int k = 0; k < cells; ++k) s += juce::String::toHexString (rng.ri (0, 255)).paddedLeft ('0', 2).toUpperCase() + " ";
                g.setColour (glow.withAlpha (0.4f + 0.4f * ((std::sin ((float) t * speed + i) + 1) * 0.5f) * (0.4f + lvl)));
                g.drawText (s, in.getX() + 4, y, in.getWidth() - 6, 10, juce::Justification::centredLeft, false);
            }
            break;
        }
    }
    // subtle glass highlight over any screen-like part
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (r.reduced (3.0f).removeFromTop (r.getHeight() * 0.3f), 3.0f);
}

// Scatter decals around a hero part on the shared chassis, gated by the effect's values.
inline void dressAround (juce::Graphics& g, juce::Rectangle<float> tile, juce::Colour col,
                        const float p[5], float level, juce::uint32 seed)
{
    Rng rng (seed ^ 0x51ed270bu);
    const float amount = p[0], tone = p[1], motion = p[2], shape = p[3], mix = p[4];
    int bucket[32];

    // corners always get a mount
    { int n = decalsInSlot (0, bucket, 32); if (n > 0) {
        const int pick = bucket[rng.ri (0, n - 1)];
        for (auto c : { tile.getTopLeft(), tile.getTopRight(), tile.getBottomLeft(), tile.getBottomRight() })
            drawDecal (g, pick, juce::Rectangle<float> (8.0f, 8.0f).withCentre (c + juce::Point<float> (c.x < tile.getCentreX() ? 6.0f : -6.0f, c.y < tile.getCentreY() ? 6.0f : -6.0f)), col, 0.0f);
    } }

    auto strip = tile.reduced (10.0f);
    // label badge along the top edge
    if (int n = decalsInSlot (2, bucket, 32)) drawDecal (g, bucket[rng.ri (0, n - 1)], strip.removeFromTop (12.0f).removeFromLeft (tile.getWidth() * 0.6f), col, 0.0f);

    // right rail of conditional detail
    auto rail = tile.removeFromRight (14.0f).reduced (2.0f, 14.0f);
    auto place = [&] (int slot, float lit) { if (int n = decalsInSlot (slot, bucket, 32)) { auto cell = rail.removeFromTop (16.0f); rail.removeFromTop (3.0f); drawDecal (g, bucket[rng.ri (0, n - 1)], cell, col, lit); } };
    if (tone  > 0.45f) place (3, juce::jlimit (0.0f, 1.0f, tone * 0.5f + level));   // lights
    if (shape > 0.5f)  place (1, 0.0f);                                             // vent/structure
    if (motion > 0.5f) place (1, 0.0f);                                            // more structure (coils/antenna)
    if (mix   > 0.55f) place (4, 0.0f);                                            // control
    if (amount > 0.7f) place (5, 0.0f);                                            // hazard/flavour
}
} // namespace parts
