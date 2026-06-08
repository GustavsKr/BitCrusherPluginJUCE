#pragma once

#include "../PluginProcessor.h"
#include <cmath>
#include <cstdlib>

class AsciiVisualizer : public juce::Component, 
                        public juce::Timer
{
public:
    explicit AsciiVisualizer (AudioPluginAudioProcessor& p) : processorRef (p)
    {
        startTimerHz (60); 
    }

    ~AsciiVisualizer() override
    {
        stopTimer();
    }

    void timerCallback() override 
    { 
        repaint(); 
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF050A05)); 

        auto samples = processorRef.getVisualizerSamples();
        float crush = processorRef.currentCrushVisual.load();
        float downsample = processorRef.currentDownsampleVisual.load();

        if (samples.size() < processorRef.visualizerSize) 
            return;

        // --- 1. SIGNAL ENVELOPE (LOUDNESS) ---
        float sampleSum = 0.0f;
        for (float s : samples) sampleSum += std::abs (s);
        float signalEnvelope = sampleSum / static_cast<float> (samples.size());
        signalEnvelope = juce::jlimit (0.0f, 1.0f, signalEnvelope * 4.0f);

        // --- 2. GRID GEOMETRY ---
        g.setFont (juce::FontOptions ("Courier New", 12.0f, juce::Font::plain));
        
        constexpr int charWidth = 8;
        constexpr int charHeight = 14;
        
        const int numCols = getWidth() / charWidth;
        const int numRows = getHeight() / charHeight;
        const int totalGridCells = numCols * numRows;
        const int maxSampleIndex = static_cast<int> (samples.size()) - 1;
        const bool useVectorWave = (downsample <= 1.01f);

        if (numCols <= 0 || numRows <= 0) return;

        // --- LAYER A: VECTOR LINE OSCILLOSCOPE ---
        if (useVectorWave)
        {
            g.setColour (juce::Colour (0xFF55FF77)); 
            juce::Path wavePath;
            for (int x = 0; x < getWidth(); ++x)
            {
                int sampleIdx = juce::jmap (x, 0, getWidth(), 0, maxSampleIndex);
                float yPos = (getHeight() / 2.0f) - (samples[sampleIdx] * (getHeight() / 2.5f));
                if (x == 0) wavePath.startNewSubPath (static_cast<float> (x), yPos);
                else        wavePath.lineTo (static_cast<float> (x), yPos);
            }
            g.strokePath (wavePath, juce::PathStrokeType (2.0f));
        }

        // --- LAYER B: STABLE ASCII OSCILLOSCOPE ---
        juce::String waveSymbol = "·";
        if (downsample > 2.0f)  waveSymbol = "-";
        if (downsample > 6.0f)  waveSymbol = "=";
        if (downsample > 12.0f) waveSymbol = "#";
        if (downsample > 24.0f) waveSymbol = "X";

        // Pre-calculate target rows for the waveform so the rain doesn't overwrite them
        std::vector<int> targetRowsPerCol (numCols, 0);
        
        for (int col = 0; col < numCols; ++col)
        {
            int sampleIdx = juce::jmap (col, 0, numCols, 0, maxSampleIndex);
            float normalizedSample = (samples[sampleIdx] + 1.0f) * 0.5f;
            int targetRow = static_cast<int> ((1.0f - normalizedSample) * (numRows - 1));
            targetRowsPerCol[col] = juce::jlimit (0, numRows - 1, targetRow);
        }

        // Draw the ASCII wave symbols across the screen cleanly (only when downsample is active)
        if (!useVectorWave)
        {
            g.setColour (juce::Colour (0xFF55FF55)); 
            for (int col = 0; col < numCols; ++col)
            {
                g.drawText (waveSymbol, col * charWidth, targetRowsPerCol[col] * charHeight, charWidth, charHeight, juce::Justification::centred);
            }
        }

        // --- LAYER C: BUDGET-CAPPED MATRIX RAIN ---
        const float targetDensityFraction = crush * signalEnvelope * 0.5f;
        const int letterBudget = static_cast<int> (totalGridCells * targetDensityFraction);

        if (letterBudget > 0)
        {
            int lettersDrawn = 0;

            for (int i = 0; i < totalGridCells; ++i)
            {
                if (lettersDrawn >= letterBudget) 
                    break;

                int scatteredIndex = (i * 103) % totalGridCells; 
                int col = scatteredIndex % numCols;
                int row = scatteredIndex / numCols;

                // Skip drawing rain drops over your active wave line
                if (row == targetRowsPerCol[col])
                    continue;

                float columnFactor = (static_cast<float> (numRows - row) / static_cast<float> (numRows));
                float triggerChance = (static_cast<float> (rand() % 100) / 100.0f);

                if (triggerChance < (0.30f * columnFactor))
                {
                    char hackerChar = (rand() % 2 == 0) ? (char)(48 + (rand() % 10)) : (char)(65 + (rand() % 6)); 
                    g.setColour (juce::Colour (0xFF0F3A0F));
                    
                    juce::String glyph (&hackerChar, 1);
                    g.drawText (glyph, col * charWidth, row * charHeight, charWidth, charHeight, juce::Justification::centred);
                    
                    lettersDrawn++; 
                }
            }
        }
    }

    void resized() override {}

private:
    AudioPluginAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AsciiVisualizer)
};