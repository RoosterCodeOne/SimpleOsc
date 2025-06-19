// === FreeMode.cpp ===
#include "FreeMode.h"
#include <algorithm>
#include "PluginProcessor.h"
#include "DebugUtils.h"

std::vector<float> snapFrequencies = { 0.0f, 174.0f, 285.0f, 396.0f, 417.0f, 528.0f, 639.0f, 741.0f, 852.0f, 963.0f }; // Default preset list

FreeMode::FreeMode(SimpleOscAudioProcessor* proc)
    : processor(proc) {}

void FreeMode::prepare(double sampleRate)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec{ sampleRate, static_cast<juce::uint32>(512), static_cast<juce::uint32>(2) };
    osc.prepare(spec);
    offsetOsc.prepare(spec);
    offsetOsc.initialise([](float x) { return std::sin(x + juce::MathConstants<float>::halfPi); });
//    smoothedFreq.reset(sampleRate, 0.0015);
    smoothedFreq.setCurrentAndTargetValue(frequency);
    osc.setFrequency(frequency);
}

void FreeMode::processBlock(juce::AudioBuffer<float>& buffer,
                             juce::MidiBuffer&, bool isOn)
{
    if (!isOn || processor == nullptr)
    {
        buffer.clear();
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float mainFreq = smoothedFreq.getNextValue();
    bool binauralOn = processor->modifierEngine.isModifierEnabled(0);
    float offset = processor->modifierEngine.getOffsetHz();
    float widthAmount = processor->modifierEngine.getStereoWidth();

    buffer.clear();

    if (binauralOn && numChannels >= 2)
    {
        osc.setFrequency(mainFreq);
        offsetOsc.setFrequency(mainFreq + offset);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float left = osc.processSample(0.0f);
            float right = offsetOsc.processSample(0.0f);

            float mid = 0.5f * (left + right);
            float sideL = (left - mid) * widthAmount;
            float sideR = (right - mid) * widthAmount;

            buffer.setSample(0, sample, mid + sideL);
            buffer.setSample(1, sample, mid + sideR);
        }

        // Apply harmonics centered
        juce::AudioBuffer<float> harmonicBuffer;
        harmonicBuffer.setSize(numChannels, numSamples);
        harmonicBuffer.clear();
        processor->modifierEngine.process(harmonicBuffer, mainFreq);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addFrom(ch, 0, harmonicBuffer, ch, 0, numSamples);

        return;
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float currentFreq = smoothedFreq.getNextValue();

        if (snapOn && !snapFrequencies.empty()) {
            currentFreq = *std::min_element(snapFrequencies.begin(), snapFrequencies.end(),
                [currentFreq](float a, float b) {
                    return std::abs(a - currentFreq) < std::abs(b - currentFreq);
                });
        }

        if (currentFreq < 1.0f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, sample, 0.0f);
            continue;
        }

        osc.setFrequency(currentFreq);
        float val = osc.processSample(0.0f);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, sample, val);
    }
    // === Apply Breath (LFO) Gain Modulation AFTER Oscillator ===
    processor->modifierEngine.process(buffer);
    
    // Apply harmonics centered
    juce::AudioBuffer<float> harmonicBuffer;
    harmonicBuffer.setSize(numChannels, numSamples);
    harmonicBuffer.clear();
    processor->modifierEngine.process(harmonicBuffer, mainFreq);
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.addFrom(ch, 0, harmonicBuffer, ch, 0, numSamples);
}

void FreeMode::parameterChanged(const juce::String& paramID, float newValue)
{
    if (paramID == "freeFrequency")
    {
        frequency = newValue;
            smoothedFreq.setCurrentAndTargetValue(frequency);
    }
    else if (paramID == "snapOn")
    {
        snapOn = (newValue > 0.5f);
    }
}
