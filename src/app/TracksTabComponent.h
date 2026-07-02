#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <memory>
#include <vector>
#include "MainComponent.h"

class TracksTabComponent final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit TracksTabComponent(MainComponent& scorePageRef)
        : scorePage(scorePageRef)
    {
        addAndMakeVisible(mixSaveStatusLabel);
        mixSaveStatusLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        refreshMixSaveStatus();
        refreshTrackRows();
        startTimerHz(2);
    }

    void setOpenSoundsAction(std::function<void(int)> action)
    {
        openSoundsAction = std::move(action);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(8);
        mixSaveStatusLabel.setBounds(bounds.removeFromTop(22));
        bounds.removeFromTop(4);
        viewport.setBounds(bounds);
        layoutRows();
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

    struct TrackRow
    {
        int trackIndex = -1;
        std::unique_ptr<juce::GroupComponent> group;
        std::unique_ptr<juce::Label> volumeLabel;
        std::unique_ptr<juce::Slider> volumeSlider;
        std::unique_ptr<juce::Label> expressionLabel;
        std::unique_ptr<juce::Slider> expressionSlider;
        std::unique_ptr<juce::Label> reverbLabel;
        std::unique_ptr<juce::Slider> reverbSlider;
        std::unique_ptr<juce::Label> channelLabel;
        std::unique_ptr<juce::TextEditor> channelInput;
        std::unique_ptr<juce::ToggleButton> muteToggle;
        std::unique_ptr<juce::ToggleButton> soloToggle;
        std::unique_ptr<juce::Label> soundLabel;
        std::unique_ptr<juce::TextButton> soundButton;
        std::unique_ptr<juce::TextButton> soundHelpButton;
    };

    static void styleMixSlider(juce::Slider& slider)
    {
        slider.setColour(juce::Slider::backgroundColourId, juce::Colours::dimgrey);
        slider.setColour(juce::Slider::trackColourId, juce::Colours::antiquewhite);
        slider.setColour(juce::Slider::thumbColourId, juce::Colours::darkred.brighter());
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::antiquewhite);
        slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void timerCallback() override
    {
        refreshMixSaveStatus();

        if (refreshPending)
        {
            refreshPending = false;
            trackSignature.clear();
            refreshTrackRows();
            return;
        }

        const auto newSignature = buildTrackSignature();
        if (newSignature != trackSignature)
        {
            trackSignature = newSignature;
            refreshTrackRows();
        }
    }

    juce::String buildTrackSignature() const
    {
        juce::String signature = scorePage.getLoadedMidiFileName() + "|" + juce::String(scorePage.getTrackMixTrackCount());
        for (int i = 0; i < scorePage.getTrackMixTrackCount(); ++i)
        {
            signature << "|" << scorePage.getTrackDisplayName(i)
                      << "|" << juce::String(scorePage.getTrackMixVolume(i))
                      << "|" << juce::String(scorePage.getTrackMixExpression(i))
                      << "|" << juce::String(scorePage.getTrackMixReverb(i))
                      << "|" << juce::String(scorePage.getTrackMixChannel(i))
                      << "|" << scorePage.getTrackSoundDisplayName(i)
                      << "|" << juce::String(scorePage.isTrackMuted(i) ? 1 : 0)
                      << "|" << juce::String(scorePage.isTrackSolo(i) ? 1 : 0);
        }
        return signature;
    }

    void refreshTrackRows()
    {
        rows.clear();
        content.removeAllChildren();

        for (int i = 0; i < scorePage.getTrackMixTrackCount(); ++i)
        {
            if (!scorePage.isTrackMixEligible(i))
                continue;

            auto row = std::make_unique<TrackRow>();
            row->trackIndex = i;

            row->group = std::make_unique<juce::GroupComponent>();
            row->group->setText("Track " + juce::String(i + 1).paddedLeft('0', 2) + ": " + scorePage.getTrackDisplayName(i));
            content.addAndMakeVisible(*row->group);

            row->volumeLabel = std::make_unique<juce::Label>();
            row->volumeLabel->setText("Volume", juce::dontSendNotification);
            row->volumeLabel->setJustificationType(juce::Justification::centredRight);
            content.addAndMakeVisible(*row->volumeLabel);

            row->volumeSlider = std::make_unique<juce::Slider>();
            row->volumeSlider->setRange(0.0, 127.0, 1.0);
            row->volumeSlider->setSliderStyle(juce::Slider::LinearHorizontal);
            row->volumeSlider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 22);
            styleMixSlider(*row->volumeSlider);
            row->volumeSlider->setValue(scorePage.getTrackMixVolume(i), juce::dontSendNotification);
            row->volumeSlider->onValueChange = [this, idx = i, slider = row->volumeSlider.get()]
            {
                scorePage.setTrackMixVolume(idx, static_cast<int>(std::round(slider->getValue())));
                trackSignature = buildTrackSignature();
            };
            content.addAndMakeVisible(*row->volumeSlider);

            row->reverbLabel = std::make_unique<juce::Label>();
            row->reverbLabel->setText("Reverb", juce::dontSendNotification);
            row->reverbLabel->setJustificationType(juce::Justification::centredRight);
            content.addAndMakeVisible(*row->reverbLabel);

            row->reverbSlider = std::make_unique<juce::Slider>();
            row->reverbSlider->setRange(0.0, 127.0, 1.0);
            row->reverbSlider->setSliderStyle(juce::Slider::LinearHorizontal);
            row->reverbSlider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 22);
            styleMixSlider(*row->reverbSlider);
            row->reverbSlider->setValue(scorePage.getTrackMixReverb(i), juce::dontSendNotification);
            row->reverbSlider->onValueChange = [this, idx = i, slider = row->reverbSlider.get()]
            {
                scorePage.setTrackMixReverb(idx, static_cast<int>(std::round(slider->getValue())));
                trackSignature = buildTrackSignature();
            };
            content.addAndMakeVisible(*row->reverbSlider);

            row->expressionLabel = std::make_unique<juce::Label>();
            row->expressionLabel->setText("Expression", juce::dontSendNotification);
            row->expressionLabel->setJustificationType(juce::Justification::centredRight);
            content.addAndMakeVisible(*row->expressionLabel);

            row->expressionSlider = std::make_unique<juce::Slider>();
            row->expressionSlider->setRange(0.0, 127.0, 1.0);
            row->expressionSlider->setSliderStyle(juce::Slider::LinearHorizontal);
            row->expressionSlider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 22);
            styleMixSlider(*row->expressionSlider);
            row->expressionSlider->setValue(scorePage.getTrackMixExpression(i), juce::dontSendNotification);
            row->expressionSlider->onValueChange = [this, idx = i, slider = row->expressionSlider.get()]
            {
                scorePage.setTrackMixExpression(idx, static_cast<int>(std::round(slider->getValue())));
                trackSignature = buildTrackSignature();
            };
            content.addAndMakeVisible(*row->expressionSlider);

            row->channelLabel = std::make_unique<juce::Label>();
            row->channelLabel->setText("Chan", juce::dontSendNotification);
            row->channelLabel->setJustificationType(juce::Justification::centredRight);
            content.addAndMakeVisible(*row->channelLabel);

            row->channelInput = std::make_unique<juce::TextEditor>();
            row->channelInput->setInputRestrictions(2, "0123456789");
            row->channelInput->setText(juce::String(scorePage.getTrackMixChannel(i)), juce::dontSendNotification);
            row->channelInput->setJustification(juce::Justification::centred);
            row->channelInput->onReturnKey = [this, idx = i, input = row->channelInput.get()]
            {
                applyChannelInput(idx, input);
            };
            row->channelInput->onFocusLost = [this, idx = i, input = row->channelInput.get()]
            {
                applyChannelInput(idx, input);
            };
            content.addAndMakeVisible(*row->channelInput);

            row->muteToggle = std::make_unique<juce::ToggleButton>("Mute");
            row->muteToggle->setToggleState(scorePage.isTrackMuted(i), juce::dontSendNotification);
            row->muteToggle->onClick = [this, idx = i, toggle = row->muteToggle.get()]
            {
                scorePage.setTrackMuted(idx, toggle->getToggleState());
                trackSignature = buildTrackSignature();
            };
            content.addAndMakeVisible(*row->muteToggle);

            row->soloToggle = std::make_unique<juce::ToggleButton>("Solo");
            row->soloToggle->setToggleState(scorePage.isTrackSolo(i), juce::dontSendNotification);
            row->soloToggle->onClick = [this, idx = i, toggle = row->soloToggle.get()]
            {
                scorePage.setTrackSolo(idx, toggle->getToggleState());
                refreshPending = true;
            };
            content.addAndMakeVisible(*row->soloToggle);

            row->soundLabel = std::make_unique<juce::Label>();
            row->soundLabel->setText("Sound", juce::dontSendNotification);
            row->soundLabel->setJustificationType(juce::Justification::centredRight);
            content.addAndMakeVisible(*row->soundLabel);

            row->soundButton = std::make_unique<juce::TextButton>();
            row->soundButton->setButtonText(scorePage.getTrackSoundDisplayName(i));
            applyStartButtonStyle(*row->soundButton);
            row->soundButton->onClick = [this, idx = i]
            {
                scorePage.setTrackSoundEditingTrackIndex(idx);
                if (openSoundsAction)
                    openSoundsAction(idx);
            };
            content.addAndMakeVisible(*row->soundButton);

            row->soundHelpButton = std::make_unique<juce::TextButton>("?");
            row->soundHelpButton->setTooltip("Show MSB/LSB/Voice override details");
            applyStartButtonStyle(*row->soundHelpButton);
            row->soundHelpButton->onClick = [this, idx = i]
            {
                const auto sound = scorePage.getTrackSoundProgram(idx);
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Track Sound Override",
                    buildTrackSoundCcHelpText(sound),
                    "OK",
                    this);
            };
            content.addAndMakeVisible(*row->soundHelpButton);

            rows.push_back(std::move(row));
        }

        layoutRows();
        repaint();
    }

    void layoutRows()
    {
        auto bounds = viewport.getLocalBounds();
        const int rowHeight = 84;
        const int gap = 6;
        const int contentWidth = juce::jmax(860, bounds.getWidth() - 20);
        int y = 0;

        for (auto& row : rows)
        {
            auto rowArea = juce::Rectangle<int>(0, y, contentWidth, rowHeight);
            row->group->setBounds(rowArea);
            auto controls = rowArea.reduced(12, 30);

            row->channelLabel->setBounds(controls.removeFromLeft(40));
            row->channelInput->setBounds(controls.removeFromLeft(44).reduced(4, 0));
            row->muteToggle->setBounds(controls.removeFromLeft(74).reduced(1, 0));
            row->soloToggle->setBounds(controls.removeFromLeft(74).reduced(1, 0));
            row->soundLabel->setBounds(controls.removeFromLeft(46));
            row->soundButton->setBounds(controls.removeFromLeft(224).reduced(1, 0));
            row->soundHelpButton->setBounds(controls.removeFromLeft(28).reduced(1, 0));
            row->volumeLabel->setBounds(controls.removeFromLeft(72));
            row->volumeSlider->setBounds(controls.removeFromLeft(180).reduced(4, 0));
            row->expressionLabel->setBounds(controls.removeFromLeft(88));
            row->expressionSlider->setBounds(controls.removeFromLeft(180).reduced(4, 0));
            row->reverbLabel->setBounds(controls.removeFromLeft(64));
            row->reverbSlider->setBounds(controls.removeFromLeft(180).reduced(4, 0));
            y += rowHeight + gap;
        }

        content.setSize(contentWidth, juce::jmax(y, bounds.getHeight()));
    }

    void refreshMixSaveStatus()
    {
        const auto text = scorePage.getTrackMixSaveStatusText();
        if (text.isEmpty())
        {
            mixSaveStatusLabel.setVisible(false);
            return;
        }

        mixSaveStatusLabel.setVisible(true);
        mixSaveStatusLabel.setText(text, juce::dontSendNotification);
        switch (scorePage.getTrackMixSaveStatus())
        {
            case MainComponent::TrackMixSaveStatus::saved:
                mixSaveStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
                break;
            case MainComponent::TrackMixSaveStatus::pending:
                mixSaveStatusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
                break;
            case MainComponent::TrackMixSaveStatus::failed:
                mixSaveStatusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
                break;
        }
    }

    void applyChannelInput(int trackIndex, juce::TextEditor* input)
    {
        if (input == nullptr)
            return;

        const int parsed = input->getText().trim().getIntValue();
        const int clamped = juce::jlimit(1, 16, parsed > 0 ? parsed : 1);
        input->setText(juce::String(clamped), juce::dontSendNotification);
        scorePage.setTrackMixChannel(trackIndex, clamped);
        trackSignature = buildTrackSignature();
    }

    MainComponent& scorePage;
    juce::Label mixSaveStatusLabel;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<TrackRow>> rows;
    juce::String trackSignature;
    bool refreshPending = false;
    std::function<void(int)> openSoundsAction;
};
