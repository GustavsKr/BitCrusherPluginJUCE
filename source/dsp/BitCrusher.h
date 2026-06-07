#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

class BitCrusher
{
public:
    BitCrusher() = default;
    ~BitCrusher() = default;

    // Allocate memory and reset smoothers based on DAW sample rate and channel count
    void prepare (double sampleRate, int numChannels)
    {
        mHeldSample.assign (numChannels, 0.0f);
        mSampleCounter.assign (numChannels, 0);
        mFilterState.assign (numChannels, 0.0f);

        smoothedMix.reset (sampleRate, 0.02);
        smoothedTone.reset (sampleRate, 0.02);

        // Pre-calculate 1-pole filter alpha for the ~1200Hz tilt point
        mFilterAlpha = static_cast<float> (2.0 * juce::MathConstants<double>::pi * 1200.0 / sampleRate);
        mFilterAlpha = juce::jlimit (0.001f, 0.99f, mFilterAlpha);
    }

    // Process the entire audio buffer using the incoming knob parameters
    void process (juce::AudioBuffer<float>& buffer, float crushAmount, int downsampleFactor, float tone, float mix)
    {
        int numSamples = buffer.getNumSamples();
        int totalChannels = buffer.getNumChannels();

        // Update target positions for smoothers
        smoothedTone.setTargetValue (tone);
        smoothedMix.setTargetValue (mix);

        // Map crush amount (0.0 to 1.0) to actual resolution (16 bits down to 2 bits)
        float bitDepth = juce::jmap (crushAmount, 0.0f, 1.0f, 16.0f, 2.0f);
        float totalLevels = std::pow (2.0f, bitDepth);

        // Pre-calculate both smoothing ramps for the entire block duration
        std::vector<float> smoothedMixValues (numSamples);
        std::vector<float> smoothedToneValues (numSamples);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            smoothedMixValues[sample] = smoothedMix.getNextValue();
            smoothedToneValues[sample] = smoothedTone.getNextValue();
        }

        // Run the processing loops
        for (int channel = 0; channel < totalChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                float rawSample = channelData[sample];
                float processedSample = rawSample;

                // --- STEP A: DOWNSAMPLING ---
                if (mSampleCounter[channel] == 0)
                {
                    mHeldSample[channel] = processedSample;
                }
                processedSample = mHeldSample[channel];
                mSampleCounter[channel] = (mSampleCounter[channel] + 1) % downsampleFactor;

                // --- STEP B: BIT DEPTH REDUCTION ---
                processedSample = std::round (processedSample * totalLevels) / totalLevels;

                // --- STEP C: SMOOTHED TILT EQ SHAPING ---
                float lp = mFilterState[channel] + mFilterAlpha * (processedSample - mFilterState[channel]);
                mFilterState[channel] = lp; 

                float hp = processedSample - lp;
                float currentTone = smoothedToneValues[sample];

                // 2.0x multiplier keeps the volume flat at center point
                processedSample = 2.0f * ((1.0f - currentTone) * lp + currentTone * hp);

                // --- STEP D: SMOOTHED WET/DRY MIX ---
                float currentMix = smoothedMixValues[sample];
                channelData[sample] = (rawSample * (1.0f - currentMix)) + (processedSample * currentMix);
            }
        }
    }

private:
    // Audio engine state vectors (one per channel)
    std::vector<float> mHeldSample;
    std::vector<int> mSampleCounter;
    std::vector<float> mFilterState;
    float mFilterAlpha = 0.0f;

    // Parameter parameter smoothing ramps
    juce::LinearSmoothedValue<float> smoothedMix;
    juce::LinearSmoothedValue<float> smoothedTone;
};