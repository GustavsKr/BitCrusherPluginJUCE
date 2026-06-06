#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), terminalVisualizer (p)
{
    juce::ignoreUnused (processorRef);

    processorRef.isGuiActive = true;
    setResizable (true, true);
    setResizeLimits (400, 300, 1000, 700); // Set safety limits so it can't be crushed to 0px
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (600, 400);

    auto setupKnob = [this](juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        addAndMakeVisible(slider);
    };

    // Set up the Crush slider
    setupKnob(crushSlider);
    crushAttachment = std::make_unique<Attachment>(processorRef.apvts, "CRUSH", crushSlider);

    setupKnob(downsampleSlider);
    downsampleAttachment = std::make_unique<Attachment>(processorRef.apvts, "DOWNSAMPLE", downsampleSlider);

    setupKnob(toneSlider);
    toneAttachment = std::make_unique<Attachment>(processorRef.apvts, "TONE", toneSlider);

    setupKnob(mixSlider);
    mixAttachment = std::make_unique<Attachment>(processorRef.apvts, "MIX", mixSlider);

    addAndMakeVisible (terminalVisualizer);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    processorRef.isGuiActive = false;
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (16.0f));

    auto bottomStrip = getLocalBounds().removeFromBottom(150);
    auto columnWidth = bottomStrip.getWidth() / 4;

    // Renamed the text label to match your new system layout
    g.drawText ("Crush",       0 * columnWidth, 230, columnWidth, 20, juce::Justification::centred);
    g.drawText ("Downsample",  1 * columnWidth, 230, columnWidth, 20, juce::Justification::centred);
    g.drawText ("Tone",        2 * columnWidth, 230, columnWidth, 20, juce::Justification::centred);
    g.drawText ("Mix",         3 * columnWidth, 230, columnWidth, 20, juce::Justification::centred);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto area = getLocalBounds();
    int visualizerHeight = area.getHeight() * 0.6f;
    terminalVisualizer.setBounds (area.removeFromTop (visualizerHeight).reduced (10));
    
    // .reduced(10) leaves a nice margin around them so they aren't squished together.
    auto columnWidth = area.getWidth() / 4;
    crushSlider.setBounds      (area.removeFromLeft(columnWidth).reduced(10));
    downsampleSlider.setBounds (area.removeFromLeft(columnWidth).reduced(10));
    toneSlider.setBounds       (area.removeFromLeft(columnWidth).reduced(10));
    mixSlider.setBounds        (area.removeFromLeft(columnWidth).reduced(10));
}
