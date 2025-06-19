//ModifierEngine.h
#pragma once

#include <JuceHeader.h>
#include "Modifier.h"
#include "DebugUtils.h"

class BinauralModifier : public Modifier {
public:
    void prepare(double sampleRate, int, int) override {
        this->sampleRate = sampleRate;
    }

    void process(juce::AudioBuffer<float>& buffer) override {
        juce::ignoreUnused(buffer);
    }

    void parameterChanged(const juce::String& paramID, float newValue) override {
        if (paramID == "binauralOffset") {
            offsetHz = newValue;  // expects -15 to +15 directly
        }
        else if (paramID == "binauralWidth") {
            stereoWidth = newValue * 2.0f - 1.0f; // Scale from 0.0–1.0 to -1.0–+1.0
        }
    }

    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }

    float getOffsetHz() const { return offsetHz; }
    float getStereoWidth() const { return stereoWidth; }

private:
    double sampleRate = 44100.0;
    float offsetHz = 0.0f;
    float stereoWidth = 1.0f;
    bool enabled = false;
};

class BreathModifier : public Modifier {
public:
    void prepare(double sampleRate, int, int numChannels) override {
        this->sampleRate = sampleRate;
        lfo.reset(sampleRate, 0.1);
        lfo.setTargetValue(0.0f);
        phase = 0.0f;
        lastNumChannels = numChannels;
    }

    void process(juce::AudioBuffer<float>& buffer) override {
        if (!enabled) return;

        const int numSamples = buffer.getNumSamples();
        const float phaseInc = juce::MathConstants<float>::twoPi * rate / sampleRate;

        for (int i = 0; i < numSamples; ++i) {
            float maxCut = 1.0f - depth; // depth = 1.0 → no cut, depth = 0.0 → full cut
            float gainMod = 1.0f - maxCut * 0.5f * (1.0f - std::cos(phase));
            for (int ch = 0; ch < lastNumChannels; ++ch) {
                buffer.setSample(ch, i, buffer.getSample(ch, i) * gainMod);
            }
            phase += phaseInc;
            if (phase >= juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }
    }

    void parameterChanged(const juce::String& paramID, float newValue) override {
        if (paramID == "breathRate") {
            rate = newValue;
        } else if (paramID == "breathDepth") {
            depth = newValue;
        }
    }

    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }

private:
    double sampleRate = 44100.0;
    float rate = 0.25f;
    float depth = 0.5f;
    float phase = 0.0f;
    int lastNumChannels = 2;
    bool enabled = false;
    juce::SmoothedValue<float> lfo;
};

class HarmonicModifier : public Modifier {
public:
    void prepare(double sampleRate, int, int) override {
        this->sampleRate = sampleRate;
    }

    void process(juce::AudioBuffer<float>& buffer, float baseFrequency) {

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i) {
            float sum = 0.0f;
            for (int h = 0; h < 8; ++h) {
                float gain = smoothedGains[h].getNextValue();
                if (gain > 0.0001f) {
                    float freq = baseFrequency * (float)(h + 2);
                    float phaseInc = freq * twoPi / sampleRate;
                    phases[h] += phaseInc;
                    if (phases[h] > twoPi) phases[h] -= twoPi;
                    sum += gain * harmonicLevels[h] * std::sin(phases[h]);
                }
            }
            float scaled = sum;

            for (int ch = 0; ch < numChannels; ++ch) {
                float orig = buffer.getSample(ch, i);
                buffer.setSample(ch, i, orig + scaled);
            }
        }
    }

    void process(juce::AudioBuffer<float>& buffer) override {
        juce::ignoreUnused(buffer); // Stub to satisfy abstract base class
    }

    void parameterChanged(const juce::String& paramID, float newValue) override {
        for (int i = 2; i <= 9; ++i) {
            juce::String toggleID = "harmonic" + juce::String(i);
            juce::String levelID = toggleID + "Level";

            if (paramID == toggleID) {
                int hIndex = i - 2;
                if (newValue > 0.5f) {
                    smoothedGains[hIndex].reset(sampleRate, harmonicAttackTime);
                    smoothedGains[hIndex].setTargetValue(1.0f);
                } else {
                    smoothedGains[hIndex].reset(sampleRate, harmonicReleaseTime);
                    smoothedGains[hIndex].setTargetValue(0.0f);
                }
            } else if (paramID == levelID) {
                int hIndex = i - 2;
                harmonicLevels[hIndex] = newValue;
            }
        }

    }
    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }
    
    void setSampleRate(double newRate)
    {
        sampleRate = newRate;
        for (auto& g : smoothedGains) {
            g.reset(sampleRate);
            g.setCurrentAndTargetValue(0.0f);
        }
    }

private:
    double sampleRate = 44100.0;
    std::array<float, 8> harmonicLevels = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    std::array<float, 8> phases = { 0 };
    std::array<juce::SmoothedValue<float>, 8> smoothedGains;
    static constexpr float harmonicAttackTime = 0.05f;
    static constexpr float harmonicReleaseTime = 1.5f;
    const float twoPi = juce::MathConstants<float>::twoPi;
    bool enabled = false;

};

class ModifierEngine {
public:
    void prepare(double sampleRate, int blockSize, int numChannels) {
        binaural.prepare(sampleRate, blockSize, numChannels);
        breath.prepare(sampleRate, blockSize, numChannels);
        harmonic.setSampleRate(sampleRate);
        harmonic.prepare(sampleRate, blockSize, numChannels);
    }

    void process(juce::AudioBuffer<float>& buffer) {
        binaural.process(buffer);
        breath.process(buffer);
    }

    void parameterChanged(const juce::String& id, float value) {
        binaural.parameterChanged(id, value);
        breath.parameterChanged(id, value);
        harmonic.parameterChanged(id, value);
    }

    void setModifierEnabled(int slotIndex, bool enable) {
        if (slotIndex == 0) binaural.setEnabled(enable);
        else if (slotIndex == 1) breath.setEnabled(enable);
        else if (slotIndex == 2) harmonic.setEnabled(enable);  // Slot 2 = harmonics
    }

    void process(juce::AudioBuffer<float>& buffer, float baseFrequency) {
        harmonic.process(buffer, baseFrequency);
    }

    bool isModifierEnabled(int slotIndex) const {
        if (slotIndex == 0) return binaural.isEnabled();
        else if (slotIndex == 1) return breath.isEnabled();
        else if (slotIndex == 2) return harmonic.isEnabled();
        return false;
    }

    float getOffsetHz() const { return binaural.getOffsetHz(); }
    float getStereoWidth() const { return binaural.getStereoWidth(); }

private:
    BinauralModifier binaural;
    BreathModifier breath;
    HarmonicModifier harmonic;
};
