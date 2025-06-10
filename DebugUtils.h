#pragma once
#include <JuceHeader.h>

inline void logToFile(const juce::String& message)
{
    auto logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                       .getChildFile("SimpleOsc_Log.txt");

    logFile.appendText(message + "\n", false, false, nullptr);
}
