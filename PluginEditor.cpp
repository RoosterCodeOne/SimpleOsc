// === PluginEditor.cpp ===
#include "PluginEditor.h"
#include "CustomSliderLookAndFeel.h"
#include "FreeMode.h"
#include "DebugUtils.h"
#include <random>

CenteredSliderLookAndFeel centeredLook;

PluginEditor::PluginEditor (SimpleOscAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)  // FIXED: Added juce:: prefix
{
    addMouseListener(this, true);

    randomEngine.seed(std::random_device{}());
    initializeBackgroundParticles();
    startTimer(16);
    
    freqSlider.setSliderStyle(juce::Slider::LinearVertical);
    freqSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 60, 20);
    freqSlider.setSnapMode(false); // starts disabled
    freqSlider.onValueChange = [this]() {
        // 🛡️ Do nothing if snap mode is off
        if (!snapModeEnabled)
            return;

        if (!::snapFrequencies.empty()) {
            double val = freqSlider.getValue();
            auto closest = *std::min_element(::snapFrequencies.begin(), ::snapFrequencies.end(),
                [val](float a, float b) {
                    return std::abs(a - val) < std::abs(b - val);
                });

            auto* param = dynamic_cast<juce::AudioParameterFloat*>(processor.parameters.getParameter("freeFrequency"));
            if (param) {
                float norm = param->convertTo0to1((float)closest);
                param->setValueNotifyingHost(norm);
            }
        }
    };

    freqSlider.setLookAndFeel(&centeredLook);
    addAndMakeVisible (freqSlider);
    
    snapToggle.setButtonText("Snap");
    snapToggle.setClickingTogglesState(true);
    addAndMakeVisible(snapToggle);

    onOffButton.setButtonText ("On");
    onOffButton.setAlpha(0.0f); // Transparent button
    onOffButton.onClick = [this]() { repaint(); }; // Force repaint on toggle
    addAndMakeVisible (onOffButton);
    
    for (int i = 0; i < 4; ++i) {
        modifierSlots[i] = std::make_unique<ModifierSlot>(i, processor);
        addAndMakeVisible(*modifierSlots[i]);
    }

    auto* slot0 = modifierSlots[0].get();
    binauralOffsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "binauralOffset", *slot0->offsetSlider);

    binauralWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "binauralWidth", *slot0->widthSlider);

    slot0->binauralToggle->onClick = [this]() {
        bool isOn = modifierSlots[0]->binauralToggle->getToggleState();
        processor.modifierEngine.setModifierEnabled(0, isOn);
    };
    auto* slot1 = modifierSlots[1].get();
    breathRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "breathRate", *slot1->breathRateSlider);
    breathDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "breathDepth", *slot1->breathDepthSlider);
    slot1->breathToggle->onClick = [this, slot1]() {
        bool isOn = slot1->breathToggle->getToggleState();
        processor.modifierEngine.setModifierEnabled(1, isOn);
    };


    
    bool initialBinauralState = processor.modifierEngine.isModifierEnabled(0);
    modifierSlots[0]->setBinauralState(initialBinauralState);
    
    snapToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, "snapOn", snapToggle);
    freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "freeFrequency", freqSlider);
    if (std::find(::snapFrequencies.begin(), ::snapFrequencies.end(), 0.0f) == ::snapFrequencies.end())
        ::snapFrequencies.insert(::snapFrequencies.begin(), 0.0f);
    freqSlider.setSnapFrequencies(snapFrequencies);
    onOffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, "isOn", onOffButton);
    
    // === Force-sync snap mode at startup ===
  bool snapRaw = processor.parameters.getRawParameterValue("snapOn")->load() > 0.5f;
  parameterChanged("snapOn", snapRaw ? 1.0f : 0.0f);
    
    settingsWindow = std::make_unique<SettingsWindow>();
    settingsWindow->onRangeSelected = [this](double min, double max, double /*unused*/) {
        auto currentVal = freqSlider.getValue();
        setFrequencyRange(min, max);

        auto* freqParam = processor.parameters.getParameter("freeFrequency");
        if (freqParam)
            freqParam->setValueNotifyingHost(freqParam->convertTo0to1(currentVal));
    };
    settingsWindow->onSnapPresetSelected = [this](const juce::String& label) {
        applySnapPreset(label);
    };

    addAndMakeVisible(*settingsWindow);
    settingsWindow->setVisible(false); // IMPORTANT: hide by default
    
    
    // === SnapPack Selector ===
    snapPackSelector.setButtonText(currentSnapLabel);
    snapPackSelector.onClick = [this]() {
        snapPackMenu.clear();
        juce::StringArray snapPresets{
            "Deep Sleep",
            "Solfeggio (Default)",
            "Mood Lifter",
            "Anxiety Buster",
            "Focus Mode"
        };
        if (snapPackManager)
            snapPresets = snapPackManager->getAllSnapPackNames();

        for (int i = 0; i < snapPresets.size(); ++i)
            snapPackMenu.addItem(i + 1, snapPresets[i]);

        snapPackMenu.addSeparator();
        snapPackMenu.addItem(snapPresets.size() + 1, "Add Custom List...");

        snapPackMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&snapPackSelector),
            [this, snapPresets](int result) {
                if (result == snapPresets.size() + 1) {
                    if (snapPackManager) {
                        snapPackManager->setSize(320, 480);
                        snapPackManager->setTopLeftPosition(getWidth() / 2 - 160, getHeight() / 2 - 240);
                        snapPackManager->setVisible(true);
                        snapPackManager->toFront(true);
                    }
                } else if (result > 0 && result <= snapPresets.size()) {
                    currentSnapLabel = snapPresets[result - 1];
                    snapPackSelector.setButtonText(currentSnapLabel);
                    applySnapPreset(currentSnapLabel);
                }
            });
    };
    addAndMakeVisible(snapPackSelector);

    // === Range Selector ===
    rangeSelector.setButtonText(currentRangeLabel);
    rangeSelector.onClick = [this]() {
        rangeMenu.clear();
        juce::StringArray labels = {
            "0-1111 Hz (Small)",
            "0-2222 Hz (Default)",
            "0-9999 Hz (Large)",
            "0-20000 Hz (Full Range)"
        };

        for (int i = 0; i < labels.size(); ++i)
            rangeMenu.addItem(i + 1, labels[i]);

        rangeMenu.addSeparator();
        rangeMenu.addItem(5, "Add Custom Range...");

        rangeMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&rangeSelector),
            [this, labels](int result) {
                if (result == 5) {
                    // TODO: add popup for custom range
                } else if (result > 0 && result <= labels.size()) {
                    currentRangeLabel = labels[result - 1];
                    rangeSelector.setButtonText(currentRangeLabel);

                    if (result == 1) setFrequencyRange(0.0, 1111.0);
                    else if (result == 2) setFrequencyRange(0.0, 2222.0);
                    else if (result == 3) setFrequencyRange(0.0, 9999.0);
                    else if (result == 4) setFrequencyRange(0.0, 20000.0);
                }
            });
    };
    addAndMakeVisible(rangeSelector);
    
    setSize (600, 600);
    setResizeLimits(400, 400, 1000, 1000);
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio(1.0);
    
    processor.parameters.addParameterListener("snapOn", this);
    processor.parameters.addParameterListener("binauralOffset", this);
    processor.parameters.addParameterListener("binauralWidth", this);

}

PluginEditor::~PluginEditor() {
    stopTimer();
    settingsWindow = nullptr;
    processor.parameters.removeParameterListener("snapOn", this);
    processor.parameters.removeParameterListener("binauralOffset", this);
    processor.parameters.removeParameterListener("binauralWidth", this);
    if (settingsWindow)
    {
        settingsWindow->onRangeSelected = nullptr;
        settingsWindow->onSnapPresetSelected = nullptr;
    }
}

void PluginEditor::applySnapPreset(const juce::String& name) {
    std::vector<float> newList;

    if (snapPresets.count(name) > 0) {
        newList = snapPresets[name];
    } else if (snapPackManager) {
        const auto& userList = snapPackManager->getUserPack(name);
        newList = userList; // returns empty vector if not found
    }

    if (!newList.empty()) {
        ::snapFrequencies = newList;
        freqSlider.setSnapFrequencies(::snapFrequencies);
        freqSlider.repaint();
    }
}

void PluginEditor::timerCallback()
{
    backgroundTime += 0.016f;
    
    // Very slow gradient rotation
    gradientRotation += 0.2f; // Degrees per frame
    if (gradientRotation > 360.0f)
        gradientRotation -= 360.0f;
    
    updateBackgroundParticles();
    repaint(); // Repaint the whole component
}

void PluginEditor::initializeBackgroundParticles()
{
    particles.clear();
    particles.reserve(25); // Keep particle count low for subtlety
    
    for (int i = 0; i < 25; ++i)
    {
        createNewParticle();
    }
}

void PluginEditor::createNewParticle()
{
    Particle p;
    
    // Random position
    std::uniform_real_distribution<float> positionDist(0.0f, 1.0f);
    p.x = positionDist(randomEngine);
    p.y = positionDist(randomEngine);
    
    // Very slow, gentle movement
    std::uniform_real_distribution<float> velocityDist(-0.0003f, 0.0003f);
    p.vx = velocityDist(randomEngine);
    p.vy = velocityDist(randomEngine);
    
    // Varied sizes, but generally small
    std::uniform_real_distribution<float> sizeDist(2.0f, 8.0f);
    p.size = sizeDist(randomEngine);
    
    // Long life for stability
    std::uniform_real_distribution<float> lifeDist(15.0f, 25.0f);
    p.maxLife = lifeDist(randomEngine);
    p.life = p.maxLife;
    
    // Random color from palette
    std::uniform_int_distribution<int> colorDist(0, meditativeColors.size() - 1);
    p.color = meditativeColors[colorDist(randomEngine)];
    
    // Start with random alpha
    std::uniform_real_distribution<float> alphaDist(0.1f, 0.4f);
    p.alpha = alphaDist(randomEngine);
    
    particles.push_back(p);
}

void PluginEditor::updateBackgroundParticles()
{
    for (auto& particle : particles)
    {
        // Update position
        particle.x += particle.vx;
        particle.y += particle.vy;
        
        // Wrap around edges smoothly
        if (particle.x < -0.1f) particle.x = 1.1f;
        if (particle.x > 1.1f) particle.x = -0.1f;
        if (particle.y < -0.1f) particle.y = 1.1f;
        if (particle.y > 1.1f) particle.y = -0.1f;
        
        // Update life
        particle.life -= 0.016f; // Decrease based on ~60fps
        
        // Fade out as life decreases
        float lifeRatio = particle.life / particle.maxLife;
        particle.alpha = lifeRatio * 0.4f; // Max alpha of 0.4 for subtlety
        
        // Recreate particle if it dies
        if (particle.life <= 0)
        {
            // Reset this particle
            std::uniform_real_distribution<float> positionDist(0.0f, 1.0f);
            particle.x = positionDist(randomEngine);
            particle.y = positionDist(randomEngine);
            
            std::uniform_real_distribution<float> velocityDist(-0.0003f, 0.0003f);
            particle.vx = velocityDist(randomEngine);
            particle.vy = velocityDist(randomEngine);
            
            std::uniform_real_distribution<float> lifeDist(15.0f, 25.0f);
            particle.maxLife = lifeDist(randomEngine);
            particle.life = particle.maxLife;
            
            std::uniform_int_distribution<int> colorDist(0, meditativeColors.size() - 1);
            particle.color = meditativeColors[colorDist(randomEngine)];
        }
    }
}

void PluginEditor::paintMeditativeBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto centerX = bounds.getWidth() * 0.5f;
    auto centerY = bounds.getHeight() * 0.5f;
    
    // Create centered square area for 1:1 aspect ratio
    juce::Rectangle<float> square(centerX - size * 0.5f, centerY - size * 0.5f, size, size);
    
    // 1. BASE: Background gradient that changes color (this should be most visible)
    float colorShift = std::sin(backgroundTime * 0.3f) * 0.2f + 1.0f; // Made faster and more intense
    juce::Colour color1 = juce::Colour(0xff1a1a2e).withMultipliedBrightness(colorShift);
    juce::Colour color2 = juce::Colour(0xff16213e).withMultipliedBrightness(colorShift * 0.8f);
    
    juce::ColourGradient backgroundGradient = juce::ColourGradient::vertical(color1, color2, square);
    g.setGradientFill(backgroundGradient);
    g.fillRect(square);
    
    // 2. OVERLAY: Subtle rotating gradient (REDUCED alpha so base shows through better)
    juce::ColourGradient overlay = juce::ColourGradient(
        juce::Colour(0xff2d1b69).withAlpha(0.15f), // Was 0.3f - now much lighter
        centerX - std::cos(juce::degreesToRadians(gradientRotation)) * size * 0.7f,
        centerY - std::sin(juce::degreesToRadians(gradientRotation)) * size * 0.7f,
        juce::Colour(0xff44318d).withAlpha(0.05f), // Was 0.1f - now much lighter
        centerX + std::cos(juce::degreesToRadians(gradientRotation)) * size * 0.7f,
        centerY + std::sin(juce::degreesToRadians(gradientRotation)) * size * 0.7f,
        false
    );
    
    g.setGradientFill(overlay);
    g.fillRect(square);
    
    // Draw particles (unchanged)
    for (const auto& particle : particles)
    {
        if (particle.alpha > 0.01f)
        {
            float x = square.getX() + particle.x * square.getWidth();
            float y = square.getY() + particle.y * square.getHeight();
            
            juce::Colour particleColor = particle.color.withAlpha(particle.alpha);
            
            for (int i = 3; i >= 1; --i)
            {
                float glowSize = particle.size * i * 0.8f;
                float glowAlpha = particle.alpha * (0.3f / i);
                g.setColour(particleColor.withAlpha(glowAlpha));
                g.fillEllipse(x - glowSize * 0.5f, y - glowSize * 0.5f, glowSize, glowSize);
            }
            
            g.setColour(particleColor);
            g.fillEllipse(x - particle.size * 0.5f, y - particle.size * 0.5f,
                         particle.size, particle.size);
        }
    }
    
    // 3. BREATHING: Very subtle breathing effect (REDUCED alpha)
    float breathe = (std::sin(backgroundTime * 0.1f) + 1.0f) * 0.5f;
    juce::Colour breatheColor = juce::Colour(0xff3c6e71).withAlpha(breathe * 0.03f); // Was 0.05f - now lighter
    
    juce::ColourGradient breatheGradient(
        breatheColor,
        centerX, centerY,
        juce::Colour(0xff3c6e71).withAlpha(0.0f),
        centerX + size * 0.4f, centerY + size * 0.4f,
        true
    );
    
    g.setGradientFill(breatheGradient);
    g.fillRect(square);
}

void PluginEditor::paint (juce::Graphics& g)
{
    paintMeditativeBackground(g);
    
    juce::DropShadow shadow(juce::Colours::black.withAlpha(0.5f), 8, {2, 2});
    shadow.drawForRectangle(g, carvedArea);
    juce::Colour topLeft = juce::Colour::fromString("ff6a6a6a");   // slightly lighter gray
    juce::Colour bottomRight = juce::Colour::fromString("ff3f3f3f"); // slightly darker gray

    juce::ColourGradient diagGrad(
        topLeft, carvedArea.getX(), carvedArea.getY(),
        bottomRight, carvedArea.getRight(), carvedArea.getBottom(),
        false
    );

    g.setGradientFill(diagGrad);
    g.fillRoundedRectangle(carvedArea.toFloat(), 8.0f);

    auto blockColor = juce::Colours::black.withAlpha(0.3f);
    auto outlineColor = juce::Colour::fromString("ff757575");

    // Top Row Blocks
    for (auto& block : { topRowBlock1, topRowBlock2, topRowBlock3, topRowBlock4 }) {
        g.setColour(blockColor);
        g.fillRoundedRectangle(block, 6.0f);
        g.setColour(outlineColor);
        g.drawRoundedRectangle(block, 6.0f, 1.0f);
    }
    // Middle Row Blocks
    for (auto& block : { midRowBlock1, midRowBlock2 }) {
        g.setColour(blockColor);
        g.fillRoundedRectangle(block, 6.0f);
        g.setColour(outlineColor);
        g.drawRoundedRectangle(block, 6.0f, 1.0f);
    }
    // Slider Block Background
    g.setColour(blockColor);
    g.fillRoundedRectangle(sliderBlock, 6.0f);
    g.setColour(outlineColor);
    g.drawRoundedRectangle(sliderBlock, 6.0f, 1.0f);
    
    //mod Blocks
    for (auto& block : { modBlock1, modBlock2, modBlock3, modBlock4 }) {
        g.setColour(blockColor);
        g.fillRoundedRectangle(block, 6.0f);
        g.setColour(outlineColor);
        g.drawRoundedRectangle(block, 6.0f, 1.0f);
    }
    
    bool isOn = processor.parameters.getRawParameterValue("isOn")->load() > 0.5f;

    // === Power Symbol in TopRowBlock1 ===
    if (isOn) {
        g.setColour(juce::Colours::limegreen.withAlpha(0.4f));
        g.fillRoundedRectangle(topRowBlock1.reduced(2.0f), 6.0f);
    }
    {
        auto bounds = topRowBlock1.reduced(12.0f);
        float baseR = std::min(bounds.getWidth(), bounds.getHeight()) * 0.4f;
        float r = baseR * 0.75f;  // 75% size
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();

        // Main arc and notch
        juce::Path arc, notch;
        arc.addCentredArc(cx, cy, r, r, 0.0f,
                          juce::MathConstants<float>::pi * 0.25f,
                          juce::MathConstants<float>::pi * 1.75f, true);
        notch.startNewSubPath(cx, cy - r);
        notch.lineTo(cx, cy - r * 0.5f);

        juce::Colour powerColor = isOn ? juce::Colours::white : juce::Colours::red;

        // Optional subtle glow when OFF
        if (!isOn)
        {
            juce::Colour glowColor = juce::Colours::red.withAlpha(0.07f);
            g.setColour(glowColor);
            g.strokePath(arc, juce::PathStrokeType(6.0f));   // fat glow layer
            g.strokePath(notch, juce::PathStrokeType(6.0f));
        }

        // Actual symbol lines
        g.setColour(powerColor);
        g.strokePath(arc, juce::PathStrokeType(2.5f));
        g.strokePath(notch, juce::PathStrokeType(2.5f));
    }

    if (snapModeEnabled)
    {
        g.setColour(juce::Colours::aqua.withAlpha(0.3f));
        g.fillRoundedRectangle(topRowBlock2.reduced(2.0f), 6.0f);
    }
    
    // === Draw Donut Cog Icon with stroked ring ===
    {
        auto area = topRowBlock3.reduced(12.0f);
        juce::Path cog;

        float cx = area.getCentreX();
        float cy = area.getCentreY();
        float ringRadius = 9.0f;
        float ringThickness = 3.0f;

        // Draw the ring (donut shape using stroke)
        juce::Path ring;
        ring.addCentredArc(cx, cy, ringRadius, ringRadius, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        g.setColour(juce::Colours::aqua);
        g.strokePath(ring, juce::PathStrokeType(ringThickness));

        // Draw gear teeth
        for (int i = 0; i < 8; ++i) {
            float angle = juce::MathConstants<float>::twoPi * i / 8.0f;
            float bx = std::cos(angle);
            float by = std::sin(angle);

            juce::Path tooth;
            juce::AffineTransform t = juce::AffineTransform::rotation(angle, 0, 0)
                                      .translated(cx + bx * (ringRadius + 2),
                                                  cy + by * (ringRadius + 2));
            tooth.addRectangle(-1.0f, -3.0f, 2.0f, 6.0f);  // small vertical rectangle
            cog.addPath(tooth, t);
        }

        g.fillPath(cog);
    }

}



void PluginEditor::mouseUp(const juce::MouseEvent& e)
{
    //Top Row Blocks
    auto pos = e.position;
    
    if (topRowBlock3.contains(pos)) {
        if (settingsWindow)
        {
            auto center = carvedArea.getCentre();
            settingsWindow->setTopLeftPosition(center.x - settingsWindow->getWidth() / 2,
                                               center.y - settingsWindow->getHeight() / 2);
            settingsWindow->setBounds(getLocalBounds());  // Fill the full plugin bounds
            settingsWindow->setVisible(true);
            settingsWindow->toFront(true);
        }
    }


    //Mid Row Blocks
    if (midRowBlock1.contains(pos)) {
        DBG("midRowBlock1 clicked");
    }
    else if (midRowBlock2.contains(pos)) {
        DBG("midRowBlock2 clicked");
    }

    
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();
    int w = bounds.getWidth();
    int h = bounds.getHeight();
    
    int size = std::min(w, h) * 3 / 4;
    carvedArea = juce::Rectangle<int>(
        bounds.getCentreX() - size / 2,
        bounds.getCentreY() - size / 2,
        size,
        size);
    uiArea = carvedArea;
    
    // === Top Row Blocks inside carvedArea ===
    int padding = 6;
    int topHeight = static_cast<int>(carvedArea.getHeight() * 0.12f);

    auto topStripArea = carvedArea.withTrimmedTop(padding)
                                   .withTrimmedBottom(carvedArea.getHeight() - topHeight - padding)
                                   .reduced(padding, 0);
    int blockSpacing = 6;
    int totalSpacing = blockSpacing * 3;
    int blockWidth = (topStripArea.getWidth() - totalSpacing) / 4;
    int blockHeight = (blockWidth * 2) / 3;

    juce::Rectangle<int> topRow = topStripArea.withHeight(blockHeight).withY(topStripArea.getY());

    topRowBlock1 = topRow.removeFromLeft(blockWidth).toFloat();
    topRow.removeFromLeft(blockSpacing);
    topRowBlock2 = topRow.removeFromLeft(blockWidth).toFloat();
    topRow.removeFromLeft(blockSpacing);
    topRowBlock3 = topRow.removeFromLeft(blockWidth).toFloat();
    topRow.removeFromLeft(blockSpacing);
    topRowBlock4 = topRow.removeFromLeft(blockWidth).toFloat();
    
    onOffButton.setBounds(topRowBlock1.toNearestInt());
    
    // === Middle Row Blocks ===
    int midBlockHeight = blockHeight / 2;
    int fullMidWidth = blockWidth * 2 + blockSpacing;
    int shrunkMidWidth = static_cast<int>(fullMidWidth * 0.75f);

    juce::Rectangle<int> midRow = carvedArea.withTrimmedTop(padding + blockHeight + padding)
                                            .withTrimmedBottom(carvedArea.getHeight() - padding - midBlockHeight)
                                            .reduced(padding, 0)
                                            .withHeight(midBlockHeight);

    midRowBlock1 = juce::Rectangle<float>(
        midRow.getX(),
        midRow.getY(),
        shrunkMidWidth,
        midBlockHeight
    );

    midRowBlock2 = juce::Rectangle<float>(
        midRow.getRight() - shrunkMidWidth,
        midRow.getY(),
        shrunkMidWidth,
        midBlockHeight
    );
    
    // === Slider Block (full width, same height as top row) ===
    int sliderBlockX = midRowBlock1.getRight() + 12;
    int sliderBlockY = midRowBlock1.getBottom() + 12;
    int sliderBlockW = midRowBlock2.getX() - midRowBlock1.getRight() - 24;
    int sliderBlockH = carvedArea.getBottom() - sliderBlockY - (blockHeight / 2) + 12;

    sliderBlock = juce::Rectangle<float>(
        (float)sliderBlockX,
        (float)sliderBlockY,
        (float)sliderBlockW,
        (float)sliderBlockH
    );
    
    // Mod Blocks
    int modBlockW = shrunkMidWidth;
    int modBlockH = static_cast<int>(blockHeight * 2);

    // === ModBlocks 1 and 2: aligned with sliderBlock top ===
    int modTopY = sliderBlock.getY();
    modBlock1 = juce::Rectangle<float>(
        midRowBlock1.getX(),
        modTopY,
        modBlockW,
        modBlockH
    );
    modBlock2 = juce::Rectangle<float>(
        midRowBlock2.getRight() - modBlockW,
        modTopY,
        modBlockW,
        modBlockH
    );

    // === ModBlocks 3 and 4: aligned with sliderBlock bottom ===
    int modBottomY = sliderBlock.getBottom() - modBlockH;
    modBlock3 = juce::Rectangle<float>(
        midRowBlock1.getX(),
        modBottomY,
        modBlockW,
        modBlockH
    );
    modBlock4 = juce::Rectangle<float>(
        midRowBlock2.getRight() - modBlockW,
        modBottomY,
        modBlockW,
        modBlockH
    );

    
    auto layoutArea = carvedArea;
    snapToggle.setBounds(topRowBlock2.reduced(8).toNearestInt());
    
    settingsButton.setBounds(topRowBlock3.toNearestInt());
    
    if (settingsWindow && settingsWindow->isVisible()) {
    settingsWindow->setBounds(getLocalBounds()); // Resize to fill full window
    settingsWindow->resized(); // Force update of internal layout like overlay + contentArea
}

    // === Volume Slider ===
    volumeSlider.setSliderStyle(juce::Slider::Rotary);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setLookAndFeel(&centeredLook);
    volumeSlider.setBounds(topRowBlock4.reduced(10).toNearestInt());
    addAndMakeVisible(volumeSlider);

    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "volume", volumeSlider);
    
    layoutArea.removeFromTop(80);
    
    // Move freqSlider slightly up and center its textbox inside the rotary knob
    auto sliderBounds = sliderBlock.toNearestInt();
    freqSlider.setBounds(sliderBounds);

    modifierSlots[0]->setBounds(modBlock1.toNearestInt());
    modifierSlots[1]->setBounds(modBlock2.toNearestInt());
    modifierSlots[2]->setBounds(modBlock3.toNearestInt());
    modifierSlots[3]->setBounds(modBlock4.toNearestInt());
    
    // Volume slider gets its bounds in topRowBlock4
    volumeSlider.setBounds(topRowBlock4.reduced(10).toNearestInt());
    
    snapPackSelector.setBounds(midRowBlock1.reduced(4).toNearestInt());
    rangeSelector.setBounds(midRowBlock2.reduced(4).toNearestInt());

}

void PluginEditor::parameterChanged(const juce::String& paramID, float newValue)
{
    // ✅ This runs for offset/width changes
    if (paramID == "binauralOffset" || paramID == "binauralWidth")
    {
        processor.parameterChanged(paramID, newValue); // 💥 forward to audio processor
    }

    if (paramID == "snapOn")
    {
        snapModeEnabled = newValue > 0.5f;
        freqSlider.setSnapMode(snapModeEnabled);
        freqSlider.setSnapFrequencies(snapFrequencies);

        if (snapModeEnabled)
        {
            float snapMin = *std::min_element(snapFrequencies.begin(), snapFrequencies.end());
            float snapMax = *std::max_element(snapFrequencies.begin(), snapFrequencies.end());
            freqSlider.setRange(snapMin, snapMax, 0.01);

            auto* freqParam = dynamic_cast<juce::AudioParameterFloat*>(processor.parameters.getParameter("freeFrequency"));
            if (freqParam)
                freqParam->range = juce::NormalisableRange<float>(snapMin, snapMax);
            
            double currentVal = freqSlider.getValue();
            auto closest = *std::min_element(snapFrequencies.begin(), snapFrequencies.end(),
                [currentVal](float a, float b) {
                    return std::abs(a - currentVal) < std::abs(b - currentVal);
                });

            auto* param = dynamic_cast<juce::AudioParameterFloat*>(processor.parameters.getParameter("freeFrequency"));
            if (param) {
                float norm = param->convertTo0to1((float)closest);
                param->setValueNotifyingHost(norm);
            }
        }
        else
        {
            freqSlider.setRange(freqSlider.freqMin, freqSlider.freqMax);
            auto* freqParam = dynamic_cast<juce::AudioParameterFloat*>(processor.parameters.getParameter("freeFrequency"));
            if (freqParam)
                freqParam->range = juce::NormalisableRange<float>(freqSlider.freqMin, freqSlider.freqMax);
        }

        freqSlider.repaint();
    }
}


void PluginEditor::setFrequencyRange(double min, double max) {
    freqSlider.freqMin = min;
    freqSlider.freqMax = max;

    if (snapModeEnabled && !snapFrequencies.empty()) {
        double snapMin = *std::min_element(snapFrequencies.begin(), snapFrequencies.end());
        double snapMax = *std::max_element(snapFrequencies.begin(), snapFrequencies.end());
        freqSlider.setRange(snapMin, snapMax, 1.0);
    }
    else
    {
        freqSlider.setRange(freqSlider.freqMin, freqSlider.freqMax); // restore free range
    }

    // Re-apply current value proportionally
    auto currentValue = freqSlider.getValue();
    auto proportion = freqSlider.valueToProportionOfLength(currentValue);
    auto newValue = freqSlider.proportionOfLengthToValue(proportion);
    freqSlider.setValue(newValue, juce::sendNotificationSync);
    
    processor.setRangeMinMax(min, max);

    auto* freqParam = dynamic_cast<juce::AudioParameterFloat*>(processor.parameters.getParameter("freeFrequency"));
    if (freqParam)
        freqParam->range = juce::NormalisableRange<float>(min, max);

    repaint();
}

