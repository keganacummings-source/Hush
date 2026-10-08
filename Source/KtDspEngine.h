#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace kt::dsp {

// Per-effect DSP ported from DreamMaster FX Builder. Each of the 200 catalog
// entries maps (via kindFor) to one of the recipes below, giving every effect
// its own distinct character instead of a shared family routine.
class DspEngine {
public:
    static constexpr int effectCount = 200;

    enum class Kind {
        Drive, Chorus, Delay, Reverb, Width, HighPass, LowPass, Tremolo,
        AutoPan, Flanger, Phaser, Crush, Vibrato, Telephone, Tape, Gate,
        Formant, Comb, Compressor, Warmth, Fade, Grain, Rotary, Air, Grit,
        Haze, Presence, Notch, MonoBass, Exciter, Duck, Shimmer, RingMod,
        Freeze, Tilt, Bass, Room, Hall, Saturate, Limiter, Echo, Slap,
        Stutter, Vinyl, Radio, Underwater, Glass, Choir, Pulse, Drift,
        Snap, Body, Bloom, Swirl, Ping, AirLift, SubBoost, LongEcho, Ink,
        Clean, Stage, Wire, Shelf, SleepGate, Acid, GhostDelay, BoneEQ,
        Cinder, Orbit, Pluck, DrumGlue, HatAir, DarkHall, BrightPlate,
        DeEss, SideAir, Flutter, Gate16, OctaveDown, OctaveUp, Smear, Dust,
        Gain, Trim, Balance, Image, DeepPhase, Fog, Sparkle, TightComp,
        OpenComp, Cream, Sheen, Focus, Resonant
    };

    struct EffectState {
        std::array<float, 2> low{};
        std::array<float, 2> low2{};
        std::array<float, 2> previous{};
        std::array<float, 8> allpass{};
        std::array<float, 2> envelope{};
        std::array<float, 2> held{};
        std::array<std::vector<float>, 4> comb;
        std::array<size_t, 4> combPosition{};
        std::vector<std::array<float, 2>> delay;
        size_t delayPosition = 0;
        size_t bufferedSamples = 0;
        float phase = 0.0f;
        float carrierPhase = 0.0f;
        float mix = 0.0f;
        float currentAmount = 0.0f;
        float crusherCounter = 0.0f;
    };

    DspEngine();
    void prepare(double sampleRate);
    void prepareState(EffectState& st, int index) const;
    void resetState(EffectState& st) const;
    static Kind kindFor(int index);
    float processOne(EffectState& st, int index, float amount, float tone,
                     float motion, float shape, float left, float right,
                     float& rightOutput);

private:
    static float sanitize(float value);
    static float softClip(float value, float drive);
    float processEffect(Kind kind, EffectState& st, int index, float amount,
                        float tone, float motion, float shape,
                        float left, float right, int channel, float& rightOutput);

    double sampleRate = 44100.0;
};

} // namespace kt::dsp
