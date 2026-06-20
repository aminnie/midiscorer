#pragma once

class IPlaybackPositionSource
{
public:
    virtual ~IPlaybackPositionSource() = default;

    virtual bool isPlaying() const = 0;
    virtual double getElapsedSeconds() const = 0;
    virtual int getCurrentBar() const = 0;
    virtual bool hasReachedEnd() const = 0;
};
