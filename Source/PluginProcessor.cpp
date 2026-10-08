#include "PluginProcessor.h"
#include "PluginEditor.h"

KyotoProcessor::KyotoProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KyotoVST", layout())
{
    inGain = apvts.getRawParameterValue ("in");
    outGain = apvts.getRawParameterValue ("out");
    mix = apvts.getRawParameterValue ("mix");
    for (int i = 0; i < kv::kMacros; ++i) macros[i] = apvts.getRawParameterValue ("macro" + juce::String (i + 1));
    rack.addMachine ("warm@0.35,0.5,0.5,0.5 > plate@0.4,0.5,0.5,0.5,0.3", 140.0f, 160.0f);
    rack.commit();
}

juce::AudioProcessorValueTreeState::ParameterLayout KyotoProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    using P = juce::AudioParameterFloat;
    const auto db = juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; });
    const auto pc = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });
    l.add (std::make_unique<P> (juce::ParameterID { "in", 1 }, "Input", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<P> (juce::ParameterID { "out", 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<P> (juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, pc));
    for (int i = 1; i <= kv::kMacros; ++i)
        l.add (std::make_unique<P> (juce::ParameterID { "macro" + juce::String (i), 1 }, "Macro " + juce::String (i), juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, pc));
    return l;
}

bool KyotoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && layouts.getMainInputChannelSet() == out;
}

void KyotoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    rack.prepare (sampleRate, samplesPerBlock);
    dry.setSize (2, samplesPerBlock);
}

void KyotoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
    if (ch == 0 || n == 0) return;
    buffer.applyGain (juce::Decibels::decibelsToGain (inGain->load()));
    meterIn = buffer.getMagnitude (0, n);

    const bool useDry = n <= dry.getNumSamples();
    if (useDry) for (int c = 0; c < 2; ++c) dry.copyFrom (c, 0, buffer, juce::jmin (c, ch - 1), 0, n);
    float m[kv::kMacros];
    for (int i = 0; i < kv::kMacros; ++i) m[i] = macros[i]->load();

    float* L = buffer.getWritePointer (0);
    if (ch > 1) rack.process (L, buffer.getWritePointer (1), n, m);
    else if (useDry) rack.process (L, dry.getWritePointer (1), n, m);

    const float w = mix->load();
    if (useDry && w < 0.999f)
    {
        if (ch == 1) dry.copyFrom (1, 0, dry, 0, 0, n);
        for (int c = 0; c < juce::jmin (2, ch); ++c)
        {
            buffer.applyGain (c, 0, n, w);
            buffer.addFrom (c, 0, dry, c, 0, n, 1.0f - w);
        }
    }
    buffer.applyGain (juce::Decibels::decibelsToGain (outGain->load()));
    meterOut = buffer.getMagnitude (0, n);
}

bool KyotoProcessor::loadPatchJson (const juce::String& json)
{
    auto v = juce::JSON::parse (json);
    if (v.hasProperty ("machineDesign")) v = v["machineDesign"];
    if (! rack.fromVar (v)) return false;
    rack.commit();
    return true;
}

void KyotoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("patch", patchJson(), nullptr);
    state.setProperty ("theme", themeId, nullptr);
    state.setProperty ("viewer", viewerMode, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void KyotoProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    themeId = state.getProperty ("theme", themeId).toString();
    viewerMode = (bool) state.getProperty ("viewer", false);
    loadPatchJson (state.getProperty ("patch").toString());
    state.removeProperty ("patch", nullptr);
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* KyotoProcessor::createEditor() { return new KyotoEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KyotoProcessor(); }
