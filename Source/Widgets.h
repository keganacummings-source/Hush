#pragma once
// Widgets: one-drag behaviour units. Unlike Machines (which model a specific piece of
// real/alien hardware), a Widget is a packaged *trick* — drop one in and the whole line
// turns into that behaviour (a time chopper, a reversed forward-echo panner, a sidechain
// pumper...). Same recipe grammar as Machines; only valid FX ids from FxCatalog are used.
#include <JuceHeader.h>
namespace kv
{
struct Widget { const char* name; const char* blurb; const char* recipe; };

inline constexpr Widget kWidgets[] = {
    { "Time Chopper",       "16th-note hard chop + pan",        "gate16@0.8,0.5,0.7 > stutter@0.5,0.5,0.6 > pan@0.5,0.5,0.6" },
    { "Reverse Thrower",    "reversed forward echo on a pan",   "reverse@0.7,0.5,0.5,0.5,0.6 > longecho@0.6,0.4,0.5,0.6,0.5 > pan2@0.5,0.5,0.7" },
    { "Glitch Shuffler",    "random stutter + crush + reverse", "stutter@0.75,0.6,0.8 > crush@0.5,0.5,0.5,0.6 > reverse@0.4" },
    { "Sidechain Pumper",   "ducking pump glue",                "duck@0.8,0.5,0.7 > glue@0.4,0.5,0.4" },
    { "Auto-Pan Trem",      "tremolo into stereo pan",          "trem@0.7,0.5,0.6 > pan2@0.6,0.5,0.7" },
    { "Octave Doubler",     "octave up + down under dry",       "octup@0.5,0.5,0.5,0.5,0.5 | octdown@0.5,0.5,0.5,0.5,0.4 | clean" },
    { "Tape Warble",        "wow/flutter + vibrato",            "tape@0.5,0.4,0.5 > wow2@0.6,0.5,0.5 > vibrato@0.4,0.5,0.4,0.5,0.4" },
    { "Shimmer Wash",       "octave shimmer into hall",         "shimmer@0.6,0.6,0.5,0.5,0.6 > hall2@0.6,0.5,0.5,0.5,0.4" },
    { "Ring Slicer",        "ring mod gated to 16ths",          "ringmod@0.6,0.5,0.5,0.5,0.7 > gate16@0.7,0.5,0.6" },
    { "Width Blaster",      "triple stereo widener",            "width@0.6 > widen2@0.5 > width3@0.6" },
    { "Freeze Hold",        "infinite freeze + halo",           "freeze@0.75,0.5,0.5,0.5,0.6 > halo2@0.5,0.5,0.5,0.5,0.5" },
    { "Grain Cloud",        "granular smear fog",               "grain@0.6,0.5,0.6,0.5,0.7 > smear@0.5,0.5,0.6 > fogbank@0.5,0.45,0.5,0.5,0.5" },
    { "Dive Bomber",        "octave-down smear dive",           "octdown@0.7,0.5,0.6,0.5,0.7 > smear@0.5,0.5,0.6" },
    { "Formant Talker",     "vowel talkbox through a phone",    "formant@0.7,0.5,0.6 > formant2@0.5,0.6 > phone@0.4" },
    { "Bit Smasher",        "double bitcrush + low-pass",       "crush@0.7,0.5,0.5,0.6 > crush2@0.5,0.5,0.6 > lpf@0.5,0.5" },
    { "Orbit Panner",       "quad orbit auto-panner",           "orbit@0.6,0.5,0.6 > orbit2@0.5 > pan2@0.5,0.5,0.7" },
    { "Ping Bouncer",       "ping-pong delay bounce",           "ping@0.65,0.5,0.5,0.6,0.5 > width3@0.5" },
    { "Swirl Phaser",       "swirl filter into phaser",         "swirl@0.6,0.5,0.7 > phaser@0.55,0.5,0.5,0.5,0.6" },
};
inline constexpr int kWidgetCount = (int) (sizeof (kWidgets) / sizeof (kWidgets[0]));
} // namespace kv
