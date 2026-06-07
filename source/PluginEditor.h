#pragma once
#include "PluginProcessor.h"
#include "components/AsciiVisualizer.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;

    AsciiVisualizer terminalVisualizer;

    juce::Slider crushSlider;
    juce::Slider downsampleSlider;
    juce::Slider toneSlider;
    juce::Slider mixSlider;
    
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    
    std::unique_ptr<Attachment> crushAttachment;
    std::unique_ptr<Attachment> downsampleAttachment;
    std::unique_ptr<Attachment> toneAttachment;
    std::unique_ptr<Attachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
