#pragma once

#include <JuceHeader.h>
#include <vector>

struct TrackMixSettings
{
    int volume = 100;
    int expression = 100;
    int reverb = 10;
    int channel = 1;
    int bankMsb = 0;
    int bankLsb = 0;
    int program = 0;
    juce::String soundName;
    bool soundConfigured = false;
    bool muted = false;
    bool solo = false;
};

inline bool operator==(const TrackMixSettings& lhs, const TrackMixSettings& rhs)
{
    return lhs.volume == rhs.volume
        && lhs.expression == rhs.expression
        && lhs.reverb == rhs.reverb
        && lhs.channel == rhs.channel
        && lhs.bankMsb == rhs.bankMsb
        && lhs.bankLsb == rhs.bankLsb
        && lhs.program == rhs.program
        && lhs.soundName == rhs.soundName
        && lhs.soundConfigured == rhs.soundConfigured
        && lhs.muted == rhs.muted
        && lhs.solo == rhs.solo;
}

class TrackMixState
{
public:
    void resizeForTrackCount(int trackCount)
    {
        if (trackCount < 0)
            trackCount = 0;
        tracks.assign((size_t) trackCount, {});
    }

    int getTrackCount() const
    {
        return static_cast<int>(tracks.size());
    }

    bool isValidTrack(int trackIndex) const
    {
        return trackIndex >= 0 && trackIndex < getTrackCount();
    }

    bool anySoloActive() const
    {
        for (const auto& track : tracks)
        {
            if (track.solo)
                return true;
        }
        return false;
    }

    int getVolume(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return 100;
        return tracks[(size_t) trackIndex].volume;
    }

    int getReverb(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return 10;
        return tracks[(size_t) trackIndex].reverb;
    }

    int getExpression(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return 100;
        return tracks[(size_t) trackIndex].expression;
    }

    int getChannel(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return 1;
        return tracks[(size_t) trackIndex].channel;
    }

    int getBankMsb(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return 0;
        return tracks[(size_t) trackIndex].bankMsb;
    }

    int getBankLsb(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return 0;
        return tracks[(size_t) trackIndex].bankLsb;
    }

    int getProgram(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return 0;
        return tracks[(size_t) trackIndex].program;
    }

    juce::String getSoundName(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return {};
        return tracks[(size_t) trackIndex].soundName;
    }

    bool isSoundConfigured(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return false;
        return tracks[(size_t) trackIndex].soundConfigured;
    }

    bool isMuted(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return false;
        return tracks[(size_t) trackIndex].muted;
    }

    bool isSolo(int trackIndex) const
    {
        if (!isValidTrack(trackIndex))
            return false;
        return tracks[(size_t) trackIndex].solo;
    }

    void setVolume(int trackIndex, int volume)
    {
        if (!isValidTrack(trackIndex))
            return;
        if (volume < 0)
            volume = 0;
        if (volume > 127)
            volume = 127;
        tracks[(size_t) trackIndex].volume = volume;
    }

    void setReverb(int trackIndex, int reverb)
    {
        if (!isValidTrack(trackIndex))
            return;
        if (reverb < 0)
            reverb = 0;
        if (reverb > 127)
            reverb = 127;
        tracks[(size_t) trackIndex].reverb = reverb;
    }

    void setExpression(int trackIndex, int expression)
    {
        if (!isValidTrack(trackIndex))
            return;
        if (expression < 0)
            expression = 0;
        if (expression > 127)
            expression = 127;
        tracks[(size_t) trackIndex].expression = expression;
    }

    void setChannel(int trackIndex, int channel)
    {
        if (!isValidTrack(trackIndex))
            return;
        tracks[(size_t) trackIndex].channel = juce::jlimit(1, 16, channel);
    }

    void setBankMsb(int trackIndex, int bankMsb)
    {
        if (!isValidTrack(trackIndex))
            return;
        tracks[(size_t) trackIndex].bankMsb = juce::jlimit(0, 127, bankMsb);
    }

    void setBankLsb(int trackIndex, int bankLsb)
    {
        if (!isValidTrack(trackIndex))
            return;
        tracks[(size_t) trackIndex].bankLsb = juce::jlimit(0, 127, bankLsb);
    }

    void setProgram(int trackIndex, int program)
    {
        if (!isValidTrack(trackIndex))
            return;
        tracks[(size_t) trackIndex].program = juce::jlimit(0, 127, program);
    }

    void setSoundName(int trackIndex, const juce::String& soundName)
    {
        if (!isValidTrack(trackIndex))
            return;
        tracks[(size_t) trackIndex].soundName = soundName.trim();
    }

    void setSoundConfigured(int trackIndex, bool soundConfigured)
    {
        if (!isValidTrack(trackIndex))
            return;
        tracks[(size_t) trackIndex].soundConfigured = soundConfigured;
    }

    void setMuted(int trackIndex, bool muted)
    {
        if (!isValidTrack(trackIndex))
            return;
        tracks[(size_t) trackIndex].muted = muted;
    }

    void setSolo(int trackIndex, bool solo)
    {
        if (!isValidTrack(trackIndex))
            return;

        if (solo)
        {
            for (auto& track : tracks)
                track.solo = false;
        }

        tracks[(size_t) trackIndex].solo = solo;
    }

private:
    std::vector<TrackMixSettings> tracks;
};
