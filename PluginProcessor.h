// === PluginProcessor.h ===
#pragma once
#include <JuceHeader.h>
#include "OscMode.h"
#include <memory>
#include "ModifierEngine.h"

class SimpleOscAudioProcessor  : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    SimpleOscAudioProcessor();
    ~SimpleOscAudioProcessor() override;

    // AudioProcessor overrides
    const juce::String getName() const override { return "SimpleOsc"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;
    
    float getRangeMin() const { return *parameters.getRawParameterValue("rangeMin"); }
    float getRangeMax() const { return *parameters.getRawParameterValue("rangeMax"); }

    void setRangeMinMax(float min, float max) {
        parameters.getParameter("rangeMin")->setValueNotifyingHost(parameters.getParameter("rangeMin")->convertTo0to1(min));
        parameters.getParameter("rangeMax")->setValueNotifyingHost(parameters.getParameter("rangeMax")->convertTo0to1(max));
    }
    void parameterChanged(const juce::String& paramID, float newValue) override;


    int lastMode = 0;
    
    ModifierEngine modifierEngine;
private:
    std::unique_ptr<OscMode> currentMode;
    double sampleRate = 44100.0;

    void switchMode(int newMode);
    void initializeModifiers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleOscAudioProcessor)
};
