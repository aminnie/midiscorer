#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <map>
#include <utility>
#include <vector>
#include "TempoMap.h"

struct MidiNoteEvent
{
    int channel = 1;
    int noteNumber = 60;
    int velocity = 100;
    double startTick = 0.0;
    double endTick = 0.0;
    double startSec = 0.0;
    double endSec = 0.0;
};

class TrackNoteExtractor
{
public:
    static std::vector<MidiNoteEvent> extract(const juce::MidiMessageSequence& sequence,
                                              const TempoMap& tempoMap)
    {
        std::vector<MidiNoteEvent> notes;
        std::multimap<std::pair<int, int>, MidiNoteEvent> active;
        double lastTick = 0.0;

        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto* holder = sequence.getEventPointer(i);
            if (holder == nullptr)
                continue;

            const auto& msg = holder->message;
            const auto tick = msg.getTimeStamp();
            lastTick = juce::jmax(lastTick, tick);
            if (msg.isMetaEvent())
                continue;

            if (msg.isNoteOn())
            {
                MidiNoteEvent ev;
                ev.channel = msg.getChannel();
                ev.noteNumber = msg.getNoteNumber();
                ev.velocity = msg.getVelocity();
                ev.startTick = tick;
                ev.startSec = tempoMap.tickToSeconds(tick);
                active.emplace(std::make_pair(ev.channel, ev.noteNumber), ev);
            }
            else if (msg.isNoteOff())
            {
                const auto key = std::make_pair(msg.getChannel(), msg.getNoteNumber());
                auto range = active.equal_range(key);
                if (range.first == range.second)
                    continue;

                auto it = range.first;
                MidiNoteEvent ev = it->second;
                active.erase(it);

                ev.endTick = tick;
                ev.endSec = tempoMap.tickToSeconds(tick);
                if (ev.endTick >= ev.startTick)
                    notes.push_back(ev);
            }
        }

        // Flush orphaned note-ons at track end to avoid silently dropping sustained tails.
        for (auto& [key, ev] : active)
        {
            juce::ignoreUnused(key);
            ev.endTick = juce::jmax(ev.startTick, lastTick);
            ev.endSec = tempoMap.tickToSeconds(ev.endTick);
            if (ev.endTick >= ev.startTick)
                notes.push_back(ev);
        }

        std::sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) { return a.startTick < b.startTick; });
        return notes;
    }
};
