// SnapPackManager.cpp
#include "SnapPackManager.h"

SnapPackManager::SnapPackManager() {
    setSize(320, 480); // fits inside SettingsWindow contentArea (2:3 ratio)
    
    addAndMakeVisible(packListViewport);
    packListContainer = std::make_unique<juce::Component>();
    packListViewport.setViewedComponent(packListContainer.get(), true);
    
    addAndMakeVisible(frequencyDisplay);
    frequencyDisplay.setMultiLine(true);
    frequencyDisplay.setReadOnly(true);
    
    addAndMakeVisible(frequencyInput);
    addAndMakeVisible(addFrequencyButton);
    addAndMakeVisible(removeFrequencyButton);
    addAndMakeVisible(createPackButton);
    addAndMakeVisible(deletePackButton);
    addAndMakeVisible(copyPackButton);
    addAndMakeVisible(renamePackButton);
    addAndMakeVisible(closeButton);
    
    // Set up button callbacks (initial)
    createPackButton.onClick = [this]() {
        juce::String baseName = "New Snap Pack";
        juce::String newName = baseName;
        int index = 1;
        
        // Check if name exists, and increment if needed
        while (userPacks.count(newName) > 0 || builtInPacks.count(newName) > 0) {
            newName = baseName + " " + juce::String(index);
            ++index;
        }
        
        userPacks[newName] = {};
        refreshPackList();

    // Setup Copy and Rename actions
    copyPackButton.onClick = [this]() {
        if (!isCustomPack(currentSelection)) return;
        auto original = currentSelection;
        juce::String newName = original + " Copy";
        int suffix = 1;
        while (userPacks.count(newName) > 0 || builtInPacks.count(newName) > 0)
            newName = original + " Copy " + juce::String(suffix++);
        userPacks[newName] = userPacks[original];
        refreshPackList();
        refreshFrequencyDisplay();
    };

    renamePackButton.onClick = [this]() {
        if (!isCustomPack(currentSelection)) return;
        renameRequestedName = currentSelection;
        refreshPackList();
    };
        refreshFrequencyDisplay();
    };
    
    addFrequencyButton.onClick = [this]() {
        if (!isCustomPack(currentSelection)) return;
        float val = frequencyInput.getText().getFloatValue();
        userPacks[currentSelection].push_back(val);
        std::sort(userPacks[currentSelection].begin(), userPacks[currentSelection].end(), std::greater<float>());
        refreshFrequencyDisplay();
    };
    
    removeFrequencyButton.onClick = [this]() {
        if (!isCustomPack(currentSelection)) return;
        float val = frequencyInput.getText().getFloatValue();
        auto& vec = userPacks[currentSelection];
        vec.erase(std::remove(vec.begin(), vec.end(), val), vec.end());
        refreshFrequencyDisplay();
    };
    
    deletePackButton.onClick = [this]() {
        if (!isCustomPack(currentSelection)) return;
        userPacks.erase(currentSelection);
        currentSelection = "";
        refreshPackList();
        refreshFrequencyDisplay();
    };
    
    closeButton.onClick = [this]() {
        setVisible(false);
        if (auto* parent = getParentComponent())
            parent->repaint();
    };
    
    closeButton.onPressedChanged = [this](bool pressed) {
        closeDown = pressed;
        repaint();
    };
    
    refreshPackList();
}

void SnapPackManager::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::darkgrey);
}

void SnapPackManager::resized() {
    auto area = getLocalBounds().reduced(10);

    // Left column (Snap Pack list + buttons)
    auto left = area.removeFromLeft(180);
    left.removeFromTop(20); // spacing from top
    auto leftHeight = area.getHeight() - 20; // match frequencyInput base
    packListViewport.setBounds(left.removeFromTop(leftHeight - 90)); // extra room for both buttons
    createPackButton.setBounds(left.removeFromTop(30));
    deletePackButton.setBounds(left.removeFromTop(30));

    // === Copy and Rename buttons (side by side) ===
    auto halfWidth = left.getWidth() / 2 - 2;
    auto row = left.removeFromTop(30);
    copyPackButton.setBounds(row.removeFromLeft(halfWidth));
    renamePackButton.setBounds(row);


    // Right column (Frequencies + controls)
    area.removeFromTop(20); // spacing from top
    frequencyDisplay.setBounds(area.removeFromTop(getHeight() - 160)); // 20 px shorter
    frequencyInput.setBounds(area.removeFromTop(30));
    addFrequencyButton.setBounds(area.removeFromTop(30));
    removeFrequencyButton.setBounds(area.removeFromTop(30));

    // Always top-right corner (inside full component bounds)
    closeButton.setBounds(getLocalBounds().reduced(10).removeFromTop(20).removeFromRight(20));
}

void SnapPackManager::refreshPackList() {
    packListContainer->removeAllChildren();

    // inline rename logic removed

    auto renderOrderedBuiltIns = [this]() {
        static juce::StringArray orderedNames {
            "Deep Sleep",
            "Solfeggio (Default)",
            "Mood Lifter",
            "Anxiety Buster",
            "Focus Mode"
        };

        for (const auto& name : orderedNames) {
            if (builtInPacks.count(name) == 0) continue;
            auto* b = new juce::TextButton(name);

            b->onClick = [this, name]() {
    if (currentSelection != name) {
        currentSelection = name;
        renameRequestedName.clear();
    }
    refreshFrequencyDisplay();
    refreshPackList();
    if (onPackSelected)
        onPackSelected(name);
};

            if (name == currentSelection)
                b->setColour(juce::TextButton::buttonColourId, juce::Colours::aqua.withAlpha(0.3f));
            else
                b->setColour(juce::TextButton::buttonColourId, juce::Colour::fromString("ff263238"));

            packListContainer->addAndMakeVisible(b);
        }
    };

auto renderUserPacks = [this]() {
    std::vector<juce::String> sortedNames;
    for (const auto& entry : userPacks)
        sortedNames.push_back(entry.first);
    std::sort(sortedNames.begin(), sortedNames.end());

    for (const auto& name : sortedNames) {
        if (name == renameRequestedName) {
            auto* editor = new juce::TextEditor();
            editor->setText(name);
            editor->setSelectAllWhenFocused(true);
            editor->setBounds(0, packListContainer->getChildren().size() * 32, packListViewport.getWidth(), 30);

            editor->onReturnKey = [this, editor, name]() {
                auto newName = editor->getText();
                if (!newName.isEmpty() && newName != name &&
                    userPacks.count(newName) == 0 && builtInPacks.count(newName) == 0) {
                    userPacks[newName] = userPacks[name];
                    userPacks.erase(name);
                    currentSelection = newName;
                }
                renameRequestedName.clear();
                refreshPackList();
                refreshFrequencyDisplay();
            };

            editor->onEscapeKey = [this]() {
                renameRequestedName.clear();
                refreshPackList();
            };

            editor->onFocusLost = [this]() {
                renameRequestedName.clear();
                refreshPackList();
            };

            packListContainer->addAndMakeVisible(editor);
            editor->grabKeyboardFocus();
        } else {
            auto* b = new juce::TextButton(name);
            b->onClick = [this, name]() {
                if (currentSelection == name) {
                    renameRequestedName.clear();
                } else {
                    currentSelection = name;
                    renameRequestedName.clear();
                }
                refreshFrequencyDisplay();
                refreshPackList();
                if (onPackSelected)
                    onPackSelected(name);
            };

            if (name == currentSelection)
                b->setColour(juce::TextButton::buttonColourId, juce::Colours::aqua.withAlpha(0.3f));
            else
                b->setColour(juce::TextButton::buttonColourId, juce::Colour::fromString("ff263238"));

            packListContainer->addAndMakeVisible(b);
        }
        }
};

    renderOrderedBuiltIns();
    renderUserPacks();

    int y = 0;
    for (auto* child : packListContainer->getChildren()) {
        child->setBounds(0, y, packListViewport.getWidth(), 30);
        y += 32;
    }
    packListContainer->setSize(packListViewport.getWidth(), y);
};

void SnapPackManager::refreshFrequencyDisplay() {
    juce::String text;
    const auto& mapRef = isCustomPack(currentSelection) ? userPacks : builtInPacks;

    if (mapRef.count(currentSelection) > 0) {
        for (auto val : mapRef.at(currentSelection))
            text += juce::String(val) + "\n";
    }
    frequencyDisplay.setText(text);
}

bool SnapPackManager::isCustomPack(const juce::String& name) const {
    return userPacks.count(name) > 0;
}
