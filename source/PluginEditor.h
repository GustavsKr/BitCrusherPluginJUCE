#pragma once
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

class AsciiVisualizer : public juce::Component, public juce::Timer
{
public:
    AsciiVisualizer (AudioPluginAudioProcessor& p) : processorRef (p)
    {
        // Start the engine! 60 Hz = ~60 Frames Per Second animation speed
        startTimerHz (60); 
    }

    ~AsciiVisualizer() override
    {
        stopTimer();
    }

    // The heart of the animation: called automatically 60 times a second
    void timerCallback() override
    {
        repaint(); 
    }

    void paint (juce::Graphics& g) override
    {
        // 1. Draw the CRT terminal background frame
        g.fillAll (juce::Colour (0xFF050A05)); 

        // 2. Fetch our safe 1024 audio sample snapshot from the Step 1 highway
        auto samples = processorRef.getVisualizerSamples();
        float rms = processorRef.currentRmsLevel.load();
        float crush = processorRef.currentCrushVisual.load();
        float downsample = processorRef.currentDownsampleVisual.load();

        if (samples.size() < 1024) return; // Wait until our buffer bucket fills up

        // ==========================================================
        // --- NEW: STEP 4 MATHEMATICAL FFT CALCULATIONS ---
        // ==========================================================
        
        // A. Apply a manual Hann Window loop to eliminate edge-clicking mathematical noise
        for (int i = 0; i < 1024; ++i)
        {
            float t = static_cast<float>(i) / 1023.0f;
            float hannWindow = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * t));
            
            fftData[i] = samples[i] * hannWindow;
        }
        // Pad the second half of the complex work array with zeroes
        juce::FloatVectorOperations::clear (fftData + 1024, 1024);

        // B. Run the magic Fast Fourier Transform math!
        forwardFFT.performRealOnlyForwardTransform (fftData);

        // C. Calculate the volume magnitudes for our 512 distinct frequency bands
        for (int i = 0; i < 512; ++i)
        {
            float real = fftData[2 * i];
            float imag = fftData[2 * i + 1];
            float magnitude = std::sqrt (real * real + imag * imag);

            // Convert raw math amplitude into a smooth, readable envelope line
            frequencies[i] = juce::jmap (magnitude, 0.0f, 30.0f, 0.0f, 1.0f);
        }

        // ==========================================================
        // --- TEXT GRID RENDERING ENGINE ---
        // ==========================================================
        g.setFont (juce::FontOptions ("Courier New", 12.0f, juce::Font::plain));
        int charWidth = 8;
        int charHeight = 14;
        
        int numCols = getWidth() / charWidth;
        int numRows = getHeight() / charHeight;

        bool isCleanState = (crush < 0.01f && downsample <= 1.01f);

        // LAYER A: THE CLEAN OSCILLOSCOPE MODE (Knobs at Zero)
        if (isCleanState)
        {
            g.setColour (juce::Colour (0xFF00FF66)); // Vibrant emerald-cyber green
            juce::Path wavePath;
            
            for (int x = 0; x < getWidth(); ++x)
            {
                int sampleIdx = juce::jmap (x, 0, getWidth(), 0, 1023);
                float yPos = (getHeight() / 2.0f) - (samples[sampleIdx] * (getHeight() / 2.5f));

                if (x == 0) wavePath.startNewSubPath (static_cast<float>(x), yPos);
                else        wavePath.lineTo (static_cast<float>(x), yPos);
            }
            g.strokePath (wavePath, juce::PathStrokeType (2.0f));
            return; 
        }

        // LAYER B: THE DEGRADED ASCII TERMINAL MODE WITH FFT FREQUENCY COLUMNS
        juce::String waveSymbol = "·";
        if (downsample > 2.0f)  waveSymbol = "-";
        if (downsample > 6.0f)  waveSymbol = "=";
        if (downsample > 12.0f) waveSymbol = "#";
        if (downsample > 24.0f) waveSymbol = "█"; 

        for (int col = 0; col < numCols; ++col)
        {
            // --- NEW FFT MAPPING MATH ---
            // Skew the columns exponentially so the bass frequencies get more screen space than the treble
            float colNormalized = static_cast<float>(col) / static_cast<float>(numCols);
            float skewedMapping = std::pow (colNormalized, 1.5f); 
            int fftBin = static_cast<int>(skewedMapping * 250); // Focus on the first 250 highly active bins
            fftBin = juce::jlimit (0, 511, fftBin);
            
            // This is the specific column frequency volume level!
            float columnFrequencyAmp = frequencies[fftBin];

            // Figure out where the foreground waveform character sits in this column
            int sampleIdx = juce::jmap (col, 0, numCols, 0, 1023);
            float normalizedSample = (samples[sampleIdx] + 1.0f) * 0.5f;
            int targetRow = static_cast<int>((1.0f - normalizedSample) * (numRows - 1));
            targetRow = juce::jlimit (0, numRows - 1, targetRow);

            for (int row = 0; row < numRows; ++row)
            {
                int xPos = col * charWidth;
                int yPos = row * charHeight;

                // DRAW FOREGROUND: The active audio wave position
                if (row == targetRow)
                {
                    g.setColour (juce::Colour (0xFF55FF55)); // Super bright neon green
                    g.drawText (waveSymbol, xPos, yPos, charWidth, charHeight, juce::Justification::centred);
                }
                // DRAW BACKGROUND MATRIX COLUMNS: Driven by active local frequencies!
                else
                {
                    float triggerChance = (static_cast<float>(rand() % 100) / 100.0f);
                    
                    // --- NEW MATRIX COEFFICIENT ---
                    // The activation height/probability of code rain in this specific column 
                    // is directly scaled by the exact frequency volume of the music playing!
                    float columnFactor = (static_cast<float>(numRows - row) / static_cast<float>(numRows));
                    float activationThreshold = columnFrequencyAmp * crush * (columnFactor * 1.8f);

                    if (triggerChance < activationThreshold)
                    {
                        char hackerChar = (rand() % 2 == 0) ? (char)(48 + (rand() % 10)) : (char)(65 + (rand() % 6)); 
                        
                        // Dim background numbers so they blend beautifully behind the wave line
                        g.setColour (juce::Colour (0xFF0F3A0F)); 
                        g.drawText (juce::String::charToString(hackerChar), xPos, yPos, charWidth, charHeight, juce::Justification::centred);
                    }
                }
            }
        }
    }
private:
    AudioPluginAudioProcessor& processorRef;

    // Order 10 means 2^10 = 1024 samples, perfectly matching our Step 1 FIFO size!
    juce::dsp::FFT forwardFFT { 10 }; 
    
    // The FFT algorithm requires a work array that is double the size of the block
    float fftData[2048] = { 0.0f }; 
    
    // Stores the calculated volume magnitudes for our frequency bars
    float frequencies[512] = { 0.0f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AsciiVisualizer)
};

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
