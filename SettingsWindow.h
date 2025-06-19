#include "SnapPackManager.h"

class PluginEditor;  // forward declaration

struct SettingsWindow : public juce::Component
{
    ~SettingsWindow() override {
        onSnapPresetSelected = nullptr;
        onRangeSelected = nullptr;
    }

    struct Overlay : public juce::Component {
        void paint(juce::Graphics& g) override {
            g.setColour(juce::Colours::black.withAlpha(0.4f)); // overlay for visibility test
            g.fillAll();
        }
    };
    
    Overlay overlay;
    
    struct ContentArea : public juce::Component {
        void paint(juce::Graphics& g) override {
            juce::DropShadow shadow(juce::Colours::black.withAlpha(0.4f), 10, {6, 6});
            shadow.drawForRectangle(g, getLocalBounds());
            
            juce::Colour topLeft = juce::Colour::fromString("ff6a6a6a");
            juce::Colour bottomRight = juce::Colour::fromString("ff3f3f3f");
            juce::ColourGradient diagGrad(
                                          topLeft, 0, 0,
                                          bottomRight, getWidth(), getHeight(),
                                          false
                                          );
            
            g.setGradientFill(diagGrad);
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.0f);
        }
    };
    
    ContentArea contentArea;
    
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
            auto bounds = getLocalBounds().toFloat().reduced(6.0f);
            g.setColour(isDown ? juce::Colours::aqua.withAlpha(0.5f) : juce::Colours::red);

            float corner = 4.0f;
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), corner, 2.0f);
            g.drawLine(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom(), 2.0f);
            g.drawLine(bounds.getRight(), bounds.getY(), bounds.getX(), bounds.getBottom(), 2.0f);
        }
    };

    
    CloseButton closeButton;
    
    juce::PopupMenu snapPackMenu;
    
    std::function<void(double, double, double)> onRangeSelected;
    std::function<void(const juce::String&)> onSnapPresetSelected;
    
    std::unique_ptr<SnapPackManager> snapPackManager;
    
    
    SettingsWindow() {
        snapPackManager = std::make_unique<SnapPackManager>();

            overlay.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(overlay);
            overlay.toBack();
            
            contentArea.setSize(360, 280);
            addAndMakeVisible(contentArea);
            
            // === Close Button === //
            addAndMakeVisible(closeButton);
            closeButton.onClick = [this]() {
                setVisible(false);
                if (auto* parent = getParentComponent())
                    parent->repaint();
            };
            
            // === Range Selector ===
            
            addAndMakeVisible(*snapPackManager);
            snapPackManager->toFront(true);
            snapPackManager->setVisible(false);
            
            snapPackManager->onPackSelected = [this](const juce::String& name) {
        if (onSnapPresetSelected)
            onSnapPresetSelected(name);
        // Do NOT close the manager here — we want to allow editing
    };
    
    }
    
    void paint(juce::Graphics& g) override {}
    
    void resized() override
    {
        overlay.setBounds(getLocalBounds());
        
        // Center the content area in the overlay
        int w = contentArea.getWidth();
        int h = contentArea.getHeight();
        contentArea.setTopLeftPosition(getLocalBounds().getCentreX() - w / 2,
                                       getLocalBounds().getCentreY() - h / 2);
        

                
        // Position the close button in the top-right of the content window
        closeButton.setBounds(contentArea.getX() + contentArea.getWidth() - 32,
                              contentArea.getY() + 8, 24, 24);
    }
};
