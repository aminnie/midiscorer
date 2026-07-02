#pragma once

#include <JuceHeader.h>

struct TrackSoundProgram
{
    int bankMsb = 0;
    int bankLsb = 0;
    int program = 0;
    juce::String voiceName;
    bool configured = false;
};

inline juce::String buildTrackSoundDisplayName(const TrackSoundProgram& sound)
{
    if (!sound.configured)
        return "Use MIDI File";

    const auto name = sound.voiceName.trim();
    const auto fallback = "Voice " + juce::String(sound.program + 1);
    return "SM: " + (name.isNotEmpty() ? name : fallback);
}

inline juce::String buildTrackSoundCcHelpText(const TrackSoundProgram& sound)
{
    if (!sound.configured)
        return "No sound-module override is active for this track.\n\n"
               "Playback uses the MIDI file's original bank/program events.";

    const auto name = sound.voiceName.trim();
    const auto label = name.isNotEmpty() ? name : ("Voice " + juce::String(sound.program + 1));
    return "Sound Module Override\n\n"
           "Name: " + label + "\n"
           "MSB CC0: " + juce::String(sound.bankMsb) + "\n"
           "LSB CC32: " + juce::String(sound.bankLsb) + "\n"
           "Voice (Program Change): " + juce::String(sound.program + 1) + "\n\n"
           "These values replace incoming MIDI CC0/CC32/Program events for this track.";
}
