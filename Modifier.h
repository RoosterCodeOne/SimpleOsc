// ===== File: Modifier.h =====
#pragma once

#include <JuceHeader.h>

class Modifier {
public:
    virtual ~Modifier() = default;
    virtual void prepare(double sampleRate, int samplesPerBlock, int numChannels) = 0;
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;
    virtual void parameterChanged(const juce::String& paramID, float newValue) = 0;

    void setActive(bool shouldBeOn) { active = shouldBeOn; }
    bool isActive() const { return active; }

protected:
    bool active = true; // 🔁 default to on unless overridden
};
