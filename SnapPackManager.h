// SnapPackManager.h
#pragma once

#include <JuceHeader.h>


class SnapPackManager : public juce::Component {
public:
    SnapPackManager();
    ~SnapPackManager() override {
        onPackSelected = nullptr;
    }

    void paint(juce::Graphics&) override;
    void resized() override;
    

    std::function<void(const juce::String&)> onPackSelected;

    juce::StringArray getAllSnapPackNames() const {
        juce::StringArray names;

        static const juce::StringArray orderedBuiltIns {
            "Deep Sleep",
            "Solfeggio (Default)",
            "Mood Lifter",
            "Anxiety Buster",
            "Focus Mode"
        };

        for (const auto& name : orderedBuiltIns)
            if (builtInPacks.count(name))
                names.add(name);

        std::vector<juce::String> userSorted;
        for (const auto& entry : userPacks)
            userSorted.push_back(entry.first);
        std::sort(userSorted.begin(), userSorted.end());

        for (const auto& name : userSorted)
            names.add(name);

        return names;
    }
    const std::vector<float>& getUserPack(const juce::String& name) const {
        static std::vector<float> empty;
        auto it = userPacks.find(name);
        return (it != userPacks.end()) ? it->second : empty;
    }


private:
    // UI Elements
    juce::Viewport packListViewport;
    std::unique_ptr<juce::Component> packListContainer;
    juce::TextEditor frequencyDisplay;
    juce::TextEditor frequencyInput;
    juce::TextButton addFrequencyButton { "Add" };
    juce::TextButton removeFrequencyButton { "Remove" };
    juce::TextButton createPackButton { "Create New Pack" };
    juce::TextButton deletePackButton { "Delete Pack" };
    juce::TextButton copyPackButton { "Copy"};
    juce::TextButton renamePackButton { "Rename"};
    juce::String renameRequestedName;

    // Close Button
    struct CloseButton : public juce::Component {
        std::function<void()> onClick;
        std::function<void(bool)> onPressedChanged;
        bool isDown = false;

        void mouseDown(const juce::MouseEvent&) override {
            isDown = true;
            if (onPressedChanged) onPressedChanged(true);
            repaint();
        }

        void mouseUp(const juce::MouseEvent&) override {
            isDown = false;
            if (onPressedChanged) onPressedChanged(false);
            if (onClick) onClick();
            repaint();
        }
        
        void paint(juce::Graphics& g) override {
            auto bounds = getLocalBounds().toFloat().reduced(4.0f);
            g.setColour(isDown ? juce::Colours::aqua.withAlpha(0.5f) : juce::Colours::red);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 4.0f, 2.0f);
            g.drawLine(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom(), 2.0f);
            g.drawLine(bounds.getRight(), bounds.getY(), bounds.getX(), bounds.getBottom(), 2.0f);
        }

    };

    CloseButton closeButton;
    bool closeHovered = false;
    bool closeDown = false;

    // Snap Pack Data
    std::map<juce::String, std::vector<float>> userPacks;
    std::map<juce::String, std::vector<float>> builtInPacks {
        {"Deep Sleep",      {0.0f, 40.0f, 50.0f, 62.0f, 108.0f, 120.0f, 136.1f, 174.0f, 285.0f}},
        {"Solfeggio (Default)", {0.0f, 174.0f, 285.0f, 396.0f, 417.0f, 528.0f, 639.0f, 741.0f, 852.0f, 963.0f}},
        {"Mood Lifter",      {0.0f, 136.1f, 528.0f, 963.0f}},
        {"Anxiety Buster",   {0.0f, 111.0f, 136.1f, 396.0f, 417.0f, 444.0f, 528.0f, 639.0f, 741.0f}},
        {"Focus Mode",       {0.0f, 40.0f, 144.72f, 888.0f, 963.0f}},
    };

    juce::String currentSelection;

    void refreshPackList();
    void refreshFrequencyDisplay();
    bool isCustomPack(const juce::String& name) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SnapPackManager)
    
};

