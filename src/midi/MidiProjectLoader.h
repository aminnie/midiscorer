#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <limits>
#include <vector>
#include "TempoMap.h"
#include "TrackNoteExtractor.h"

struct MidiTrackData
{
    juce::String name;
    juce::MidiMessageSequence sequence;
    std::vector<MidiNoteEvent> notes;
};

struct MidiProjectData
{
    juce::File file;
    TempoMap tempoMap;
    std::vector<MidiTrackData> tracks;
    double totalDurationSec = 0.0;
    int maxBar = 1;
    bool hasKeySignature = false;
    int keySharpsOrFlats = 0;
    bool keyIsMajor = true;

    juce::String getKeyDisplayText() const
    {
        if (!hasKeySignature)
            return "n/a";

        static constexpr const char* majorKeys[15]
            = { "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#" };
        static constexpr const char* minorKeys[15]
            = { "Abm", "Ebm", "Bbm", "Fm", "Cm", "Gm", "Dm", "Am", "Em", "Bm", "F#m", "C#m", "G#m", "D#m", "A#m" };

        const int idx = keySharpsOrFlats + 7;
        if (idx >= 0 && idx < 15)
            return keyIsMajor ? majorKeys[idx] : minorKeys[idx];

        return keySharpsOrFlats > 0
            ? juce::String(keySharpsOrFlats) + " sharps"
            : juce::String(std::abs(keySharpsOrFlats)) + " flats";
    }
};

class MidiProjectLoader
{
public:
    bool load(const juce::File& midiFile, MidiProjectData& outData, juce::String& error)
    {
        error.clear();
        outData = {};

        juce::FileInputStream stream(midiFile);
        if (!stream.openedOk())
        {
            error = "Failed to open file: " + midiFile.getFullPathName();
            return false;
        }

        juce::MidiFile parsed;
        if (!parsed.readFrom(stream))
        {
            error = "Failed to read MIDI file: " + midiFile.getFileName();
            return false;
        }

        const int timeFormat = parsed.getTimeFormat();
        if (timeFormat <= 0)
        {
            error = "Unsupported MIDI time format (SMPTE/non-PPQ). Please use a PPQ-timed MIDI file.";
            return false;
        }

        const double ppq = static_cast<double>(timeFormat);

        std::vector<TempoMetaEvent> tempos;
        std::vector<TimeSignatureMetaEvent> signatures;
        double maxTick = 0.0;
        double earliestKeyTick = std::numeric_limits<double>::max();

        for (int trackIndex = 0; trackIndex < parsed.getNumTracks(); ++trackIndex)
        {
            const auto* seq = parsed.getTrack(trackIndex);
            if (seq == nullptr)
                continue;

            for (int eventIndex = 0; eventIndex < seq->getNumEvents(); ++eventIndex)
            {
                const auto* holder = seq->getEventPointer(eventIndex);
                if (holder == nullptr)
                    continue;

                const auto& msg = holder->message;
                const auto tick = msg.getTimeStamp();
                maxTick = juce::jmax(maxTick, tick);

                if (msg.isTempoMetaEvent())
                    tempos.push_back({ tick, msg.getTempoSecondsPerQuarterNote() > 0.0 ? 60.0 / msg.getTempoSecondsPerQuarterNote() : 120.0 });
                else if (msg.isTimeSignatureMetaEvent())
                {
                    int numerator = 4;
                    int denominator = 4;
                    msg.getTimeSignatureInfo(numerator, denominator);
                    signatures.push_back({ tick, numerator, denominator });
                }
                else if (msg.isKeySignatureMetaEvent() && tick <= earliestKeyTick)
                {
                    earliestKeyTick = tick;
                    outData.hasKeySignature = true;
                    outData.keySharpsOrFlats = msg.getKeySignatureNumberOfSharpsOrFlats();
                    outData.keyIsMajor = msg.isKeySignatureMajorKey();
                }
            }
        }

        if (!outData.tempoMap.build(ppq, tempos, signatures, maxTick))
        {
            error = "Unable to build tempo map.";
            return false;
        }

        outData.file = midiFile;
        outData.tracks.reserve(static_cast<size_t>(parsed.getNumTracks()));

        for (int trackIndex = 0; trackIndex < parsed.getNumTracks(); ++trackIndex)
        {
            const auto* src = parsed.getTrack(trackIndex);
            if (src == nullptr)
                continue;

            MidiTrackData track;
            track.sequence = *src;
            track.name = readTrackName(track.sequence, trackIndex);
            track.notes = TrackNoteExtractor::extract(track.sequence, outData.tempoMap);

            for (const auto& note : track.notes)
            {
                outData.totalDurationSec = juce::jmax(outData.totalDurationSec, note.endSec);
                outData.maxBar = juce::jmax(outData.maxBar, outData.tempoMap.secondsToBar(note.endSec));
            }

            outData.tracks.push_back(std::move(track));
        }

        if (outData.tracks.empty())
        {
            error = "No readable MIDI tracks found in file.";
            return false;
        }

        return true;
    }

private:
    static juce::String readTrackName(const juce::MidiMessageSequence& sequence, int trackIndex)
    {
        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto* holder = sequence.getEventPointer(i);
            if (holder == nullptr)
                continue;

            const auto& msg = holder->message;
            if (msg.isTrackNameEvent())
                return msg.getTextFromTextMetaEvent();
        }

        return "Track " + juce::String(trackIndex + 1);
    }
};
