// === PluginProcessor.cpp ===
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FreeMode.h"

SimpleOscAudioProcessor::SimpleOscAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, juce::Identifier ("APVTS"), createParameterLayout())
{
    parameters.addParameterListener("snapOn", this);
    parameters.addParameterListener("freeFrequency", this);
    parameters.addParameterListener("isOn", this);
    parameters.addParameterListener("breathRate", this);
    parameters.addParameterListener("breathDepth", this);


    for (int i = 2; i <= 9; ++i) {
        juce::String id = "harmonic" + juce::String(i);
        parameters.addParameterListener(id, this);
    }
    parameters.addParameterListener("harmonicLevel", this);
    parameters.addParameterListener("atmoType", this);
    parameters.addParameterListener("atmoLevel", this);
    
    switchMode(0);
    
    
}

SimpleOscAudioProcessor::~SimpleOscAudioProcessor()
{
    for (int i = 2; i <= 9; ++i) {
        juce::String id = "harmonic" + juce::String(i);
        parameters.removeParameterListener(id, this);
    }
    parameters.removeParameterListener("harmonicLevel", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleOscAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("rangeMin", "Range Min", 0.0f, 20000.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("rangeMax", "Range Max", 0.0f, 20000.0f, 2222.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("freeFrequency", "Free Frequency", juce::NormalisableRange<float>(0.0f, 2222.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("volume", "Volume", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("isOn", "On/Off", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("snapOn", "Snap On", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "binauralOffset", "Binaural Offset", -15.0f, 15.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "binauralWidth", "Binaural Width", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("breathRate", "Breath Rate", 0.01f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("breathDepth", "Breath Depth", 0.0f, 1.0f, 0.5f));


    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonic2Level", "harmonic2Level", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonic3Level", "harmonic3Level", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonic4Level", "harmonic4Level", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonic5Level", "harmonic5Level", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonic6Level", "harmonic6Level", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonic7Level", "harmonic7Level", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonic8Level", "harmonic8Level", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonic9Level", "harmonic9Level", 0.0f, 1.0f, 0.5f));
    for (int i = 2; i <= 9; ++i) {
        juce::String id = "harmonic" + juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterBool>(id, id, false));
    }
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "atmoType", "Atmosphere Type",
        juce::NormalisableRange<float>(0.0f, 7.0f, 1.0f), 0.0f));  // 0=Off, 1=White, etc.
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "atmoLevel", "Atmosphere Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));
    
    return { params.begin(), params.end() };
}

void SimpleOscAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    if (currentMode)
        currentMode->prepare(sampleRate);
    modifierEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    modifierEngine.setModifierEnabled(0, false); // Ensure Binaural is active
    modifierEngine.setModifierEnabled(2, false); // Harmonics off by default
}

void SimpleOscAudioProcessor::releaseResources() {}

void SimpleOscAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    bool isOn = parameters.getRawParameterValue("isOn")->load() > 0.5f;
    if (currentMode)
        currentMode->processBlock(buffer, midi, isOn);

    modifierEngine.process(buffer);

    float volume = parameters.getRawParameterValue("volume")->load();
    buffer.applyGain(volume);
}

void SimpleOscAudioProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    if (currentMode)
        currentMode->parameterChanged(paramID, newValue);
    modifierEngine.parameterChanged(paramID, newValue);
}

void SimpleOscAudioProcessor::switchMode(int newMode)
{
    if (newMode == 0)
        currentMode = std::make_unique<FreeMode>(this); // pass 'this'

    if (currentMode)
    {
        currentMode->prepare(sampleRate);
        parameterChanged("freeFrequency", parameters.getRawParameterValue("freeFrequency")->load());
    }
}

juce::AudioProcessorEditor* SimpleOscAudioProcessor::createEditor()      { return new PluginEditor (*this); }
bool SimpleOscAudioProcessor::hasEditor() const                          { return true; }
void SimpleOscAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = parameters.copyState().createXml()) {
        copyXmlToBinary(*xml, destData);
    }
}
void SimpleOscAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes)) {
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()                   { return new SimpleOscAudioProcessor(); }
