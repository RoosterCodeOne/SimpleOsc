#pragma once
#include <JuceHeader.h>

struct FreeSlider : public juce::Slider {
    using juce::Slider::Slider;

    FreeSlider() {
        setSliderStyle(juce::Slider::LinearVertical);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 60, 20);
        setSnapMode(false);
        setVelocityBasedMode(false);
    }

    void setSnapMode(bool enabled) {
        snapEnabled = enabled;
        repaint();
    }


    void setSnapFrequencies(const std::vector<float>& freqs) {
        snapFrequencies = freqs;
    }
    bool isSnapEnabled() const { return snapEnabled; }
    const std::vector<float>& getSnapFrequencies() const { return snapFrequencies; }

    juce::String getTextFromValue(double value) override {
        return value < 1.0 ? "OFF" : juce::String((int)value) + " Hz";
    }

    double freqMin = 0.0;
    double freqMax = 2222.0;

private:
    bool snapEnabled = false;
    std::vector<float> snapFrequencies;
};