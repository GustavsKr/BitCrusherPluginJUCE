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
    float bitDepth = *apvts.getRawParameterValue("BIT_DEPTH");
    int downsampleFactor = static_cast<int>(*apvts.getRawParameterValue("DOWNSAMPLE"));
    float mix = *apvts.getRawParameterValue("MIX");

    // Calculate how many discrete amplitude levels we have based on the bit depth.
    // Math formula: Total Levels = 2^bitDepth
    float totalLevels = std::pow(2.0f, bitDepth);

    
    // 2. Loop through each audio channel (Left and Right)
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // 3. Loop through every single sample in the current block
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

            // Increment counter, wrap it around based on our downsample factor
            mSampleCounter[channel] = (mSampleCounter[channel] + 1) % downsampleFactor;


            // --- STEP B: BIT DEPTH REDUCTION ---
            // Scale sample up to our "integer range", round it, and scale it back down.
            // Example: If totalLevels is 4, values clamp to -1.0, -0.5, 0.0, 0.5, 1.0
            processedSample = std::round(processedSample * totalLevels) / totalLevels;


            // --- STEP C: WET/DRY MIX ---
            // Linear blend: (Dry Signal * (1 - Mix)) + (Wet Signal * Mix)
            channelData[sample] = (rawSample * (1.0f - mix)) + (processedSample * mix);
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
    // return new AudioPluginAudioProcessorEditor (*this); UNCOMMENT LATER WHEN CREATING OUR GUI

    return new juce::GenericAudioProcessorEditor (*this);
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
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Bit Depth: 1 bit (pure noise) to 32 bits (pristine). Default to 16.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("BIT_DEPTH", 1), "Bit Depth", 1.0f, 32.0f, 16.0f));

    // Downsample: 1 (no downsampling) to 50 (hold every 50th sample). Default to 1.
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("DOWNSAMPLE", 1), "Downsample", 1, 50, 1));

    // Mix: 0.0 (completely dry) to 1.0 (completely wet). Default to 1.0.
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("MIX", 1), "Mix", 0.0f, 1.0f, 1.0f));

    return layout;
}