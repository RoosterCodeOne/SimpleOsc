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

class AtmosphereModifier : public Modifier {
public:
    enum AtmosphereType {
        Off = 0,
        WhiteNoise = 1,
        PinkNoise = 2,
        Wind = 3,
        Rain = 4,
        Ocean = 5,
        Forest = 6,
        Birds = 7
    };

    void prepare(double sampleRate, int, int numChannels) override {
        this->sampleRate = sampleRate;
        this->numChannels = numChannels;
        
        // Initialize pink noise filter (simple first-order)
        pinkFilterState = 0.0f;
        
        // Simple filter states for different atmospheres
        windFilterState1 = 0.0f;
        windFilterState2 = 0.0f;
        rainFilterState = 0.0f;
        
        // Ocean wave oscillators - simple phase tracking
        for (int i = 0; i < 3; ++i) {
            oceanPhases[i] = 0.0f;
        }
        oceanFreqs[0] = 0.1f;
        oceanFreqs[1] = 0.3f;
        oceanFreqs[2] = 0.7f;
    }

    void process(juce::AudioBuffer<float>& buffer) override {
        if (currentType == Off || !enabled) return;
        
        const int numSamples = buffer.getNumSamples();
        
        // Generate atmosphere audio
        for (int sample = 0; sample < numSamples; ++sample) {
            float atmosphereValue = generateAtmosphereSample();
            
            // Apply gain (converted from dB)
            float finalGain = juce::Decibels::decibelsToGain(gainDb);
            atmosphereValue *= finalGain;
            
            // Add to all channels
            for (int ch = 0; ch < numChannels; ++ch) {
                float currentSample = buffer.getSample(ch, sample);
                buffer.setSample(ch, sample, currentSample + atmosphereValue);
            }
        }
    }

    void parameterChanged(const juce::String& paramID, float newValue) override {
        if (paramID == "atmoType") {
            currentType = static_cast<AtmosphereType>(static_cast<int>(newValue));
            DBG("Atmosphere type changed to: " << static_cast<int>(currentType));
        } else if (paramID == "atmoLevel") {
            // Convert from 0.0-1.0 to -inf to 0.0 dB
            if (newValue <= 0.0001f) {
                gainDb = -60.0f; // Effectively -inf
            } else {
                gainDb = 20.0f * std::log10(newValue); // Convert to dB
            }
            DBG("Atmosphere level changed to: " << gainDb << " dB");
        }
    }

    void setEnabled(bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }

private:
    double sampleRate = 44100.0;
    int numChannels = 2;
    AtmosphereType currentType = Off;
    float gainDb = -12.0f; // Start at -12dB (quiet background)
    bool enabled = false;
    
    // Audio generation components
    juce::Random random;
    
    // Simple filter states for different atmospheres
    float pinkFilterState = 0.0f;
    float windFilterState1 = 0.0f;
    float windFilterState2 = 0.0f;
    float rainFilterState = 0.0f;
    
    // Ocean oscillator phases and frequencies
    float oceanPhases[3];
    float oceanFreqs[3];
    
    float generateAtmosphereSample() {
        switch (currentType) {
            case Off:
                return 0.0f;
                
            case WhiteNoise:
                return generateWhiteNoise();
                
            case PinkNoise:
                return generatePinkNoise();
                
            case Wind:
                return generateWind();
                
            case Rain:
                return generateRain();
                
            case Ocean:
                return generateOcean();
                
            case Forest:
                return generateForest();
                
            case Birds:
                return generateBirds();
                
            default:
                return 0.0f;
        }
    }
    
    float generateWhiteNoise() {
        return random.nextFloat() * 2.0f - 1.0f;
    }
    
    float generatePinkNoise() {
        // Simple pink noise approximation using first-order filter
        float white = generateWhiteNoise();
        pinkFilterState = 0.99f * pinkFilterState + 0.01f * white;
        return pinkFilterState * 3.0f; // Boost since filtering reduces amplitude
    }
    
    float generateWind() {
        float noise = generateWhiteNoise();
        
        // Much gentler wind - lower frequency rumble
        float cutoff = 0.005f; // Even lower cutoff for deeper rumble
        windFilterState1 += cutoff * (noise - windFilterState1);
        windFilterState2 += cutoff * (windFilterState1 - windFilterState2);
        
        // Add some very gentle mid-frequency content
        static float windMid = 0.0f;
        windMid += 0.02f * (noise - windMid);
        
        return (windFilterState2 * 1.5f + windMid * 0.3f) * 0.4f; // Much quieter
    }
    
    float generateRain() {
        float noise = generateWhiteNoise();
        
        // Gentler rain - less harsh filtering
        float highpass = noise - rainFilterState;
        rainFilterState += 0.03f * (noise - rainFilterState); // Gentler filter
        
        // Much less frequent droplet spikes
        if (random.nextFloat() < 0.0003f) { // Way less frequent
            highpass += (random.nextFloat() * 2.0f - 1.0f) * 0.15f; // Quieter spikes
        }
        
        return highpass * 0.2f; // Much quieter overall
    }
    
    float generateOcean() {
        // Much gentler ocean waves
        float waves = 0.0f;
        
        for (int i = 0; i < 3; ++i) {
            waves += std::sin(oceanPhases[i]) * (0.15f - i * 0.05f); // Quieter waves
            oceanPhases[i] += 2.0f * juce::MathConstants<float>::pi * oceanFreqs[i] / static_cast<float>(sampleRate);
            if (oceanPhases[i] > 2.0f * juce::MathConstants<float>::pi) {
                oceanPhases[i] -= 2.0f * juce::MathConstants<float>::pi;
            }
        }
        
        // Very gentle background noise
        float noise = generateWhiteNoise() * 0.03f;
        
        return (waves + noise) * 0.6f; // Much gentler overall
    }
    
    float generateForest() {
        // Much gentler forest - like distant rustling
        float noise = generateWhiteNoise() * 0.5f; // Start with quieter noise
        
        // Very gentle filtering
        static float forestLow = 0.0f;
        static float forestHigh = 0.0f;
        
        forestLow += 0.02f * (noise - forestLow); // Gentler filtering
        forestHigh = noise - forestLow;
        
        return (forestLow * 0.6f + forestHigh * 0.1f) * 0.3f; // Much quieter
    }
    
    float generateBirds() {
        // Much gentler birds - like distant chirping
        float noise = generateWhiteNoise() * 0.3f; // Start quieter
        
        // Gentler filtering
        static float birdsLow = 0.0f;
        birdsLow += 0.1f * (noise - birdsLow); // Less aggressive filtering
        float chirpy = noise - birdsLow;
        
        // Much less frequent and gentler chirps
        if (random.nextFloat() < 0.0005f) { // Much less frequent
            chirpy += (random.nextFloat() * 2.0f - 1.0f) * 0.2f; // Gentler chirps
        }
        
        return chirpy * 0.15f; // Much quieter overall
    }
};

class ModifierEngine {
public:
    void prepare(double sampleRate, int blockSize, int numChannels) {
        binaural.prepare(sampleRate, blockSize, numChannels);
        breath.prepare(sampleRate, blockSize, numChannels);
        harmonic.setSampleRate(sampleRate);
        harmonic.prepare(sampleRate, blockSize, numChannels);
        atmosphere.prepare(sampleRate, blockSize, numChannels);  // Add this line
    }

    void process(juce::AudioBuffer<float>& buffer) {
        // Add atmosphere to the buffer (it will be affected by breath LFO later)
        atmosphere.process(buffer);
        
        // Apply binaural processing (only affects main oscillator, not atmosphere)
        binaural.process(buffer);
        
        // Apply breath LFO last (affects everything including atmosphere)
        breath.process(buffer);
    }

    void parameterChanged(const juce::String& id, float value) {
        binaural.parameterChanged(id, value);
        breath.parameterChanged(id, value);
        harmonic.parameterChanged(id, value);
        atmosphere.parameterChanged(id, value);  // Add this line
    }

    void setModifierEnabled(int slotIndex, bool enable) {
        if (slotIndex == 0) binaural.setEnabled(enable);
        else if (slotIndex == 1) breath.setEnabled(enable);
        else if (slotIndex == 2) harmonic.setEnabled(enable);
        else if (slotIndex == 3) atmosphere.setEnabled(enable);  // Add this line
    }

    void process(juce::AudioBuffer<float>& buffer, float baseFrequency) {
        harmonic.process(buffer, baseFrequency);
    }

    bool isModifierEnabled(int slotIndex) const {
        if (slotIndex == 0) return binaural.isEnabled();
        else if (slotIndex == 1) return breath.isEnabled();
        else if (slotIndex == 2) return harmonic.isEnabled();
        else if (slotIndex == 3) return atmosphere.isEnabled();  // Add this line
        return false;
    }

    float getOffsetHz() const { return binaural.getOffsetHz(); }
    float getStereoWidth() const { return binaural.getStereoWidth(); }

private:
    BinauralModifier binaural;
    BreathModifier breath;
    HarmonicModifier harmonic;
    AtmosphereModifier atmosphere;  // Add this line
};
