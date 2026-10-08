#pragma once
#include <JuceHeader.h>
#include "Rack.h"

class KyotoProcessor : public juce::AudioProcessor
{
public:
    KyotoProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "KyotoVST"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::String patchJson() const { return juce::JSON::toString (rack.toVar(), true); }
    bool loadPatchJson (const juce::String& json);

    kv::Rack rack;
    juce::AudioProcessorValueTreeState apvts;
    juce::String themeId { "trippah" }, sessionUser, sessionToken, sessionRole;
    bool viewerMode = false;
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    std::atomic<float>* inGain = nullptr; std::atomic<float>* outGain = nullptr; std::atomic<float>* mix = nullptr;
    std::atomic<float>* macros[kv::kMacros] {};
    juce::AudioBuffer<float> dry;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KyotoProcessor)
};
