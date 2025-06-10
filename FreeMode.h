// === FreeMode.h ===
#pragma once
#include "OscMode.h"
#include <juce_dsp/juce_dsp.h>

class SimpleOscAudioProcessor; // forward declaration

class FreeMode : public OscMode
{
public:
    explicit FreeMode(SimpleOscAudioProcessor* proc); // updated constructor
    void prepare(double sampleRate) override;
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages,
                      bool isOn) override;
    void parameterChanged(const juce::String& paramID,
                          float newValue) override;

private:
    double currentSampleRate = 44100.0;
    float frequency = 0.0f;
    juce::SmoothedValue<float> smoothedFreq;
    juce::dsp::Oscillator<float> offsetOsc;
    juce::dsp::Oscillator<float> osc{[](float x){ return std::sin(x); }};
    bool snapOn = false;

    SimpleOscAudioProcessor* processor = nullptr; // new member
};

extern std::vector<float> snapFrequencies;

