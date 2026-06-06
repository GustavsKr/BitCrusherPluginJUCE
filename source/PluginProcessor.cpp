#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                    ),
                    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    auto numChannels = getTotalNumInputChannels();
    
    // Resize our vectors to match the number of audio channels (usually 2 for stereo)
    mHeldSample.assign(numChannels, 0.0f);
    mSampleCounter.assign(numChannels, 0);
    mFilterState.resize(numChannels, 0.0f);

    // Initialize Visualizer Buffers
    fifoBuffer.assign (fftSize, 0.0f);
    visualizerStorage.assign (fftSize, 0.0f);
    fifoIndex = 0;
    fifoReady = false;

    // Initialize both smoothers to glide over 20 milliseconds (0.02s) - only for Tone and Mix knobs
    smoothedMix.reset(sampleRate, 0.02);
    smoothedTone.reset(sampleRate, 0.02);
    // Calculate a stable 1-pole filter coefficient for a ~1200Hz tilt point
    // This scales automatically whether the DAW runs at 44.1kHz, 48kHz, or 96kHz.
    mFilterAlpha = static_cast<float>(2.0 * juce::MathConstants<double>::pi * 1200.0 / sampleRate);
    mFilterAlpha = juce::jlimit(0.001f, 0.99f, mFilterAlpha); // Keep math safe
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    // 1. Fetch current parameter values from the APVTS
    float crushAmount = *apvts.getRawParameterValue("CRUSH");
    float bitDepth = juce::jmap(crushAmount, 0.0f, 1.0f, 16.0f, 2.0f); // Map 0.0 (Left) to 16 bits, and 1.0 (Right) to 2 bits
    int downsampleFactor = static_cast<int>(*apvts.getRawParameterValue("DOWNSAMPLE"));
    float tone = *apvts.getRawParameterValue("TONE");
    smoothedTone.setTargetValue(tone);
    float mix = *apvts.getRawParameterValue("MIX");
    smoothedMix.setTargetValue(mix);

    // Calculate how many discrete amplitude levels we have based on the bit depth.
    // Math formula: Total Levels = 2^bitDepth
    float totalLevels = std::pow(2.0f, bitDepth);

    // 2. Pre-calculate BOTH smoothing ramps for the entire block
    int numSamples = buffer.getNumSamples();
    std::vector<float> smoothedMixValues(numSamples);
    std::vector<float> smoothedToneValues(numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Generate the ramp steps. This advances the smoother safely for the whole block.
        smoothedMixValues[sample] = smoothedMix.getNextValue();
        smoothedToneValues[sample] = smoothedTone.getNextValue();
    }

    // 3. Main Audio Channel Processing Loops
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float rawSample = channelData[sample];
            float processedSample = rawSample;

            // --- STEP A: DOWNSAMPLING ---
            // We only grab a new sample value when our counter hits 0.
            if (mSampleCounter[channel] == 0)
            {
                mHeldSample[channel] = processedSample;
            }
            
            // Overwrite the current sample with the one we are "holding"
            processedSample = mHeldSample[channel];
            mSampleCounter[channel] = (mSampleCounter[channel] + 1) % downsampleFactor;


            // --- STEP B: BIT DEPTH REDUCTION ---
            // Scale sample up to our "integer range", round it, and scale it back down.
            // Example: If totalLevels is 4, values clamp to -1.0, -0.5, 0.0, 0.5, 1.0
            processedSample = std::round(processedSample * totalLevels) / totalLevels;


            // --- STEP C: SMOOTHED TILT EQ SHAPING ---
            // Extract the low frequencies using our 1-pole filter math
            float lp = mFilterState[channel] + mFilterAlpha * (processedSample - mFilterState[channel]);
            mFilterState[channel] = lp; // Update filter memory

            // High frequencies are whatever is left over when you subtract the lows
            float hp = processedSample - lp;

            // Grab the current smooth knob position for this specific sample
            float currentTone = smoothedToneValues[sample];

            // Crossfade math: 2.0x multiplier keeps the volume perfectly unity/flat at 0.5
            processedSample = 2.0f * ((1.0f - currentTone) * lp + currentTone * hp);

            // --- STEP D: SMOOTHED WET/DRY MIX ---
            float currentMix = smoothedMixValues[sample];
            channelData[sample] = (rawSample * (1.0f - currentMix)) + (processedSample * currentMix);
        }
    }

    // If GUI is active (plugin not closed), PUSH data to the visualiser  
    if (isGuiActive.load())
    {
        // 1. Broadcast parameter levels to the visualizer
        currentCrushVisual.store(crushAmount);
        currentDownsampleVisual.store(static_cast<float>(downsampleFactor));

        // 2. Calculate the average loudness (RMS) of the block (Mono fallback)
        float rms = buffer.getRMSLevel(0, 0, numSamples);
        currentRmsLevel.store(rms);

        // 3. Collect samples into our FIFO ring buffer (using Left Channel)
        auto* leftChannel = buffer.getReadPointer(0);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            fifoBuffer[fifoIndex] = leftChannel[sample];
            fifoIndex++;

            // If our buffer bucket is full (hit 1024 samples), lock it and swap arrays!
            if (fifoIndex >= fftSize)
            {
                const juce::ScopedLock sl (fifoCriticalSection);
                visualizerStorage = fifoBuffer; // Thread-safe handover
                fifoIndex = 0;
            }
        }
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);

    // return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Change "BIT_DEPTH" to "CRUSH". Range is 0.0 (Clean) to 1.0 (Destroyed)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "CRUSH", 
        "Crush", 
        0.0f, 
        1.0f, 
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>("DOWNSAMPLE", "Downsample", 1, 32, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("TONE", "Tone", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", 0.0f, 1.0f, 0.5f));

    return { params.begin(), params.end() };
}

std::vector<float> AudioPluginAudioProcessor::getVisualizerSamples()
{
    const juce::ScopedLock sl (fifoCriticalSection);
    return visualizerStorage;
}