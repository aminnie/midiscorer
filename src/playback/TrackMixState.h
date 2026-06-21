#pragma once

#include <vector>

struct TrackMixSettings
{
    int volume = 100;
    int reverb = 10;
    bool muted = false;
    bool solo = false;
};

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
