/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleCompressionAudioProcessor::SimpleCompressionAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), treestate(*this, nullptr, "PARMETERS", createParameterLayout()), comp()
#endif
{
    treestate.addParameterListener("input gain", this);
    treestate.addParameterListener("output gain", this);
    treestate.addParameterListener("threshold", this);
    treestate.addParameterListener("ratio", this);
    treestate.addParameterListener("attack", this);
    treestate.addParameterListener("release", this);
}

SimpleCompressionAudioProcessor::~SimpleCompressionAudioProcessor()
{
    treestate.removeParameterListener("input gain", this);
    treestate.removeParameterListener("output gain", this);
    treestate.removeParameterListener("threshold", this);
    treestate.removeParameterListener("ratio", this);
    treestate.removeParameterListener("attack", this);
    treestate.removeParameterListener("release", this);
}

float SimpleCompressionAudioProcessor::calcAttack(float value)
{
    float returnvalue = 0;
    returnvalue = value * 3;
    return returnvalue;
}

float SimpleCompressionAudioProcessor::calcRelease(float value)
{
    float rvalue = 0;
    rvalue = 50 + (value * 25);
    return rvalue;
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleCompressionAudioProcessor::createParameterLayout()
{
    std::vector < std::unique_ptr<juce::RangedAudioParameter >> params;

    auto pInputGain = std::make_unique<juce::AudioParameterFloat>("input gain", "Input Gain", -24.0, 24.0, 0.0);
    auto pOutputGain = std::make_unique<juce::AudioParameterFloat>("output gain", "Output Gain", -24.0, 24.0, 0.0);

    auto pThreshold = std::make_unique<juce::AudioParameterFloat>("threshold", "Threshold", -36.0, 0.0, 0.0);
    auto pRatio = std::make_unique<juce::AudioParameterFloat>("ratio", "Ratio", 1.0, 10.0, 3.0);
    auto pAttack = std::make_unique<juce::AudioParameterFloat>("attack", "Attack", 0.0, 10.0, 3.0);
    auto pRelease = std::make_unique<juce::AudioParameterFloat>("release", "Release", 0.0, 10.0, 3.0);

    //params.push_back(std::move(pClipChoice));
    params.push_back(std::move(pInputGain));
    params.push_back(std::move(pOutputGain));
    params.push_back(std::move(pThreshold));
    params.push_back(std::move(pRatio));
    params.push_back(std::move(pAttack));
    params.push_back(std::move(pRelease));

    return { params.begin(), params.end() };

}

void SimpleCompressionAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{

    if (parameterID == "input gain")
    {
        inputrawGain = juce::Decibels::decibelsToGain(newValue);
    }
    if (parameterID == "output gain")
    {
        outputrawGain = juce::Decibels::decibelsToGain(newValue);
    }
    if (parameterID == "threshold")
    {
        threshold = juce::Decibels::decibelsToGain(newValue);
    }
    if (parameterID == "ratio")
    {
        ratio = newValue;
    }
    if (parameterID == "attack")
    {
        attack = calcAttack(newValue);
    }
    if (parameterID == "release")
    {
        release = calcRelease(newValue);
    }
}

//==============================================================================
const juce::String SimpleCompressionAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleCompressionAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleCompressionAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleCompressionAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleCompressionAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleCompressionAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleCompressionAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleCompressionAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleCompressionAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleCompressionAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleCompressionAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();
    spec.sampleRate = sampleRate;


    //satdistortion.reset();
    comp.prepare(spec);
    
    comp.reset();
    
    inputrawGain = juce::Decibels::decibelsToGain(static_cast<float>(*treestate.getRawParameterValue("input gain")));
    outputrawGain = juce::Decibels::decibelsToGain(static_cast<float>(*treestate.getRawParameterValue("output gain")));

    threshold = juce::Decibels::decibelsToGain(static_cast<float>(*treestate.getRawParameterValue("threshold")));
    ratio = *treestate.getRawParameterValue("ratio");
    attack = calcAttack(*treestate.getRawParameterValue("attack")); // make the range from 0-10 convert to solid ms range for attack
    release = calcRelease(*treestate.getRawParameterValue("release"));  //make the range from 0 - 10 convert to solid ms range for release

    updateCompressor();
}

void SimpleCompressionAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleCompressionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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
#endif

void SimpleCompressionAudioProcessor::updateCompressor()
{
    comp.setAttack(*treestate.getRawParameterValue("attack"));
    comp.setRelease(*treestate.getRawParameterValue("release"));
    comp.setRatio(*treestate.getRawParameterValue("ratio"));
    comp.setThreshold(*treestate.getRawParameterValue("threshold"));
}

void SimpleCompressionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // my audio block object
    juce::dsp::AudioBlock<float> block(buffer);

    // for loops grabbing all the samples from each channel
    for (int inchannel = 0; inchannel < block.getNumChannels(); ++inchannel)
    {
        auto* inchannelData = block.getChannelPointer(inchannel);
        for (int insample = 0; insample < block.getNumSamples(); ++insample)
        {
            // input gain control
            inchannelData[insample] *= inputrawGain;
        }
    }

    updateCompressor();
    comp.process(juce::dsp::ProcessContextReplacing <float>(block));

    for (int outchannel = 0; outchannel < block.getNumChannels(); ++outchannel)
    {
        auto* outchannelData = block.getChannelPointer(outchannel);
        for (int outsample = 0; outsample < block.getNumSamples(); ++outsample)
        {
            // output gain control
            outchannelData[outsample] *= outputrawGain;
        }

    }
}

//==============================================================================
bool SimpleCompressionAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleCompressionAudioProcessor::createEditor()
{
    //return new L2SaturationAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleCompressionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream stream(destData, false);
    treestate.state.writeToStream(stream);
}

void SimpleCompressionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));

    if (tree.isValid())
    {
        treestate.state = tree;
        threshold = juce::Decibels::decibelsToGain(static_cast<float>(*treestate.getRawParameterValue("threshold")));
        ratio = *treestate.getRawParameterValue("ratio");
        attack = calcAttack(*treestate.getRawParameterValue("attack"));
        release = calcRelease(*treestate.getRawParameterValue("release"));
        inputrawGain = juce::Decibels::decibelsToGain(static_cast<float>(*treestate.getRawParameterValue("input gain")));
        outputrawGain = juce::Decibels::decibelsToGain(static_cast<float>(*treestate.getRawParameterValue("output gain")));
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleCompressionAudioProcessor();
}
