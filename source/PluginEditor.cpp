#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (600, 400);

    bitDepthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    bitDepthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible(bitDepthSlider);
    bitDepthAttachment = std::make_unique<Attachment>(processorRef.apvts, "BIT_DEPTH", bitDepthSlider);

    downsampleSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    downsampleSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible(downsampleSlider);
    downsampleAttachment = std::make_unique<Attachment>(processorRef.apvts, "DOWNSAMPLE", downsampleSlider);

    mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    addAndMakeVisible(mixSlider);
    mixAttachment = std::make_unique<Attachment>(processorRef.apvts, "MIX", mixSlider);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (16.0f));

    g.drawText ("Future Visualizer / Top Workspace Area", 0, 0, getWidth(), 250, juce::Justification::centred);

    auto bottomStrip = getLocalBounds().removeFromBottom(150);
    auto columnWidth = bottomStrip.getWidth() / 3;
    g.drawText ("Bit Depth",  0 * columnWidth, 230, columnWidth, 20, juce::Justification::centred);
    g.drawText ("Downsample", 1 * columnWidth, 230, columnWidth, 20, juce::Justification::centred);
    g.drawText ("Mix",        2 * columnWidth, 230, columnWidth, 20, juce::Justification::centred);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto area = getLocalBounds();
    auto topSpaceReserved = area.removeFromTop(250); 
    auto columnWidth = area.getWidth() / 3;
    
    // .reduced(10) leaves a nice margin around them so they aren't squished together.
    bitDepthSlider.setBounds   (area.removeFromLeft(columnWidth).reduced(10));
    downsampleSlider.setBounds (area.removeFromLeft(columnWidth).reduced(10));
    mixSlider.setBounds        (area.removeFromLeft(columnWidth).reduced(10));
}
