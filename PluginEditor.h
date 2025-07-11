// === PluginEditor.h ===
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ModifierSlot.h"
#include "SettingsWindow.h"
#include "FreeSlider.h"
#include <random>    // Add this for std::mt19937
#include <fstream>

class PluginEditor  : public juce::AudioProcessorEditor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::Timer
{
public:
    PluginEditor (SimpleOscAudioProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& e) override;
    void parameterChanged(const juce::String& paramID, float newValue) override;
    void setFrequencyRange(double min, double max);
    void applySnapPreset(const juce::String& name);
    void timerCallback() override;


private:
    SimpleOscAudioProcessor& processor;
    juce::Image carvedBackground;
    juce::Image backgroundImage;
    juce::Rectangle<int> uiArea;
    juce::Rectangle<int> carvedArea;
    juce::Rectangle<float> topRowBlock1, topRowBlock2, topRowBlock3, topRowBlock4;
    juce::Rectangle<float> midRowBlock1, midRowBlock2;
    juce::Rectangle<float> sliderBlock;
    juce::Rectangle<float> modBlock1, modBlock2, modBlock3, modBlock4;

    bool snapModeEnabled = false;

    juce::ToggleButton snapToggle;
    FreeSlider freqSlider;
    juce::Slider volumeSlider;
    juce::ToggleButton onOffButton;
    juce::TextButton settingsButton { "⚙" };
    std::array<std::unique_ptr<ModifierSlot>, 4> modifierSlots;
    std::map<juce::String, std::vector<float>> snapPresets = {
        {"Deep Sleep",      {0.0f, 40.0f, 50.0f, 62.0f, 108.0f, 120.0f, 136.1f, 174.0f, 285.0f}},
        {"Solfeggio (Default)", {0.0f, 174.0f, 285.0f, 396.0f, 417.0f, 528.0f, 639.0f, 741.0f, 852.0f, 963.0f}},
        {"Mood Lifter",      {0.0f, 136.1f, 432.0f, 528.0f, 852.0f, 888.0f, 963.0f}},
        {"Anxiety Buster",   {0.0f, 111.0f, 136.1f, 396.0f, 417.0f, 444.0f, 528.0f, 639.0f, 741.0f}},
        {"Focus Mode",       {0.0f, 40.0f, 111.0f, 144.72f, 396.0f, 417.0f, 528.0f, 888.0f, 963.0f}},
    };
    juce::TextButton snapPackSelector;
    juce::TextButton rangeSelector;
    juce::PopupMenu snapPackMenu;
    juce::PopupMenu rangeMenu;
    juce::String currentSnapLabel { "Solfeggio (Default)" };
    juce::String currentRangeLabel { "0-2222 Hz (Default)" };

    std::unique_ptr<SnapPackManager> snapPackManager;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> snapToggleAttachment;
    std::unique_ptr<SettingsWindow> settingsWindow;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onOffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> binauralOffsetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> binauralWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> breathRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> breathDepthAttachment;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> harmonicToggleAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> atmoLevelAttachment;

    struct Particle
    {
        float x, y;           // Position
        float vx, vy;         // Velocity
        float size;           // Size
        float alpha;          // Transparency
        float life;           // Life remaining (0-1)
        float maxLife;        // Maximum life
        juce::Colour color;   // Particle color
    };

    std::vector<Particle> particles;
    std::mt19937 randomEngine;
    float gradientRotation = 0.0f;
    float backgroundTime = 0.0f;
    
    // Meditative color palette
    std::vector<juce::Colour> meditativeColors = {
        juce::Colour(0xff2d1b69), // Deep purple
        juce::Colour(0xff44318d), // Medium purple
        juce::Colour(0xff5e3c99), // Light purple
        juce::Colour(0xff3c6e71), // Teal
        juce::Colour(0xff284b63), // Deep blue
        juce::Colour(0xff353535), // Charcoal
        juce::Colour(0xff1e3a5f)  // Navy
    };
    
    // Background animation methods
    void initializeBackgroundParticles();
    void updateBackgroundParticles();
    void paintMeditativeBackground(juce::Graphics& g);
    void createNewParticle();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
