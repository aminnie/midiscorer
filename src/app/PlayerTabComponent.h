#pragma once

#include <JuceHeader.h>
#include <vector>
#include "MainComponent.h"

class PlayerTabComponent final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit PlayerTabComponent(MainComponent& scorePageRef)
        : scorePage(scorePageRef)
    {
        addAndMakeVisible(fileLabel);
        fileLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(outputLabel);
        outputLabel.setText("MIDI Output", juce::dontSendNotification);
        outputLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(refreshOutputsButton);
        refreshOutputsButton.setButtonText("Refresh");
        refreshOutputsButton.onClick = [this] { refreshOutputDeviceList(); };

        addAndMakeVisible(outputSelector);
        outputSelector.onChange = [this] { handleOutputSelectionChanged(); };

        refreshOutputDeviceList();
        refreshLiveStatus();
        startTimerHz(5);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        auto row1 = area.removeFromTop(28);
        outputLabel.setBounds(row1.removeFromLeft(90));
        outputSelector.setBounds(row1.removeFromLeft(360).reduced(4, 0));
        refreshOutputsButton.setBounds(row1.removeFromLeft(96).reduced(4, 0));

        area.removeFromTop(8);
        fileLabel.setBounds(area.removeFromTop(24));

        area.removeFromTop(8);
        statusLabel.setBounds(area.removeFromTop(24));
    }

private:
    void timerCallback() override
    {
        refreshLiveStatus();
    }

    void refreshOutputDeviceList()
    {
        availableOutputs = scorePage.getAvailableMidiOutputs();
        outputSelector.clear(juce::dontSendNotification);
        outputSelector.addItem("(None)", 1);

        int itemId = 2;
        int selectedId = 1;
        const auto currentSelection = scorePage.getSelectedMidiOutputIdentifier();
        for (const auto& output : availableOutputs)
        {
            outputSelector.addItem(output.name, itemId);
            if (output.identifier == currentSelection)
                selectedId = itemId;
            ++itemId;
        }

        outputSelector.setSelectedId(selectedId, juce::dontSendNotification);
    }

    void handleOutputSelectionChanged()
    {
        const int selectedId = outputSelector.getSelectedId();
        if (selectedId <= 1)
        {
            scorePage.clearMidiOutputDevice();
            refreshLiveStatus();
            return;
        }

        const int index = selectedId - 2;
        if (index < 0 || index >= static_cast<int>(availableOutputs.size()))
            return;

        juce::String error;
        const auto selectedIdentifier = availableOutputs[(size_t) index].identifier;
        const bool selected = scorePage.selectMidiOutputDevice(selectedIdentifier, true, error);
        if (!selected)
            statusLabel.setText("MIDI output error: " + error, juce::dontSendNotification);
    }

    void refreshLiveStatus()
    {
        const auto midiName = scorePage.getLoadedMidiFileName();
        fileLabel.setText(midiName.isNotEmpty() ? ("Loaded MIDI: " + midiName) : "Loaded MIDI: none",
                          juce::dontSendNotification);

        const bool isPlaying = scorePage.isPlaybackRunning();
        const int currentBar = scorePage.getCurrentPlaybackBar();
        const int maxBar = scorePage.getMaximumBar();
        auto outputName = scorePage.getSelectedMidiOutputName();
        if (outputName.isEmpty())
            outputName = "(None)";

        statusLabel.setText(juce::String(isPlaying ? "Playing" : "Stopped")
                                + "  | Bar " + juce::String(currentBar) + "/" + juce::String(maxBar)
                                + "  | Output: " + outputName,
                            juce::dontSendNotification);
    }

    MainComponent& scorePage;
    std::vector<MidiOutputDeviceInfo> availableOutputs;

    juce::Label outputLabel;
    juce::ComboBox outputSelector;
    juce::TextButton refreshOutputsButton;
    juce::Label fileLabel;
    juce::Label statusLabel;
};
