#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <vector>
#include "TrackMixState.h"

class TrackMixProcessor
{
public:
    static constexpr int kReverbController = 91;
    static constexpr int kExpressionController = 11;
    static constexpr int kBankMsbController = 0;
    static constexpr int kBankLsbController = 32;

    static bool shouldSendTrack(int trackIndex, const TrackMixState& mixState)
    {
        if (!mixState.isValidTrack(trackIndex))
            return true;

        if (mixState.anySoloActive())
            return mixState.isSolo(trackIndex);

        return !mixState.isMuted(trackIndex);
    }

    static juce::MidiMessage applyVolumeToMessage(const juce::MidiMessage& message,
                                                  int trackIndex,
                                                  const TrackMixState& mixState)
    {
        if (!mixState.isValidTrack(trackIndex) || message.isMetaEvent())
            return message;

        const int outChannel = mixState.getChannel(trackIndex);
        const bool hasSoundOverride = mixState.isSoundConfigured(trackIndex);
        const int trackVolume = mixState.getVolume(trackIndex);
        const int trackExpression = mixState.getExpression(trackIndex);
        const float volumeGain = static_cast<float>(trackVolume) / 127.0f;
        const float expressionGain = static_cast<float>(trackExpression) / 127.0f;
        const float noteOnGain = volumeGain * expressionGain;

        if (message.isNoteOn())
        {
            const int scaledVelocity = juce::jlimit(0, 127, static_cast<int>(std::round(message.getVelocity() * noteOnGain)));
            auto transformed = juce::MidiMessage::noteOn(outChannel,
                                                         message.getNoteNumber(),
                                                         static_cast<juce::uint8>(scaledVelocity));
            transformed.setTimeStamp(message.getTimeStamp());
            return transformed;
        }

        if (message.isNoteOff())
        {
            auto transformed = juce::MidiMessage::noteOff(outChannel, message.getNoteNumber(), message.getVelocity());
            transformed.setTimeStamp(message.getTimeStamp());
            return transformed;
        }

        if (message.isController())
        {
            const int controller = message.getControllerNumber();
            if (hasSoundOverride && (controller == kBankMsbController || controller == kBankLsbController))
            {
                const int value = controller == kBankMsbController
                    ? mixState.getBankMsb(trackIndex)
                    : mixState.getBankLsb(trackIndex);
                auto transformed = juce::MidiMessage::controllerEvent(outChannel, controller, value);
                transformed.setTimeStamp(message.getTimeStamp());
                return transformed;
            }

            if (controller == 7)
            {
                const int scaledValue = juce::jlimit(0, 127, static_cast<int>(std::round(message.getControllerValue() * volumeGain)));
                auto transformed = juce::MidiMessage::controllerEvent(outChannel, controller, scaledValue);
                transformed.setTimeStamp(message.getTimeStamp());
                return transformed;
            }

            if (controller == kExpressionController)
            {
                const int scaledValue = juce::jlimit(0, 127, static_cast<int>(std::round(message.getControllerValue() * expressionGain)));
                auto transformed = juce::MidiMessage::controllerEvent(outChannel, controller, scaledValue);
                transformed.setTimeStamp(message.getTimeStamp());
                return transformed;
            }

            if (controller == kReverbController)
            {
                const int trackReverb = mixState.getReverb(trackIndex);
                const int mergedValue = juce::jlimit(0, 127,
                    static_cast<int>(std::round((static_cast<double>(message.getControllerValue())
                                                 * static_cast<double>(trackReverb)) / 127.0)));
                auto transformed = juce::MidiMessage::controllerEvent(outChannel, controller, mergedValue);
                transformed.setTimeStamp(message.getTimeStamp());
                return transformed;
            }
        }

        if (message.isProgramChange())
        {
            const int program = hasSoundOverride
                ? mixState.getProgram(trackIndex)
                : message.getProgramChangeNumber();
            auto transformed = juce::MidiMessage::programChange(outChannel, program);
            transformed.setTimeStamp(message.getTimeStamp());
            return transformed;
        }

        if (message.isPitchWheel())
        {
            auto transformed = juce::MidiMessage::pitchWheel(outChannel, message.getPitchWheelValue());
            transformed.setTimeStamp(message.getTimeStamp());
            return transformed;
        }

        auto transformed = message;
        transformed.setChannel(outChannel);
        return transformed;
    }

    static std::vector<juce::MidiMessage> buildProgramSelectMessages(int trackIndex, const TrackMixState& mixState)
    {
        std::vector<juce::MidiMessage> messages;
        if (!mixState.isValidTrack(trackIndex) || !mixState.isSoundConfigured(trackIndex))
            return messages;

        const int outChannel = mixState.getChannel(trackIndex);
        return buildProgramSelectMessagesForValues(outChannel,
                                                   mixState.getBankMsb(trackIndex),
                                                   mixState.getBankLsb(trackIndex),
                                                   mixState.getProgram(trackIndex));
    }

    static std::vector<juce::MidiMessage> buildProgramSelectMessagesForValues(int outChannel,
                                                                               int bankMsb,
                                                                               int bankLsb,
                                                                               int program)
    {
        std::vector<juce::MidiMessage> messages;
        messages.push_back(juce::MidiMessage::controllerEvent(juce::jlimit(1, 16, outChannel),
                                                              kBankMsbController,
                                                              juce::jlimit(0, 127, bankMsb)));
        messages.push_back(juce::MidiMessage::controllerEvent(juce::jlimit(1, 16, outChannel),
                                                              kBankLsbController,
                                                              juce::jlimit(0, 127, bankLsb)));
        messages.push_back(juce::MidiMessage::programChange(juce::jlimit(1, 16, outChannel),
                                                            juce::jlimit(0, 127, program)));
        return messages;
    }
};
