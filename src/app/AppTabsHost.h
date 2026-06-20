#pragma once

#include <JuceHeader.h>
#include "MainComponent.h"
#include "PlayerTabComponent.h"

class AppTabsHost final : public juce::Component
{
public:
    AppTabsHost()
        : tabs(juce::TabbedButtonBar::TabsAtTop),
          playerTab(scoreTab)
    {
        addAndMakeVisible(tabs);
        tabs.addTab("Score", juce::Colour(0xff1f2a36), &scoreTab, false);
        tabs.addTab("Player", juce::Colour(0xff20303a), &playerTab, false);
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
};
