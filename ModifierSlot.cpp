// === ModifierSlot.cpp ===
#include "ModifierSlot.h"
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomSliderLookAndFeel.h"

extern CenteredSliderLookAndFeel centeredLook;

juce::Colour getHarmonicColor(int index)
{
    static std::array<juce::Colour, 8> colors = {
        juce::Colours::red,
        juce::Colours::orange,
        juce::Colours::yellow,
        juce::Colours::green,
        juce::Colours::blue,
        juce::Colours::indigo,
        juce::Colours::violet,
        juce::Colours::pink
    };
    return colors[index % colors.size()];
}

ModifierSlot::ModifierSlot(int index, SimpleOscAudioProcessor& proc)
    : slotIndex(index), processor(proc)
{
    if (slotIndex == 0) { // Only slot 0 has UI for Binaural
    
    binauralToggle = std::make_unique<juce::ToggleButton>("Binaural");
    binauralToggle->setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(*binauralToggle);
    
    offsetSlider = std::make_unique<juce::Slider>();
    offsetSlider->setSliderStyle(juce::Slider::LinearVertical);
    offsetSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    offsetSlider->setRange(-15.0, 15.0, 0.1);  // ✅ Real-world ±15 Hz range
    offsetSlider->setValue(0.0);
    offsetSlider->setTooltip("Adjusts frequency difference between ears: ±15 Hz");
    addAndMakeVisible(*offsetSlider);
    offsetSlider->onValueChange = [this]() {
        showValuePopupFromSlider(*offsetSlider);
    };
    offsetSlider->addMouseListener(this, true);
    
    widthSlider = std::make_unique<juce::Slider>();
    widthSlider->setSliderStyle(juce::Slider::LinearVertical);
    widthSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    widthSlider->setRange(0.0, 1.0, 0.01);
    widthSlider->setValue(1.0);
    widthSlider->setTooltip("Controls stereo image spread: -1 (mono) to +1 (wide)");
    addAndMakeVisible(*widthSlider);
    widthSlider->onValueChange = [this]() {
        showValuePopupFromSlider(*widthSlider);
    };
    widthSlider->addMouseListener(this, true);
    
    offsetLabel = std::make_unique<juce::Label>();
    offsetLabel->setText("Offset", juce::dontSendNotification);
    offsetLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*offsetLabel);
    offsetLabel->setInterceptsMouseClicks(false, false);
    
    widthLabel = std::make_unique<juce::Label>();
    widthLabel->setText("Width", juce::dontSendNotification);
    widthLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*widthLabel);
    widthLabel->setInterceptsMouseClicks(false, false);
}
    
    if (slotIndex == 1) {
        breathToggle = std::make_unique<juce::ToggleButton>("Breath");
        breathToggle->setToggleState(false, juce::dontSendNotification); // Default OFF
        addAndMakeVisible(*breathToggle);

        breathRateSlider = std::make_unique<juce::Slider>();
        breathRateSlider->setSliderStyle(juce::Slider::LinearVertical);
        breathRateSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        breathRateSlider->setRange(0.01, 1.0, 0.01);
        breathRateSlider->setValue(0.25);
        addAndMakeVisible(*breathRateSlider);

        breathDepthSlider = std::make_unique<juce::Slider>();
        breathDepthSlider->setSliderStyle(juce::Slider::LinearVertical);
        breathDepthSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        breathDepthSlider->setRange(1.0, 0.0, 0.01);
        breathDepthSlider->setValue(0.2);

        addAndMakeVisible(*breathDepthSlider);

        breathRateLabel = std::make_unique<juce::Label>();
        breathRateLabel->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*breathRateLabel);

        breathDepthLabel = std::make_unique<juce::Label>();
        breathDepthLabel->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*breathDepthLabel);

        breathRateSlider->onValueChange = [this]() {
            float hz = breathRateSlider->getValue();
            breathRateLabel->setText(juce::String(hz, 2) + " Hz", juce::dontSendNotification);
        };

        breathDepthSlider->onValueChange = [this]() {
            float norm = breathDepthSlider->getValue();  // 1.0 (top) to 0.0 (bottom)
            float gain = norm; // now gain = 1.0 at top, 0.0 at bottom
            float dbCut = (gain > 0.0001f) ? 20.0f * std::log10(gain) : -15.0f;
            dbCut = juce::jlimit(-15.0f, 0.0f, dbCut);
            breathDepthLabel->setText(juce::String(dbCut, 1) + " dB", juce::dontSendNotification);
        };

        breathRateSlider->onValueChange();
        breathDepthSlider->onValueChange();
    }

    // === Slot 2: Harmonics ===
    if (slotIndex == 2) {
        for (int i = 0; i < 8; ++i)
        {
            auto slider = std::make_unique<HarmonicSlider>(i, [this](int hIndex, bool enabled) {
                juce::String paramID = "harmonic" + juce::String(hIndex + 2);
                processor.parameters.getParameter(paramID)->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
            });

            slider->setRange(0.0, 1.0, 0.01);
            slider->setBaseColour(getHarmonicColor(i));
            slider->setLookAndFeel(&centeredLook);
            addAndMakeVisible(*slider);

            juce::String levelID = "harmonic" + juce::String(i + 2) + "Level";
            harmonicLevelSliderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.parameters, levelID, *slider);

            harmonicLevelSliders[i] = std::move(slider);
        }
    }
    // === Slot 3: Atmosphere ===
    if (slotIndex == 3) {
        // Remove the atmoToggle - selector will handle on/off
            
        atmoSelector = std::make_unique<juce::TextButton>("Off");  // Start with "Off"
        atmoSelector->onClick = [this]() {
            juce::PopupMenu menu;
            menu.addItem(1, "Off");
            menu.addItem(2, "White Noise");
            menu.addItem(3, "Pink Noise");
            menu.addItem(4, "Wind");
            menu.addItem(5, "Rain");
            menu.addItem(6, "Ocean");
            menu.addItem(7, "Forest");
            menu.addItem(8, "Birds");
            
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(atmoSelector.get()),
                [this](int result) {
                    if (result == 1) {
                        atmoSelector->setButtonText("Off");
                        processor.parameters.getParameter("atmoType")->setValueNotifyingHost(0.0f);
                        processor.modifierEngine.setModifierEnabled(3, false);
                    }
                    else if (result == 2) {
                        atmoSelector->setButtonText("White Noise");
                        processor.parameters.getParameter("atmoType")->setValueNotifyingHost(1.0f);
                        processor.modifierEngine.setModifierEnabled(3, true);
                    }
                    else if (result == 3) {
                        atmoSelector->setButtonText("Pink Noise");
                        processor.parameters.getParameter("atmoType")->setValueNotifyingHost(2.0f);
                        processor.modifierEngine.setModifierEnabled(3, true);
                    }
                    else if (result == 4) {
                        atmoSelector->setButtonText("Wind");
                        processor.parameters.getParameter("atmoType")->setValueNotifyingHost(3.0f);
                        processor.modifierEngine.setModifierEnabled(3, true);
                    }
                    else if (result == 5) {
                        atmoSelector->setButtonText("Rain");
                        processor.parameters.getParameter("atmoType")->setValueNotifyingHost(4.0f);
                        processor.modifierEngine.setModifierEnabled(3, true);
                    }
                    else if (result == 6) {
                        atmoSelector->setButtonText("Ocean");
                        processor.parameters.getParameter("atmoType")->setValueNotifyingHost(5.0f);
                        processor.modifierEngine.setModifierEnabled(3, true);
                    }
                    else if (result == 7) {
                        atmoSelector->setButtonText("Forest");
                        processor.parameters.getParameter("atmoType")->setValueNotifyingHost(6.0f);
                        processor.modifierEngine.setModifierEnabled(3, true);
                    }
                    else if (result == 8) {
                        atmoSelector->setButtonText("Birds");
                        processor.parameters.getParameter("atmoType")->setValueNotifyingHost(7.0f);
                        processor.modifierEngine.setModifierEnabled(3, true);
                    }
                });
        };
        addAndMakeVisible(*atmoSelector);

        atmoLevelSlider = std::make_unique<juce::Slider>();
        atmoLevelSlider->setSliderStyle(juce::Slider::LinearVertical);
        atmoLevelSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        atmoLevelSlider->setRange(0.0, 1.0, 0.01);
        atmoLevelSlider->setValue(0.25);  // Start at -12dB equivalent
        atmoLevelSlider->setTooltip("Atmosphere volume: -inf to 0dB");
        addAndMakeVisible(*atmoLevelSlider);
    }
}

void ModifierSlot::setBinauralState(bool isOn) {
    if (binauralToggle)
        binauralToggle->setToggleState(isOn, juce::dontSendNotification);
}

void ModifierSlot::paint(juce::Graphics& g) {
    auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colours::darkgrey.withAlpha(0.4f));
    g.drawRoundedRectangle(area, 4.0f, 1.0f);
}

void ModifierSlot::showValuePopupFromSlider(juce::Slider& slider) {
    if (!valuePopup) return;
    auto localMouse = getMouseXYRelative();

    juce::String displayText;
    if (&slider == offsetSlider.get()) {
        float hz = slider.getValue();
        juce::String sign = hz >= 0.0f ? "+" : "";
        displayText = sign + juce::String(hz, 1) + " Hz";
    }

    else if (&slider == widthSlider.get()) {
        float width = slider.getValue() * 2.0f - 1.0f; // scale to -1 to +1
        displayText = juce::String(width, 2);
    }
    else {
        displayText = juce::String(slider.getValue(), 2);
    }

    valuePopup->setText(displayText, juce::dontSendNotification);
    valuePopup->setBounds(localMouse.x + 10, localMouse.y - 20, 60, 20);
    valuePopup->setVisible(true);
    valuePopup->toFront(false);
}

void ModifierSlot::hideValuePopup() {
    if (valuePopup)
        valuePopup->setVisible(false);
}

void ModifierSlot::mouseEnter(const juce::MouseEvent& e) {
    if (slotIndex != 0) return;
    if (e.eventComponent == offsetSlider.get())
        showValuePopupFromSlider(*offsetSlider);
    else if (e.eventComponent == widthSlider.get())
        showValuePopupFromSlider(*widthSlider);
}

void ModifierSlot::mouseExit(const juce::MouseEvent&) {
    if (slotIndex != 0) return;
    hideValuePopup();
}

void ModifierSlot::mouseDrag(const juce::MouseEvent& e) {
    if (slotIndex != 0) return;
    if (e.eventComponent == offsetSlider.get())
        showValuePopupFromSlider(*offsetSlider);
    else if (e.eventComponent == widthSlider.get())
        showValuePopupFromSlider(*widthSlider);
}

void ModifierSlot::resized() {
    auto area = getLocalBounds().reduced(6);

    if (slotIndex == 0 && offsetSlider && widthSlider && binauralToggle) {
        binauralToggle->setBounds(area.removeFromTop(20));

        int spacing = 6;
        int labelHeight = 18;
        int totalHeight = area.getHeight();
        int sliderHeight = totalHeight - labelHeight - 6;
        int totalWidth = area.getWidth();
        int colWidth = (totalWidth - spacing) / 2;

        juce::Rectangle<int> sliderArea = area.removeFromTop(sliderHeight);
        juce::Rectangle<int> col1 = sliderArea.removeFromLeft(colWidth);
        sliderArea.removeFromLeft(spacing);
        juce::Rectangle<int> col2 = sliderArea;

        offsetSlider->setBounds(col1);
        widthSlider->setBounds(col2);

        juce::Rectangle<int> labelRow = area.removeFromTop(labelHeight);
        juce::Rectangle<int> col1Label = labelRow.removeFromLeft(colWidth);
        labelRow.removeFromLeft(spacing);
        juce::Rectangle<int> col2Label = labelRow;

        offsetLabel->setBounds(col1Label);
        widthLabel->setBounds(col2Label);
    }

    if (slotIndex == 1 && breathRateSlider && breathDepthSlider && breathToggle && breathRateLabel && breathDepthLabel) {
        breathToggle->setBounds(area.removeFromTop(20));

        int spacing = 6;
        int labelHeight = 16;
        int sliderHeight = area.getHeight() - labelHeight;

        int totalWidth = area.getWidth();
        int colWidth = (totalWidth - spacing) / 2;

        int groupHeight = sliderHeight + labelHeight;
        int verticalOffset = (area.getHeight() - groupHeight) / 2;
        auto content = area.withTrimmedTop(verticalOffset).withHeight(groupHeight);

        auto col1 = content.removeFromLeft(colWidth);
        content.removeFromLeft(spacing);
        auto col2 = content;

        breathRateSlider->setBounds(col1.removeFromTop(sliderHeight));
        breathRateLabel->setBounds(col1);

        breathDepthSlider->setBounds(col2.removeFromTop(sliderHeight));
        breathDepthLabel->setBounds(col2);
    }

    else if (slotIndex == 2 && harmonicLevelSliders.size() == 8) {
        int padding = 5;
        int gridCols = 4;
        int gridRows = 2;
        int spacing = padding;

        auto contentArea = area.reduced(padding); // apply outer padding once

        int cellHeight = 60; // Increased by ~25% from assumed ~40
        int cellWidth = (contentArea.getWidth() - (gridCols - 1) * spacing) / gridCols;
        int gridHeight = gridRows * cellHeight + (gridRows - 1) * spacing;

        // === Vertically center the full grid inside contentArea ===
        int yStart = contentArea.getY() + (contentArea.getHeight() - gridHeight) / 2;

        for (int i = 0; i < 8; ++i) {
            int row = i / gridCols;
            int col = i % gridCols;
            int x = contentArea.getX() + col * (cellWidth + spacing);
            int y = yStart + row * (cellHeight + spacing);
            harmonicLevelSliders[i]->setBounds(x, y, cellWidth, cellHeight);
        }
    }

    else if (slotIndex == 3 && atmoSelector && atmoLevelSlider) {
        // No toggle anymore, just selector and slider
        int selectorHeight = 28;
        atmoSelector->setBounds(area.removeFromTop(selectorHeight));
        area.removeFromTop(6); // Small gap
        atmoLevelSlider->setBounds(area);
    }
}
