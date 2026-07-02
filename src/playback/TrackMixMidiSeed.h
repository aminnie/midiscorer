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
    static constexpr int kDefaultBankMsb = 0;
    static constexpr int kDefaultBankLsb = 0;
    static constexpr int kDefaultProgram = 0;
    static constexpr int kVolumeController = 7;
    static constexpr int kExpressionController = 11;
    static constexpr int kBankMsbController = 0;
    static constexpr int kBankLsbController = 32;

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

    static std::optional<int> findLastProgramChange(const juce::MidiMessageSequence& sequence)
    {
        std::optional<int> lastProgram;
        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto* holder = sequence.getEventPointer(i);
            if (holder == nullptr)
                continue;

            const auto& msg = holder->message;
            if (msg.isMetaEvent())
                continue;

            if (msg.isProgramChange())
                lastProgram = juce::jlimit(0, 127, msg.getProgramChangeNumber());
        }
        return lastProgram;
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

            const auto bankMsb = findLastControllerValue(sequences[(size_t) i], kBankMsbController);
            const auto bankLsb = findLastControllerValue(sequences[(size_t) i], kBankLsbController);
            const auto program = findLastProgramChange(sequences[(size_t) i]);
            mixState.setBankMsb(i, bankMsb.has_value() ? juce::jlimit(0, 127, *bankMsb) : kDefaultBankMsb);
            mixState.setBankLsb(i, bankLsb.has_value() ? juce::jlimit(0, 127, *bankLsb) : kDefaultBankLsb);
            mixState.setProgram(i, program.has_value() ? *program : kDefaultProgram);
            const bool hasSeededSound = bankMsb.has_value() || bankLsb.has_value() || program.has_value();
            mixState.setSoundConfigured(i, hasSeededSound);
            mixState.setSoundName(i, {});
        }
    }
};
