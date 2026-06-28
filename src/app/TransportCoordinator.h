#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../midi/MidiProjectLoader.h"
#include "../notation/ScoreRenderer.h"
#include "../playback/MidiFilePlaybackEngineAdapter.h"
#include "../playback/MidiOutputDevice.h"
#include "../playback/PlaybackController.h"

class TransportCoordinator
{
public:
    struct Context
    {
        const MidiProjectData* project = nullptr;
        PlaybackController* playbackController = nullptr;
        MidiFilePlaybackEngineAdapter* midiPlaybackEngine = nullptr;
        MidiOutputDevice* midiOutputDevice = nullptr;
        ScoreRenderer* renderers[3] = {};
        juce::TextButton* transportToggleButton = nullptr;
        juce::TextButton* continueButton = nullptr;
        juce::TextEditor* continueBarInput = nullptr;
        juce::ToggleButton* loopEnabledToggle = nullptr;
        juce::TextEditor* loopStartInput = nullptr;
        juce::TextEditor* loopEndInput = nullptr;
        juce::Label* transportLabel = nullptr;
        bool* continueArmed = nullptr;
        bool* loopEnabled = nullptr;
        int* loopStartBar = nullptr;
        int* loopEndBar = nullptr;
        int* displayedBar = nullptr;
        std::function<void(const juce::String&)> setStatusMessage;
        std::function<void()> resetLiveChordState;
        std::function<void()> updateTransportControls;
    };

    static void startPlayback(const Context& ctx)
    {
        if (ctx.project == nullptr || ctx.project->tracks.empty() || ctx.playbackController == nullptr)
            return;

        ctx.playbackController->playFromStart();
        if (ctx.midiPlaybackEngine != nullptr)
            ctx.midiPlaybackEngine->seekToPlaybackTime(0.0);
        if (ctx.continueArmed != nullptr)
            *ctx.continueArmed = false;
        if (ctx.resetLiveChordState)
            ctx.resetLiveChordState();
        for (auto* renderer : ctx.renderers)
        {
            if (renderer != nullptr)
                renderer->setCurrentBar(1);
        }
        if (ctx.transportLabel != nullptr)
            ctx.transportLabel->setText("Bar 1", juce::dontSendNotification);
        if (ctx.displayedBar != nullptr)
            *ctx.displayedBar = 1;
        if (ctx.updateTransportControls)
            ctx.updateTransportControls();
        if (ctx.setStatusMessage)
            ctx.setStatusMessage("Playback running");
    }

    static void continuePlaybackFromBar(const Context& ctx)
    {
        if (ctx.project == nullptr || ctx.project->tracks.empty() || ctx.continueArmed == nullptr
            || !(*ctx.continueArmed) || ctx.playbackController == nullptr || ctx.continueBarInput == nullptr)
            return;

        const int requestedBar = ctx.continueBarInput->getText().trim().getIntValue();
        const int clampedBar = juce::jlimit(1, juce::jmax(1, ctx.project->maxBar), juce::jmax(1, requestedBar));
        ctx.continueBarInput->setText(juce::String(clampedBar), juce::dontSendNotification);

        ctx.playbackController->playFromBar(clampedBar);
        if (ctx.midiPlaybackEngine != nullptr)
            ctx.midiPlaybackEngine->seekToPlaybackTime(ctx.project->tempoMap.barToSecondsDownbeat(clampedBar));
        *ctx.continueArmed = false;
        if (ctx.resetLiveChordState)
            ctx.resetLiveChordState();
        for (auto* renderer : ctx.renderers)
        {
            if (renderer != nullptr)
                renderer->setCurrentBar(clampedBar);
        }
        if (ctx.transportLabel != nullptr)
            ctx.transportLabel->setText("Bar " + juce::String(clampedBar), juce::dontSendNotification);
        if (ctx.displayedBar != nullptr)
            *ctx.displayedBar = clampedBar;
        if (ctx.updateTransportControls)
            ctx.updateTransportControls();
        if (ctx.setStatusMessage)
            ctx.setStatusMessage("Playback continuing from bar " + juce::String(clampedBar));
    }

    static void stopPlayback(const Context& ctx, bool userInitiated)
    {
        if (ctx.playbackController == nullptr)
            return;

        if (userInitiated && ctx.project != nullptr && !ctx.project->tracks.empty() && ctx.continueBarInput != nullptr)
        {
            const int currentBar = juce::jmax(1, ctx.playbackController->getCurrentBar());
            ctx.continueBarInput->setText(juce::String(currentBar), juce::dontSendNotification);
        }

        ctx.playbackController->stop();
        if (ctx.midiPlaybackEngine != nullptr)
            ctx.midiPlaybackEngine->seekToPlaybackTime(0.0);
        if (ctx.midiOutputDevice != nullptr)
            ctx.midiOutputDevice->sendAllNotesOff();
        if (ctx.continueArmed != nullptr)
            *ctx.continueArmed = userInitiated && ctx.project != nullptr && !ctx.project->tracks.empty();
        if (ctx.updateTransportControls)
            ctx.updateTransportControls();
        if (userInitiated && ctx.setStatusMessage)
            ctx.setStatusMessage("Playback stopped");
    }

    static void onTransportToggleClicked(const Context& ctx)
    {
        if (ctx.playbackController == nullptr)
            return;

        if (ctx.playbackController->isPlaying())
            stopPlayback(ctx, true);
        else
            startPlayback(ctx);
    }

    static void refreshTransportToggleButtonText(const Context& ctx)
    {
        if (ctx.transportToggleButton == nullptr || ctx.playbackController == nullptr)
            return;

        ctx.transportToggleButton->setButtonText(ctx.playbackController->isPlaying() ? "Stop" : "Start");
    }

    static void refreshTransportToggleButtonStyle(const Context& ctx, bool hasProject, bool isPlaying)
    {
        if (ctx.transportToggleButton == nullptr)
            return;

        const auto readyOff = juce::Colours::darkgreen;
        const auto readyOn = juce::Colours::darkgreen.brighter();
        const auto playingOff = juce::Colours::darkred;
        const auto playingOn = juce::Colours::darkred.brighter();
        const auto disabled = juce::Colours::darkgrey;

        ctx.transportToggleButton->setColour(juce::TextButton::textColourOffId,
                                             hasProject && isPlaying ? juce::Colours::white : juce::Colours::black);
        ctx.transportToggleButton->setColour(juce::TextButton::textColourOnId,
                                             hasProject && isPlaying ? juce::Colours::white : juce::Colours::black);
        ctx.transportToggleButton->setColour(juce::TextButton::buttonColourId,
                                             hasProject ? (isPlaying ? playingOff : readyOff) : disabled);
        ctx.transportToggleButton->setColour(juce::TextButton::buttonOnColourId,
                                             hasProject ? (isPlaying ? playingOn : readyOn) : disabled);
    }

    static void updateTransportControls(const Context& ctx)
    {
        const bool hasProject = ctx.project != nullptr && !ctx.project->tracks.empty();
        const bool isPlaying = ctx.playbackController != nullptr && ctx.playbackController->isPlaying();
        if (ctx.transportToggleButton != nullptr)
            ctx.transportToggleButton->setEnabled(hasProject);
        refreshTransportToggleButtonText(ctx);
        refreshTransportToggleButtonStyle(ctx, hasProject, isPlaying);
        if (ctx.continueButton != nullptr)
            ctx.continueButton->setEnabled(hasProject && !isPlaying && ctx.continueArmed != nullptr && *ctx.continueArmed);
        if (ctx.continueBarInput != nullptr)
            ctx.continueBarInput->setEnabled(hasProject && !isPlaying && ctx.continueArmed != nullptr && *ctx.continueArmed);
        if (ctx.loopEnabledToggle != nullptr)
            ctx.loopEnabledToggle->setEnabled(hasProject);
        if (ctx.loopStartInput != nullptr)
            ctx.loopStartInput->setEnabled(hasProject);
        if (ctx.loopEndInput != nullptr)
            ctx.loopEndInput->setEnabled(hasProject);
    }

    static bool handleLoopWrap(const Context& ctx, int currentBar)
    {
        if (ctx.project == nullptr || ctx.project->tracks.empty() || ctx.loopEnabled == nullptr || !(*ctx.loopEnabled)
            || ctx.loopStartBar == nullptr || ctx.loopEndBar == nullptr || ctx.playbackController == nullptr)
            return false;

        if (*ctx.loopEndBar < *ctx.loopStartBar || currentBar <= *ctx.loopEndBar)
            return false;

        ctx.playbackController->seekToSecond(ctx.project->tempoMap.barToSecondsDownbeat(*ctx.loopStartBar));
        if (ctx.midiPlaybackEngine != nullptr)
            ctx.midiPlaybackEngine->seekToPlaybackTime(ctx.project->tempoMap.barToSecondsDownbeat(*ctx.loopStartBar));
        if (ctx.midiOutputDevice != nullptr)
            ctx.midiOutputDevice->sendAllNotesOff();
        if (ctx.resetLiveChordState)
            ctx.resetLiveChordState();
        for (auto* renderer : ctx.renderers)
        {
            if (renderer != nullptr)
                renderer->setCurrentBar(*ctx.loopStartBar);
        }
        if (ctx.transportLabel != nullptr)
            ctx.transportLabel->setText("Bar " + juce::String(*ctx.loopStartBar), juce::dontSendNotification);
        if (ctx.displayedBar != nullptr)
            *ctx.displayedBar = *ctx.loopStartBar;
        return true;
    }
};
