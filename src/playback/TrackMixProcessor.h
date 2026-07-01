#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "TrackMixState.h"

class TrackMixProcessor
{
public:
    static constexpr int kReverbController = 91;
    static constexpr int kExpressionController = 11;

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
            auto transformed = juce::MidiMessage::programChange(outChannel, message.getProgramChangeNumber());
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
};
