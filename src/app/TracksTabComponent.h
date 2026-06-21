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
        // #region agent log
        appendDebugLog("post-fix-solo-crash-1",
                       "H7",
                       "src/app/TracksTabComponent.h:ctor",
                       "TracksTabComponent initialized with deferred-refresh instrumentation",
                       juce::var());
        // #endregion
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        refreshTrackRows();
        startTimerHz(2);
    }

    void resized() override
    {
        viewport.setBounds(getLocalBounds().reduced(8));
        layoutRows();
    }

private:
    static void appendDebugLog(const juce::String& runId,
                               const juce::String& hypothesisId,
                               const juce::String& location,
                               const juce::String& message,
                               juce::var data)
    {
        auto payload = std::make_unique<juce::DynamicObject>();
        payload->setProperty("sessionId", "e2bc95");
        payload->setProperty("runId", runId);
        payload->setProperty("hypothesisId", hypothesisId);
        payload->setProperty("location", location);
        payload->setProperty("message", message);
        payload->setProperty("data", data);
        payload->setProperty("timestamp", static_cast<juce::int64>(juce::Time::currentTimeMillis()));
        juce::File("C:/Users/a_min/midiscorer/debug-e2bc95.log")
            .appendText(juce::JSON::toString(juce::var(payload.release()), true) + "\n");
    }

    struct TrackRow
    {
        int trackIndex = -1;
        std::unique_ptr<juce::GroupComponent> group;
        std::unique_ptr<juce::Label> volumeLabel;
        std::unique_ptr<juce::Slider> volumeSlider;
        std::unique_ptr<juce::ToggleButton> muteToggle;
        std::unique_ptr<juce::ToggleButton> soloToggle;
    };

    void timerCallback() override
    {
        if (refreshPending)
        {
            // #region agent log
            appendDebugLog("post-fix-solo-crash-1",
                           "F1",
                           "src/app/TracksTabComponent.h:timerCallback",
                           "Processing deferred track-row refresh from callback",
                           juce::var());
            // #endregion
            refreshPending = false;
            trackSignature.clear();
            refreshTrackRows();
            return;
        }

        const auto newSignature = buildTrackSignature();
        if (newSignature != trackSignature)
        {
            auto data = std::make_unique<juce::DynamicObject>();
            data->setProperty("oldSignatureLen", trackSignature.length());
            data->setProperty("newSignatureLen", newSignature.length());
            // #region agent log
            appendDebugLog("tracks-solo-crash-1",
                           "H3",
                           "src/app/TracksTabComponent.h:timerCallback",
                           "Track signature changed; refreshing track rows from timer",
                           juce::var(data.release()));
            // #endregion
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
                      << "|" << juce::String(scorePage.isTrackMuted(i) ? 1 : 0)
                      << "|" << juce::String(scorePage.isTrackSolo(i) ? 1 : 0);
        }
        return signature;
    }

    void refreshTrackRows()
    {
        auto startData = std::make_unique<juce::DynamicObject>();
        startData->setProperty("existingRows", static_cast<int>(rows.size()));
        startData->setProperty("projectTrackCount", scorePage.getTrackMixTrackCount());
        // #region agent log
        appendDebugLog("tracks-solo-crash-1",
                       "H1",
                       "src/app/TracksTabComponent.h:refreshTrackRows",
                       "Entering refreshTrackRows",
                       juce::var(startData.release()));
        // #endregion

        rows.clear();
        content.removeAllChildren();

        for (int i = 0; i < scorePage.getTrackMixTrackCount(); ++i)
        {
            if (!scorePage.isTrackMixEligible(i))
                continue;

            auto row = std::make_unique<TrackRow>();
            row->trackIndex = i;

            row->group = std::make_unique<juce::GroupComponent>();
            row->group->setText(juce::String(i + 1).paddedLeft('0', 2) + ": " + scorePage.getTrackDisplayName(i));
            content.addAndMakeVisible(*row->group);

            row->volumeLabel = std::make_unique<juce::Label>();
            row->volumeLabel->setText("Volume", juce::dontSendNotification);
            row->volumeLabel->setJustificationType(juce::Justification::centredRight);
            content.addAndMakeVisible(*row->volumeLabel);

            row->volumeSlider = std::make_unique<juce::Slider>();
            row->volumeSlider->setRange(0.0, 127.0, 1.0);
            row->volumeSlider->setSliderStyle(juce::Slider::LinearHorizontal);
            row->volumeSlider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
            row->volumeSlider->setColour(juce::Slider::backgroundColourId, juce::Colours::dimgrey);
            row->volumeSlider->setColour(juce::Slider::trackColourId, juce::Colours::green.darker());
            row->volumeSlider->setColour(juce::Slider::thumbColourId, juce::Colours::darkred.brighter());
            row->volumeSlider->setValue(scorePage.getTrackMixVolume(i), juce::dontSendNotification);
            row->volumeSlider->onValueChange = [this, idx = i, slider = row->volumeSlider.get()]
            {
                scorePage.setTrackMixVolume(idx, static_cast<int>(std::round(slider->getValue())));
                trackSignature = buildTrackSignature();
            };
            content.addAndMakeVisible(*row->volumeSlider);

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
                auto clickData = std::make_unique<juce::DynamicObject>();
                clickData->setProperty("trackIndex", idx);
                clickData->setProperty("toggleState", toggle != nullptr ? toggle->getToggleState() : false);
                clickData->setProperty("togglePtrNull", toggle == nullptr);
                // #region agent log
                appendDebugLog("tracks-solo-crash-1",
                               "H2",
                               "src/app/TracksTabComponent.h:soloToggle.onClick",
                               "Solo toggle callback entered",
                               juce::var(clickData.release()));
                // #endregion
                scorePage.setTrackSolo(idx, toggle->getToggleState());
                // #region agent log
                appendDebugLog("tracks-solo-crash-1",
                               "H4",
                               "src/app/TracksTabComponent.h:soloToggle.onClick",
                               "Deferring refresh from solo callback",
                               juce::var());
                // #endregion
                refreshPending = true;
                // #region agent log
                appendDebugLog("tracks-solo-crash-1",
                               "H4",
                               "src/app/TracksTabComponent.h:soloToggle.onClick",
                               "Marked refreshPending in solo callback",
                               juce::var());
                // #endregion
                // #region agent log
                appendDebugLog("tracks-solo-crash-1",
                               "H6",
                               "src/app/TracksTabComponent.h:soloToggle.onClick",
                               "Completed solo callback tail after deferred refresh setup",
                               juce::var());
                // #endregion
            };
            content.addAndMakeVisible(*row->soloToggle);

            rows.push_back(std::move(row));
        }

        layoutRows();
        repaint();

        auto endData = std::make_unique<juce::DynamicObject>();
        endData->setProperty("rebuiltRows", static_cast<int>(rows.size()));
        endData->setProperty("contentHeight", content.getHeight());
        // #region agent log
        appendDebugLog("tracks-solo-crash-1",
                       "H1",
                       "src/app/TracksTabComponent.h:refreshTrackRows",
                       "Completed refreshTrackRows",
                       juce::var(endData.release()));
        // #endregion
    }

    void layoutRows()
    {
        auto bounds = viewport.getLocalBounds();
        const int rowHeight = 84;
        const int gap = 6;
        const int contentWidth = juce::jmax(560, bounds.getWidth() - 20);
        int y = 0;

        for (auto& row : rows)
        {
            auto rowArea = juce::Rectangle<int>(0, y, contentWidth, rowHeight);
            row->group->setBounds(rowArea);
            auto controls = rowArea.reduced(12, 30);

            row->volumeLabel->setBounds(controls.removeFromLeft(72));
            row->volumeSlider->setBounds(controls.removeFromLeft(280).reduced(4, 0));
            row->muteToggle->setBounds(controls.removeFromLeft(88).reduced(4, 0));
            row->soloToggle->setBounds(controls.removeFromLeft(88).reduced(4, 0));
            y += rowHeight + gap;
        }

        content.setSize(contentWidth, juce::jmax(y, bounds.getHeight()));
    }

    MainComponent& scorePage;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<TrackRow>> rows;
    juce::String trackSignature;
    bool refreshPending = false;
};
