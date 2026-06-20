#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <functional>
#include <vector>

class MidiFilePlaybackEngineAdapter
{
public:
    struct ScheduledEvent
    {
        double playbackTimeSeconds = 0.0;
        juce::MidiMessage message;
    };

    struct ProcessResult
    {
        int emittedEventCount = 0;
        bool reachedEndOfStream = false;
    };

    bool loadFromFile(const juce::File& file, juce::String& error)
    {
        clear();
        error.clear();

        juce::FileInputStream stream(file);
        if (!stream.openedOk())
        {
            error = "Failed to open file: " + file.getFullPathName();
            return false;
        }

        juce::MidiFile midiFile;
        if (!midiFile.readFrom(stream))
        {
            error = "Failed to parse MIDI: " + file.getFileName();
            return false;
        }

        midiFile.convertTimestampTicksToSeconds();
        for (int track = 0; track < midiFile.getNumTracks(); ++track)
        {
            const auto* sequence = midiFile.getTrack(track);
            if (sequence == nullptr)
                continue;

            for (int eventIndex = 0; eventIndex < sequence->getNumEvents(); ++eventIndex)
            {
                const auto* holder = sequence->getEventPointer(eventIndex);
                if (holder == nullptr)
                    continue;

                const auto& message = holder->message;
                if (message.isMetaEvent() || message.isEndOfTrackMetaEvent())
                    continue;

                events.push_back({ juce::jmax(0.0, message.getTimeStamp()), message });
            }
        }

        std::sort(events.begin(), events.end(),
                  [](const ScheduledEvent& a, const ScheduledEvent& b)
                  {
                      return a.playbackTimeSeconds < b.playbackTimeSeconds;
                  });
        return !events.empty();
    }

    void clear()
    {
        events.clear();
        nextEventIndex = 0;
        lastProcessedPlaybackSec = 0.0;
    }

    void seekToPlaybackTime(double playbackSec)
    {
        const auto clamped = juce::jmax(0.0, playbackSec);
        const auto it = std::lower_bound(events.begin(), events.end(), clamped,
                                         [](const ScheduledEvent& event, double sec)
                                         {
                                             return event.playbackTimeSeconds < sec;
                                         });
        nextEventIndex = static_cast<int>(std::distance(events.begin(), it));
        lastProcessedPlaybackSec = clamped;
    }

    ProcessResult processUntilPlaybackTime(double playbackSec, const std::function<void(const juce::MidiMessage&)>& sink)
    {
        ProcessResult result;
        if (sink == nullptr)
            return result;

        const double targetSec = juce::jmax(0.0, playbackSec);
        if (targetSec + 1.0e-9 < lastProcessedPlaybackSec)
            seekToPlaybackTime(targetSec);

        while (nextEventIndex < static_cast<int>(events.size())
               && events[(size_t) nextEventIndex].playbackTimeSeconds <= targetSec)
        {
            sink(events[(size_t) nextEventIndex].message);
            ++nextEventIndex;
            ++result.emittedEventCount;
        }

        lastProcessedPlaybackSec = targetSec;
        result.reachedEndOfStream = nextEventIndex >= static_cast<int>(events.size());
        return result;
    }

    bool hasEvents() const { return !events.empty(); }
    bool hasPendingEvents() const { return nextEventIndex < static_cast<int>(events.size()); }
    int getEventCount() const { return static_cast<int>(events.size()); }
    const std::vector<ScheduledEvent>& getEvents() const { return events; }

private:
    std::vector<ScheduledEvent> events;
    int nextEventIndex = 0;
    double lastProcessedPlaybackSec = 0.0;
};
