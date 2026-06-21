#pragma once

#include <JuceHeader.h>
#include "MainComponent.h"
#include "PlayerTabComponent.h"
#include "TracksTabComponent.h"

class AppTabsHost final : public juce::Component
{
public:
    AppTabsHost()
        : tabs(juce::TabbedButtonBar::TabsAtTop),
          playerTab(scoreTab),
          tracksTab(scoreTab)
    {
        addAndMakeVisible(tabs);
        const auto defaultTabColour = findColour(juce::ResizableWindow::backgroundColourId);
        tabs.addTab("Score", defaultTabColour, &scoreTab, false);
        tabs.addTab("Player", defaultTabColour, &playerTab, false);
        tabs.addTab("Effects", defaultTabColour, &tracksTab, false);
        setSize(1280, 720);
    }

    void resized() override
    {
        tabs.setBounds(getLocalBounds());
    }

private:
    juce::TabbedComponent tabs;
    MainComponent scoreTab;
    PlayerTabComponent playerTab;
    TracksTabComponent tracksTab;
};
