#pragma once

#include <JuceHeader.h>
#include <array>
#include <set>
#include <vector>
#include "../midi/TempoMap.h"
#include "../midi/TrackNoteExtractor.h"
#include "../notation/ScoreModel.h"

class ChordDetector
{
public:
    enum class DetectionResolution
    {
        quarter = 1,
        eighth = 2
    };

    enum class AccidentalPreference
    {
        preferSharps,
        preferFlats
    };

    enum class JazzAliasStyle
    {
        plain,
        jazzSymbols
    };

    enum class ChordComplexity
    {
        simple = 1,
        standard = 2,
        rich = 3
    };

    struct NamingOptions
    {
        AccidentalPreference accidentalPreference = AccidentalPreference::preferSharps;
        JazzAliasStyle jazzAliasStyle = JazzAliasStyle::plain;
        ChordComplexity complexity = ChordComplexity::rich;
    };

    static double windowQuarterLength(DetectionResolution resolution)
    {
        return resolution == DetectionResolution::eighth ? 0.5 : 1.0;
    }

    static std::vector<ChordAnnotation> detect(const std::vector<MidiNoteEvent>& notes,
                                               const TempoMap& map,
                                               int maxBar,
                                               NamingOptions options = {},
                                               DetectionResolution resolution = DetectionResolution::quarter)
    {
        // ScoreRebuildService caches one detect() result per rebuild when chord-track
        // selection is active, so this scan is not repeated for every staff lane.
        std::vector<ChordAnnotation> out;
        if (notes.empty())
            return out;

        const int barCount = juce::jmax(1, maxBar);
        const double windowQuarter = windowQuarterLength(resolution);
        juce::String previousSymbol;

        for (int bar = 1; bar <= barCount; ++bar)
        {
            previousSymbol.clear();
            const auto barQ = map.barToQuarterDownbeat(bar);
            const auto nextBarQ = map.barToQuarterDownbeat(bar + 1);
            for (double q = barQ; q < nextBarQ - 1.0e-6; q += windowQuarter)
            {
                const auto secA = map.tickToSeconds(map.quarterToTick(q));
                const auto secB = map.tickToSeconds(map.quarterToTick(q + windowQuarter));
                const auto symbol = detectWindow(notes, secA, secB, options);
                if (symbol.isEmpty())
                {
                    previousSymbol.clear();
                    continue;
                }

                if (symbol != previousSymbol)
                {
                    out.push_back({ bar, q, symbol });
                    previousSymbol = symbol;
                }
            }
        }

        return out;
    }

    static juce::String detectInWindow(const std::vector<MidiNoteEvent>& notes,
                                       double startSec,
                                       double endSec,
                                       NamingOptions options = {})
    {
        if (notes.empty() || endSec <= startSec)
            return {};

        return detectWindow(notes, startSec, endSec, options);
    }

private:
    struct ChordTemplate
    {
        juce::String suffix;
        std::vector<int> required;
        std::vector<int> optional;
    };

    static juce::String detectWindow(const std::vector<MidiNoteEvent>& notes,
                                     double startSec,
                                     double endSec,
                                     const NamingOptions& options)
    {
        std::set<int> pcs;
        int bass = -1;
        int highest = -1;

        for (const auto& n : notes)
        {
            if (n.endSec <= startSec || n.startSec >= endSec)
                continue;

            const int pc = ((n.noteNumber % 12) + 12) % 12;
            pcs.insert(pc);
            bass = bass < 0 ? n.noteNumber : juce::jmin(bass, n.noteNumber);
            highest = highest < 0 ? n.noteNumber : juce::jmax(highest, n.noteNumber);
        }

        if (pcs.size() < 3)
            return {};

        const auto bassPc = bass >= 0 ? bass % 12 : -1;

        int bestScore = std::numeric_limits<int>::min();
        juce::String best;

        for (int root = 0; root < 12; ++root)
        {
            for (const auto& tmpl : templates())
            {
                if (!templateAllowedForComplexity(tmpl.suffix, options.complexity))
                    continue;

                if (!containsRequired(pcs, root, tmpl.required))
                    continue;

                int score = 0;
                for (int pc : pcs)
                {
                    const int rel = ((pc - root) + 12) % 12;
                    if (containsInt(tmpl.required, rel))
                        score += 5;
                    else if (containsInt(tmpl.optional, rel))
                        score += 2;
                    else
                        score -= 1;
                }

                if (bassPc == root)
                    score += 2;
                if (highest >= 0 && ((highest % 12) == (root + 7) % 12))
                    score += 1;

                if (score > bestScore)
                {
                    bestScore = score;
                    best = pitchName(root, options.accidentalPreference) + formatSuffix(tmpl.suffix, options.jazzAliasStyle);
                    if (bassPc >= 0 && bassPc != root)
                        best += "/" + pitchName(bassPc, options.accidentalPreference);
                }
            }
        }

        return best;
    }

    static bool containsRequired(const std::set<int>& pcs, int root, const std::vector<int>& required)
    {
        for (int interval : required)
        {
            const int target = (root + interval) % 12;
            if (pcs.find(target) == pcs.end())
                return false;
        }
        return true;
    }

    static bool containsInt(const std::vector<int>& list, int value)
    {
        return std::find(list.begin(), list.end(), value) != list.end();
    }

    static juce::String pitchName(int pc, AccidentalPreference accidentalPreference)
    {
        static constexpr const char* sharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static constexpr const char* flatNames[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        const int idx = (pc % 12 + 12) % 12;
        return accidentalPreference == AccidentalPreference::preferFlats ? flatNames[idx] : sharpNames[idx];
    }

    static juce::String formatSuffix(const juce::String& raw, JazzAliasStyle style)
    {
        if (style == JazzAliasStyle::plain)
            return raw;

        if (raw == "maj7")
            return "^7";
        if (raw == "mMaj7")
            return "m^7";
        if (raw == "m")
            return "-";
        if (raw == "m7")
            return "-7";

        return raw;
    }

    static const std::vector<ChordTemplate>& templates()
    {
        static const std::vector<ChordTemplate> all
        {
            { "",      { 0, 4, 7 }, { 2, 9, 11 } },
            { "m",     { 0, 3, 7 }, { 2, 10 } },
            { "dim",   { 0, 3, 6 }, { 9 } },
            { "aug",   { 0, 4, 8 }, { 10 } },
            { "sus2",  { 0, 2, 7 }, { 10 } },
            { "sus4",  { 0, 5, 7 }, { 10 } },
            { "6",     { 0, 4, 7, 9 }, { 2 } },
            { "m6",    { 0, 3, 7, 9 }, { 2 } },
            { "7",     { 0, 4, 7, 10 }, { 2, 5, 9 } },
            { "maj7",  { 0, 4, 7, 11 }, { 2, 9 } },
            { "m7",    { 0, 3, 7, 10 }, { 2, 5 } },
            { "mMaj7", { 0, 3, 7, 11 }, { 2 } },
            { "7b5",   { 0, 4, 6, 10 }, { 2 } },
            { "7#5",   { 0, 4, 8, 10 }, { 2 } },
            { "9",     { 0, 4, 7, 10, 2 }, { 5, 9 } },
            { "m9",    { 0, 3, 7, 10, 2 }, { 5 } },
            { "11",    { 0, 4, 7, 10, 5 }, { 2 } },
            { "13",    { 0, 4, 7, 10, 9 }, { 2, 5 } },
            { "7b9",   { 0, 4, 7, 10, 1 }, { 8 } },
            { "7#9",   { 0, 4, 7, 10, 3 }, { 8 } },
            { "add9",  { 0, 4, 7, 2 }, { 9, 11 } }
        };
        return all;
    }

    static bool templateAllowedForComplexity(const juce::String& suffix, ChordComplexity complexity)
    {
        if (complexity == ChordComplexity::rich)
            return true;

        if (complexity == ChordComplexity::simple)
        {
            return suffix.isEmpty()
                || suffix == "m"
                || suffix == "dim"
                || suffix == "aug"
                || suffix == "sus2"
                || suffix == "sus4";
        }

        return suffix != "7b5"
            && suffix != "7#5"
            && suffix != "7b9"
            && suffix != "7#9";
    }
};
