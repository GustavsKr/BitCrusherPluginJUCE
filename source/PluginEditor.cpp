#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), terminalVisualizer (p)
{
    juce::ignoreUnused (processorRef);

    processorRef.isGuiActive = true;
    setSize (600, 400);

    auto setupKnob = [this](juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xFF3A3A3A));
        slider.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colour (0xFFCCCCCC));
        slider.setColour (juce::Slider::thumbColourId,               juce::Colours::transparentBlack);
        
        slider.onValueChange = [this] { repaint(); };
        
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
    // 1. Gojira Matte Obsidian Background
    g.fillAll (juce::Colour (0xFF161616));
    
    // 2. Clear Screen Border Frame
    g.setColour (juce::Colour (0xFF2D2D2D)); 
    g.drawRect (10, 10, 580, 220, 1);       

    // 3. Absolute Coordinate Layout Metrics
    const int columnWidth = 150; 
    
    // --- THE FIX: Shifted up to pull the text closer to the knobs ---
    const int labelY = 326;       // Snug right below the 90x90 knob radius
    const int valueY = 344;       // Sits tightly underneath the title text
    const int textHeight = 18;

    // --- ROW 1: Minimalist Labels (Helvetica 13pt Bold, Silver) ---
    g.setFont (juce::FontOptions ("Helvetica", 13.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xFFE5E5E5));

    g.drawText ("CRUSH",      0 * columnWidth, labelY, columnWidth, textHeight, juce::Justification::centred);
    g.drawText ("DOWNSAMPLE", 1 * columnWidth, labelY, columnWidth, textHeight, juce::Justification::centred);
    g.drawText ("TONE",       2 * columnWidth, labelY, columnWidth, textHeight, juce::Justification::centred);
    g.drawText ("MIX",        3 * columnWidth, labelY, columnWidth, textHeight, juce::Justification::centred);

    // --- ROW 2: Borderless Live Values (Helvetica 11pt Bold, Silver) ---
    g.setFont (juce::FontOptions ("Helvetica", 11.0f, juce::Font::bold));
    
    // Calculate Normalized Downsample Display Value (0.00 - 1.00)
    double rawDownsample = downsampleSlider.getValue();
    double normalizedDownsample = (rawDownsample - 1.0) / 31.0;
    juce::String downsampleDisplay = juce::String (normalizedDownsample, 2);

    // Draw the bold value strings
    g.drawText (crushSlider.getTextFromValue(crushSlider.getValue()), 0 * columnWidth, valueY, columnWidth, textHeight, juce::Justification::centred);
    g.drawText (downsampleDisplay,                                    1 * columnWidth, valueY, columnWidth, textHeight, juce::Justification::centred);
    g.drawText (toneSlider.getTextFromValue(toneSlider.getValue()),   2 * columnWidth, valueY, columnWidth, textHeight, juce::Justification::centred);
    g.drawText (mixSlider.getTextFromValue(mixSlider.getValue()),     3 * columnWidth, valueY, columnWidth, textHeight, juce::Justification::centred);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // 1. Lock screen visualizer bounds
    terminalVisualizer.setBounds (10, 10, 580, 220);

    // 2. Move physical knobs up to leave breathing room for text below them
    const int columnWidth = 150;
    const int knobSize = 90;
    const int knobY = 240; // Shifted UP closer to the animation screen
    const int xOffset = (columnWidth - knobSize) / 2; 

    // Map each slider directly to its new higher absolute slots
    crushSlider.setBounds      (0 * columnWidth + xOffset, knobY, knobSize, knobSize);
    downsampleSlider.setBounds (1 * columnWidth + xOffset, knobY, knobSize, knobSize);
    toneSlider.setBounds       (2 * columnWidth + xOffset, knobY, knobSize, knobSize);
    mixSlider.setBounds        (3 * columnWidth + xOffset, knobY, knobSize, knobSize);
}