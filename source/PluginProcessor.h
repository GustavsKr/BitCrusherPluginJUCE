#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/BitCrusher.h"

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Atomic flags so the GUI can see exactly how "broken" the sound is
    std::atomic<bool> isGuiActive { false };
    std::atomic<float> currentCrushVisual { 0.0f };
    std::atomic<float> currentDownsampleVisual { 1.0f };
    
    // Grabs a block of audio samples safely for the UI thread
    static constexpr int visualizerSize = 512;
    std::vector<float> getVisualizerSamples();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    BitCrusher crusherEngine;
    
    std::vector<float> fifoBuffer;
    int fifoIndex = 0;
    std::vector<float> visualizerStorage; // Holds the safe copy for the UI
    juce::CriticalSection fifoCriticalSection; // Lock preventing thread crashes


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
