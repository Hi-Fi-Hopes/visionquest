#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/Theme.h"

using namespace vq;

VisionQuestProcessor::VisionQuestProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VisionQuest", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VisionQuestProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; };

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Mode", ids::modeNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::theme, 1 }, "Theme", themeNames(), 0));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::sensitivity, 1 }, "Sensitivity",
        NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::smoothing, 1 }, "Smoothing",
        NormalisableRange<float> (0.0f, 1.0f), 0.45f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::speed, 1 }, "Speed",
        NormalisableRange<float> (0.1f, 3.0f, 0.0f, 0.6f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + "x"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::trails, 1 }, "Trails",
        NormalisableRange<float> (0.0f, 1.0f), 0.45f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::glow, 1 }, "Glow",
        NormalisableRange<float> (0.0f, 1.0f), 0.6f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::react, 1 }, "Beat React",
        NormalisableRange<float> (0.0f, 1.0f), 0.6f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::detail, 1 }, "Detail",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::hueDrift, 1 }, "Hue Drift",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::mirror, 1 }, "Mirror", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::cycle, 1 }, "Auto Cycle", ids::cycleNames(), 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::shuffle, 1 }, "Shuffle", false));

    return { p.begin(), p.end() };
}

void VisionQuestProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store (sampleRate);
    scratchR.allocate ((size_t) juce::jmax (1, samplesPerBlock), true);
    scratchSize = juce::jmax (1, samplesPerBlock);
}

bool VisionQuestProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return in.size() <= out.size();
}

void VisionQuestProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    // Mono in, stereo out: copy the left channel across so the output is stereo.
    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int ch = juce::jmax (numIn, numIn == 1 ? 2 : numIn); ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    if (n <= 0 || buffer.getNumChannels() == 0) return;

    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;

    // Push in chunks so a host block bigger than the ring can't overrun it.
    for (int pos = 0; pos < n; pos += vq::SampleTap::capacity / 2)
    {
        const int len = juce::jmin (n - pos, vq::SampleTap::capacity / 2);
        tap.push (l + pos, r + pos, len);
    }
}

juce::AudioProcessorEditor* VisionQuestProcessor::createEditor()
{
    return new VisionQuestEditor (*this);
}

void VisionQuestProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiWidth", savedWidth, nullptr);
    state.setProperty ("uiHeight", savedHeight, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void VisionQuestProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            savedWidth = state.getProperty ("uiWidth", savedWidth);
            savedHeight = state.getProperty ("uiHeight", savedHeight);
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VisionQuestProcessor();
}
