// === OscMode.h ===
#pragma once
#include <JuceHeader.h>

/**
 * Abstract interface for oscillator modes.
 */
struct OscMode
{
    virtual ~OscMode() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void processBlock(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midiMessages,
                              bool isOn) = 0;
    virtual void parameterChanged(const juce::String& paramID,
                                  float newValue) = 0;
};
