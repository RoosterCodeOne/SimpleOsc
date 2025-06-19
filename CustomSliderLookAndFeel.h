//CustomSliderLookAndFeel.h//

#pragma once
#include <JuceHeader.h>
#include "FreeSlider.h"

class CenteredSliderLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override {
        auto value = slider.getValue();
        float norm = juce::jmap<float>(value, slider.getMinimum(), slider.getMaximum(), 0.0f, 1.0f);
        sliderPos = juce::jmap(norm, (float)y + height - 2.0f, (float)y + 2.0f); // invert drag direction
        int sidePadding = 2;
        int topBottomPadding = 2;
        auto trackBounds = juce::Rectangle<float>((float)x + sidePadding, (float)y + topBottomPadding, (float)width - 2 * sidePadding, (float)height - 2 * topBottomPadding);
        auto filled = trackBounds.withTop(sliderPos);

        // === Snap Mode Visual Debug ===
        bool usingSnapRange = false;
        if (auto* fs = dynamic_cast<FreeSlider*>(&slider)) {
            if (fs->isSnapEnabled()) {
                usingSnapRange = true;
            }

        }

        // Background track
        g.setColour(usingSnapRange ? juce::Colours::red.withAlpha(0.5f) : juce::Colours::darkgrey.withAlpha(0.1f));
        g.fillRoundedRectangle(trackBounds, 6.0f);

        // Filled portion (value)
        g.setColour(juce::Colours::aqua);
        g.fillRoundedRectangle(filled, 6.0f);

        // Optional thumb circle
        auto thumbRadius = 6.0f;
        g.setColour(juce::Colours::white);
        g.fillEllipse(trackBounds.getCentreX() - thumbRadius, sliderPos - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);

        // Text overlay
        auto text = slider.getTextFromValue(value);
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawFittedText(text, trackBounds.toNearestInt(), juce::Justification::centred, 1);

        // Tick marks
        if (auto* fs = dynamic_cast<FreeSlider*>(&slider))
        {
            if (fs->isSnapEnabled())
            {
                g.setColour(juce::Colours::white.withAlpha(0.3f));
                for (auto snapVal : fs->getSnapFrequencies()) {
                    if (snapVal < slider.getMinimum() || snapVal > slider.getMaximum()) continue;
                    float snapNorm = juce::jmap<float>(snapVal, slider.getMinimum(), slider.getMaximum(), 0.0f, 1.0f);
                    float yTick = juce::jmap(snapNorm, (float)y + height - 2.0f, (float)y + 2.0f);
                    g.drawLine((float)x + 4.0f, yTick, (float)x + width - 4.0f, yTick, 1.0f);
                }
            }
        }
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override {
        juce::Slider::SliderLayout layout;
        layout.sliderBounds = slider.getLocalBounds();
        return layout;
    }
};

class HarmonicSlider : public juce::Slider
{
public:
    HarmonicSlider(int harmonicIndex, std::function<void(int, bool)> onToggleCallback)
        : index(harmonicIndex), toggleCallback(onToggleCallback)
    {
        setSliderStyle(LinearVertical);
        setTextBoxStyle(NoTextBox, false, 0, 0);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            if (e.getNumberOfClicks() == 1)
            {
                isOn = !isOn;
                if (toggleCallback)
                    toggleCallback(index, isOn);
                repaint();
            }
        }

        // only allow dragging when on
        if (isOn)
            juce::Slider::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isOn)
            juce::Slider::mouseDrag(e);
    }

    void paint(juce::Graphics& g) override
    {
        // Base background
        auto area = getLocalBounds().toFloat();
        g.setColour(baseColour.withAlpha(0.2f));
        g.fillRoundedRectangle(area, 4.0f);

        // Fill area if on
        if (isOn)
        {
            float fillRatio = (float) (getValue() - getMinimum()) / (getMaximum() - getMinimum());
            auto fillArea = area;
            fillArea.setY(area.getBottom() - area.getHeight() * fillRatio);
            fillArea.setHeight(area.getHeight() * fillRatio);

            g.setColour(baseColour.withAlpha(0.9f));
            g.fillRoundedRectangle(fillArea, 4.0f);
        }
        else
        {
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillRoundedRectangle(area, 4.0f);
        }
    }

    void setBaseColour(juce::Colour c) { baseColour = c; repaint(); }
    bool getToggleState() const { return isOn; }

private:
    int index;
    bool isOn = false;
    juce::Colour baseColour;
    std::function<void(int, bool)> toggleCallback;
};

