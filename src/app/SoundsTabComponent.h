#pragma once

#include <JuceHeader.h>
#include <optional>
#include "../sounds/InstrumentCatalog.h"
#include "MainComponent.h"

class SoundsTabComponent final : public juce::Component,
                                 private juce::ListBoxModel,
                                 private juce::Timer
{
public:
    explicit SoundsTabComponent(MainComponent& scorePageRef)
        : scorePage(scorePageRef)
    {
        addAndMakeVisible(trackLabel);
        trackLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(moduleLabel);
        moduleLabel.setText("Module", juce::dontSendNotification);
        moduleLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(moduleSelector);
        moduleSelector.onChange = [this]
        {
            const int moduleIndex = moduleSelector.getSelectedItemIndex();
            juce::String error;
            if (!catalog.loadModuleByIndex(moduleIndex, error))
                statusLabel.setText(error, juce::dontSendNotification);
            refreshCategories();
        };

        addAndMakeVisible(categoryLabel);
        categoryLabel.setText("Category", juce::dontSendNotification);
        categoryLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(categorySelector);
        categorySelector.onChange = [this] { refreshVoices(); };

        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(voicesList);
        voicesList.setModel(this);
        voicesList.setRowHeight(24);

        addAndMakeVisible(detailGroup);
        detailGroup.setText("Selected Sound");

        addAndMakeVisible(detailTrackSource);
        detailTrackSource.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(detailName);
        detailName.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(detailBank);
        detailBank.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(detailProgram);
        detailProgram.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(backToEffectsButton);
        backToEffectsButton.setButtonText("Save to Effects");
        applyStartButtonStyle(backToEffectsButton);
        backToEffectsButton.onClick = [this]
        {
            const int trackIndex = scorePage.getTrackSoundEditingTrackIndex();
            if (pendingSelection.has_value())
                scorePage.setTrackSoundProgram(trackIndex, pendingSelection.value());
            if (navigateToEffectsAction)
                navigateToEffectsAction();
        };

        for (const auto& module : catalog.getModules())
            moduleSelector.addItem(module.displayName, moduleSelector.getNumItems() + 1);
        moduleSelector.setSelectedItemIndex(juce::jmax(0, catalog.getSelectedModuleIndex()), juce::dontSendNotification);
        refreshCategories();
        startTimerHz(2);
    }

    void setNavigateToEffectsAction(std::function<void()> action)
    {
        navigateToEffectsAction = std::move(action);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        auto topRow = bounds.removeFromTop(30);
        trackLabel.setBounds(topRow.removeFromLeft(360));
        moduleLabel.setBounds(topRow.removeFromLeft(70));
        moduleSelector.setBounds(topRow.removeFromLeft(220).reduced(4, 0));
        categoryLabel.setBounds(topRow.removeFromLeft(80));
        categorySelector.setBounds(topRow.removeFromLeft(220).reduced(4, 0));
        backToEffectsButton.setBounds(topRow.removeFromRight(180).reduced(4, 0));

        bounds.removeFromTop(4);
        statusLabel.setBounds(bounds.removeFromTop(24));
        bounds.removeFromTop(6);

        auto left = bounds.removeFromLeft(bounds.getWidth() / 2 + 30);
        voicesList.setBounds(left);

        auto right = bounds.reduced(8, 0);
        detailGroup.setBounds(right);
        auto inner = right.reduced(16, 34);
        detailTrackSource.setBounds(inner.removeFromTop(26));
        detailName.setBounds(inner.removeFromTop(30));
        detailBank.setBounds(inner.removeFromTop(26));
        detailProgram.setBounds(inner.removeFromTop(26));
    }

private:
    static void applyStartButtonStyle(juce::Button& button)
    {
        const auto baseOff = juce::Colours::lightgrey;
        const auto baseOn = juce::Colours::lightgrey;
        const auto baseText = juce::Colours::black;
        button.setColour(juce::TextButton::buttonColourId, baseOff);
        button.setColour(juce::TextButton::buttonOnColourId, baseOn);
        button.setColour(juce::TextButton::textColourOffId, baseText);
        button.setColour(juce::TextButton::textColourOnId, baseText);
    }

    int getNumRows() override
    {
        return static_cast<int>(currentVoices.size());
    }

    void paintListBoxItem(int rowNumber,
                          juce::Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(currentVoices.size()))
            return;

        if (rowIsSelected)
            g.fillAll(juce::Colours::darkblue.withAlpha(0.45f));

        g.setColour(juce::Colours::antiquewhite);
        g.drawText(currentVoices[(size_t) rowNumber].name, 8, 0, width - 10, height, juce::Justification::centredLeft);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        if (lastRowSelected < 0 || lastRowSelected >= static_cast<int>(currentVoices.size()))
            return;

        const auto& voice = currentVoices[(size_t) lastRowSelected];
        TrackSoundProgram pending;
        pending.bankMsb = voice.bankMsb;
        pending.bankLsb = voice.bankLsb;
        pending.program = voice.program;
        pending.voiceName = voice.name;
        pending.configured = true;
        pendingSelection = pending;
        scorePage.previewTrackSoundProgram(scorePage.getTrackSoundEditingTrackIndex(), pending, true);
        refreshDetailPanel();
    }

    void timerCallback() override
    {
        const int trackIndex = scorePage.getTrackSoundEditingTrackIndex();
        if (trackIndex != displayedTrackIndex)
        {
            displayedTrackIndex = trackIndex;
            pendingSelection = scorePage.getTrackSoundProgram(trackIndex);
            refreshTrackHeader();
            refreshDetailPanel();
        }
    }

    void refreshCategories()
    {
        categorySelector.clear(juce::dontSendNotification);
        for (const auto& category : catalog.getCategories())
            categorySelector.addItem(category.name, categorySelector.getNumItems() + 1);
        if (categorySelector.getNumItems() > 0)
            categorySelector.setSelectedItemIndex(0, juce::dontSendNotification);
        refreshVoices();
    }

    void refreshVoices()
    {
        currentVoices.clear();
        const int selected = categorySelector.getSelectedItemIndex();
        if (selected >= 0 && selected < static_cast<int>(catalog.getCategories().size()))
            currentVoices = catalog.getCategories()[(size_t) selected].voices;

        voicesList.updateContent();
        voicesList.repaint();
        refreshTrackHeader();
        refreshDetailPanel();
    }

    void refreshTrackHeader()
    {
        const int trackIndex = scorePage.getTrackSoundEditingTrackIndex();
        trackLabel.setText("Track " + juce::String(trackIndex + 1) + ": " + scorePage.getTrackDisplayName(trackIndex),
                           juce::dontSendNotification);
        statusLabel.setText(catalog.getVendorName().isNotEmpty()
                                ? ("Vendor: " + catalog.getVendorName())
                                : "Select a sound and click Back to Effects to save.",
                            juce::dontSendNotification);
    }

    void refreshDetailPanel()
    {
        const int trackIndex = scorePage.getTrackSoundEditingTrackIndex();
        const auto active = pendingSelection.has_value()
            ? pendingSelection.value()
            : scorePage.getTrackSoundProgram(trackIndex);
        const auto trackSourceText = juce::String(trackIndex + 1).paddedLeft('0', 2)
            + ": " + scorePage.getTrackDisplayName(trackIndex);
        detailTrackSource.setText("GM Source Track: " + trackSourceText, juce::dontSendNotification);
        detailName.setText("Name: " + buildTrackSoundDisplayName(active), juce::dontSendNotification);
        detailBank.setText("Bank: MSB " + juce::String(active.bankMsb) + " / LSB " + juce::String(active.bankLsb),
                           juce::dontSendNotification);
        detailProgram.setText("Program: " + juce::String(active.program + 1), juce::dontSendNotification);
    }

    MainComponent& scorePage;
    InstrumentCatalog catalog;
    std::function<void()> navigateToEffectsAction;
    std::optional<TrackSoundProgram> pendingSelection;
    std::vector<InstrumentVoice> currentVoices;
    int displayedTrackIndex = -1;

    juce::Label trackLabel;
    juce::Label moduleLabel;
    juce::ComboBox moduleSelector;
    juce::Label categoryLabel;
    juce::ComboBox categorySelector;
    juce::Label statusLabel;
    juce::ListBox voicesList;
    juce::GroupComponent detailGroup;
    juce::Label detailTrackSource;
    juce::Label detailName;
    juce::Label detailBank;
    juce::Label detailProgram;
    juce::TextButton backToEffectsButton;
};
