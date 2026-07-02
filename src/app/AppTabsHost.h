#pragma once

#include <JuceHeader.h>
#include "MainComponent.h"
#include "PlayerTabComponent.h"
#include "SoundsTabComponent.h"
#include "TracksTabComponent.h"

enum AppTabIndex
{
    PTStart = 0,
    PTScore = 1,
    PTSounds = 2,
    PTEffects = 3,
    PTExit = 4
};

class AppTabbedComponent final : public juce::TabbedComponent
{
public:
    AppTabbedComponent()
        : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
    {
    }

    void currentTabChanged(int newCurrentTabIndex, const juce::String& newCurrentTabName) override
    {
        if (newCurrentTabName == "Exit")
        {
            setCurrentTabIndex(lastNonExitTabIndex, false);

            auto closeApplication = [safeTop = juce::Component::SafePointer<juce::Component>(getTopLevelComponent())]()
            {
                if (safeTop != nullptr)
                {
                    juce::MessageManager::callAsync([safeTop]()
                    {
                        if (safeTop != nullptr)
                            safeTop->userTriedToCloseWindow();
                    });
                }
                else if (auto* app = juce::JUCEApplication::getInstance())
                {
                    juce::MessageManager::callAsync([app]() { app->systemRequestedQuit(); });
                }
            };
            closeApplication();
            return;
        }

        lastNonExitTabIndex = newCurrentTabIndex;
        juce::TabbedComponent::currentTabChanged(newCurrentTabIndex, newCurrentTabName);
    }

private:
    int lastNonExitTabIndex = PTStart;
};

class AppTabsHost final : public juce::Component
{
public:
    AppTabsHost()
        : playerTab(scoreTab),
          soundsTab(scoreTab),
          tracksTab(scoreTab)
    {
        addAndMakeVisible(tabs);
        const auto defaultTabColour = findColour(juce::ResizableWindow::backgroundColourId);
        tabs.addTab("Start", defaultTabColour, &playerTab, false);
        tabs.addTab("Score", defaultTabColour, &scoreTab, false);
        tabs.addTab("Sounds", defaultTabColour, &soundsTab, false);
        tabs.addTab("Effects", defaultTabColour, &tracksTab, false);
        tabs.addTab("Exit", defaultTabColour, &exitPlaceholder, false);

        playerTab.setExitAction([this]() { tabs.setCurrentTabIndex(PTExit, true); });
        tracksTab.setOpenSoundsAction([this](int) { tabs.setCurrentTabIndex(PTSounds, true); });
        soundsTab.setNavigateToEffectsAction([this]() { tabs.setCurrentTabIndex(PTEffects, true); });

        setSize(1280, 720);
    }

    void resized() override
    {
        tabs.setBounds(getLocalBounds());
    }

    void runStartupTasks()
    {
        scoreTab.runStartupResumeIfEnabled();
    }

private:
    AppTabbedComponent tabs;
    juce::Component exitPlaceholder;
    MainComponent scoreTab;
    PlayerTabComponent playerTab;
    SoundsTabComponent soundsTab;
    TracksTabComponent tracksTab;
};
