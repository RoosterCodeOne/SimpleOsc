// === ModifierSlot.h ===
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomSliderLookAndFeel.h"

class ModifierSlot : public juce::Component {
public:
    ModifierSlot(int index, SimpleOscAudioProcessor& proc);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void setBinauralState(bool isOn);

    // === Slot 0: Binaural ===
    std::unique_ptr<juce::Slider> offsetSlider;
    std::unique_ptr<juce::Slider> widthSlider;
    // === Slot 1: Breath ===
    std::unique_ptr<juce::Slider> breathRateSlider;
    std::unique_ptr<juce::Slider> breathDepthSlider;
    std::unique_ptr<juce::Label> breathRateLabel;
    std::unique_ptr<juce::Label> breathDepthLabel;


    // === Slot 2: Harmonics ===
    std::array<std::unique_ptr<HarmonicSlider>, 8> harmonicLevelSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> harmonicLevelSliderAttachments;


    // === Slot 3: Atmosphere ===
    std::unique_ptr<juce::TextButton> atmoSelector;
    std::unique_ptr<juce::Slider> atmoLevelSlider;
    
    // === Toggles ===
    std::unique_ptr<juce::ToggleButton> binauralToggle;
    std::unique_ptr<juce::ToggleButton> breathToggle;
    std::unique_ptr<juce::ToggleButton> atmoToggle;


private:
    int slotIndex;
    SimpleOscAudioProcessor& processor;

    std::unique_ptr<juce::Label> offsetLabel;
    std::unique_ptr<juce::Label> widthLabel;
    std::unique_ptr<juce::Label> valuePopup;
    
    void showValuePopupFromSlider(juce::Slider&);
    void hideValuePopup();


};
