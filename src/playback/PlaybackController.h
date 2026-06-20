#pragma once

#include <JuceHeader.h>
#include <optional>
#include "PlaybackClock.h"
#include "IPlaybackPositionSource.h"
#include "../midi/TempoMap.h"

class PlaybackController : public IPlaybackPositionSource
{
public:
    void setTempoMap(const TempoMap* mapPtr, double durationSec)
    {
        tempoMap = mapPtr;
        totalDurationSec = juce::jmax(0.0, durationSec);
        clock.stop();
    }

    void setTempoOverrideBpm(std::optional<double> overrideBpmValue, std::optional<double> referenceBpmValue)
    {
        const auto normalizedReference = (referenceBpmValue.has_value() && referenceBpmValue.value() > 1.0e-6)
            ? referenceBpmValue
            : std::optional<double>(120.0);

        if (!overrideBpmValue.has_value() || overrideBpmValue.value() <= 1.0e-6)
        {
            tempoScale = 1.0;
            return;
        }

        tempoScale = juce::jmax(1.0e-4, overrideBpmValue.value() / normalizedReference.value());
    }

    void playFromStart()
    {
        clock.start(0.0);
    }

    void playFromSecond(double second)
    {
        const auto clampedMappedSec = totalDurationSec > 0.0
            ? juce::jlimit(0.0, totalDurationSec, juce::jmax(0.0, second))
            : juce::jmax(0.0, second);
        clock.start(clampedMappedSec / juce::jmax(1.0e-4, tempoScale));
    }

    void playFromBar(int barNumber)
    {
        if (tempoMap == nullptr)
        {
            playFromStart();
            return;
        }

        const int normalizedBar = juce::jmax(1, barNumber);
        playFromSecond(tempoMap->barToSecondsDownbeat(normalizedBar));
    }

    void seekToSecond(double second)
    {
        const auto clampedMappedSec = totalDurationSec > 0.0
            ? juce::jlimit(0.0, totalDurationSec, juce::jmax(0.0, second))
            : juce::jmax(0.0, second);
        clock.seek(clampedMappedSec / juce::jmax(1.0e-4, tempoScale));
    }

    void stop()
    {
        clock.stop();
    }

    void pause()
    {
        clock.pause();
    }

    bool isPlaying() const override
    {
        return clock.isPlaying();
    }

    double getElapsedSeconds() const override
    {
        return clock.getElapsedSeconds() * tempoScale;
    }

    int getCurrentBar() const override
    {
        if (tempoMap == nullptr)
            return 1;
        return tempoMap->secondsToBar(getElapsedSeconds());
    }

    bool hasReachedEnd() const override
    {
        return totalDurationSec > 0.0 && getElapsedSeconds() >= totalDurationSec;
    }

private:
    PlaybackClock clock;
    const TempoMap* tempoMap = nullptr;
    double totalDurationSec = 0.0;
    double tempoScale = 1.0;
};
