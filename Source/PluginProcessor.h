#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/SampleTap.h"
#include "Params.h"

// VisionQuest passes audio through untouched. All it does on the audio thread
// is copy each block into the tap; the editor does the analysis and drawing,
// so with the window closed it costs next to nothing.
class VisionQuestProcessor : public juce::AudioProcessor
{
public:
    VisionQuestProcessor();
    ~VisionQuestProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VisionQuest"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    vq::SampleTap tap;
    std::atomic<double> currentSampleRate { 44100.0 };

    // Window size, kept with the project.
    int savedWidth = 960, savedHeight = 600;

private:
    juce::HeapBlock<float> scratchR;
    int scratchSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisionQuestProcessor)
};
