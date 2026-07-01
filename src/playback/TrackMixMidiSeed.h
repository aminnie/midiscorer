#pragma once

#include <JuceHeader.h>
#include <optional>
#include <vector>
#include "TrackMixProcessor.h"
#include "TrackMixState.h"

class TrackMixMidiSeed
{
public:
    static constexpr int kDefaultVolume = 100;
    static constexpr int kDefaultExpression = 100;
    static constexpr int kDefaultReverb = 10;
    static constexpr int kDefaultChannel = 1;
    static constexpr int kVolumeController = 7;
    static constexpr int kExpressionController = 11;

    static std::optional<int> findFirstChannel(const juce::MidiMessageSequence& sequence)
    {
        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto* holder = sequence.getEventPointer(i);
            if (holder == nullptr)
                continue;

            const auto& msg = holder->message;
            if (msg.isMetaEvent())
                continue;

            return juce::jlimit(1, 16, msg.getChannel());
        }
        return std::nullopt;
    }

    static std::optional<int> findLastControllerValue(const juce::MidiMessageSequence& sequence, int controllerNumber)
    {
        std::optional<int> lastValue;
        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto* holder = sequence.getEventPointer(i);
            if (holder == nullptr)
                continue;

            const auto& msg = holder->message;
            if (msg.isMetaEvent())
                continue;

            if (msg.isController() && msg.getControllerNumber() == controllerNumber)
                lastValue = msg.getControllerValue();
        }
        return lastValue;
    }

    static int volumeFromTrackSequence(const juce::MidiMessageSequence& sequence)
    {
        const auto cc = findLastControllerValue(sequence, kVolumeController);
        return cc.has_value() ? juce::jlimit(0, 127, *cc) : kDefaultVolume;
    }

    static int reverbFromTrackSequence(const juce::MidiMessageSequence& sequence)
    {
        const auto cc = findLastControllerValue(sequence, TrackMixProcessor::kReverbController);
        return cc.has_value() ? juce::jlimit(0, 127, *cc) : kDefaultReverb;
    }

    static int expressionFromTrackSequence(const juce::MidiMessageSequence& sequence)
    {
        const auto cc = findLastControllerValue(sequence, kExpressionController);
        return cc.has_value() ? juce::jlimit(0, 127, *cc) : kDefaultExpression;
    }

    static int channelFromTrackSequence(const juce::MidiMessageSequence& sequence)
    {
        const auto channel = findFirstChannel(sequence);
        return channel.has_value() ? *channel : kDefaultChannel;
    }

    static void applyFromTrackSequences(const std::vector<juce::MidiMessageSequence>& sequences, TrackMixState& mixState)
    {
        mixState.resizeForTrackCount(static_cast<int>(sequences.size()));
        for (int i = 0; i < static_cast<int>(sequences.size()); ++i)
        {
            mixState.setVolume(i, volumeFromTrackSequence(sequences[(size_t) i]));
            mixState.setExpression(i, expressionFromTrackSequence(sequences[(size_t) i]));
            mixState.setReverb(i, reverbFromTrackSequence(sequences[(size_t) i]));
            mixState.setChannel(i, channelFromTrackSequence(sequences[(size_t) i]));
        }
    }
};
