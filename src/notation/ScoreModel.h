#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <vector>
#include "../midi/TempoMap.h"
#include "Quantizer.h"

struct ChordAnnotation
{
    int barNumber = 1;
    double quarter = 0.0;
    juce::String symbol;
};

struct ScoreNoteSymbol
{
    int barNumber = 1;
    double quarter = 0.0;
    double quarterInBar = 0.0;
    double durationQuarter = 1.0;
    int midiNote = 60;
    NoteValue value = NoteValue::quarter;
    bool isRest = false;
    bool tieIntoNextBar = false;
};

struct ScoreBar
{
    int barNumber = 1;
    int numerator = 4;
    int denominator = 4;
    std::vector<ScoreNoteSymbol> notes;
    std::vector<ChordAnnotation> chords;
};

class ScoreModel
{
public:
    void clear()
    {
        bars.clear();
        firstBar = 1;
        lastBar = 1;
    }

    void build(const TempoMap& map,
               const std::vector<QuantizedNote>& quantizedNotes,
               const std::vector<ChordAnnotation>& chordEvents,
               int maxBarHint)
    {
        clear();

        firstBar = 1;
        lastBar = juce::jmax(1, maxBarHint);
        bars.reserve(static_cast<size_t>(lastBar));

        for (int bar = firstBar; bar <= lastBar; ++bar)
        {
            ScoreBar scoreBar;
            scoreBar.barNumber = bar;
            const auto signature = signatureForBar(map, bar);
            scoreBar.numerator = signature.first;
            scoreBar.denominator = signature.second;
            bars.push_back(scoreBar);
        }

        for (const auto& n : quantizedNotes)
        {
            const int bar = map.quarterToBar(n.startQuarter);
            const int idx = barToIndex(bar);
            if (!isValidIndex(idx))
                continue;

            ScoreNoteSymbol symbol;
            symbol.barNumber = bar;
            symbol.quarter = n.startQuarter;
            symbol.quarterInBar = juce::jmax(0.0, n.startQuarter - map.barToQuarterDownbeat(bar));
            symbol.durationQuarter = n.durationQuarter;
            symbol.midiNote = n.midiNote;
            symbol.value = n.value;

            const auto endQuarter = n.startQuarter + n.durationQuarter;
            const int endBar = map.quarterToBar(endQuarter - 1.0e-6);
            symbol.tieIntoNextBar = endBar > bar;
            bars[(size_t) idx].notes.push_back(symbol);
        }

        for (const auto& chord : chordEvents)
        {
            const int idx = barToIndex(chord.barNumber);
            if (!isValidIndex(idx))
                continue;

            bars[(size_t) idx].chords.push_back(chord);
        }

        for (auto& bar : bars)
        {
            std::sort(bar.notes.begin(), bar.notes.end(), [](const auto& a, const auto& b) { return a.quarterInBar < b.quarterInBar; });
            insertRestsIntoBar(bar);
            std::sort(bar.notes.begin(), bar.notes.end(), [](const auto& a, const auto& b) { return a.quarterInBar < b.quarterInBar; });
            std::sort(bar.chords.begin(), bar.chords.end(), [](const auto& a, const auto& b) { return a.quarter < b.quarter; });
        }
    }

    std::vector<ScoreBar> getWindowBars(int centerBar, int left = 2, int right = 2) const
    {
        std::vector<ScoreBar> out;
        if (bars.empty())
            return out;

        const auto from = juce::jmax(firstBar, centerBar - left);
        const auto to = juce::jmin(lastBar, centerBar + right);
        out.reserve(static_cast<size_t>(juce::jmax(0, to - from + 1)));

        for (int b = from; b <= to; ++b)
        {
            const auto idx = barToIndex(b);
            if (isValidIndex(idx))
                out.push_back(bars[(size_t) idx]);
        }

        return out;
    }

    int getFirstBar() const { return firstBar; }
    int getLastBar() const { return lastBar; }
    bool empty() const { return bars.empty(); }

private:
    static double noteValueToQuarter(NoteValue value)
    {
        switch (value)
        {
            case NoteValue::sixteenth: return 0.25;
            case NoteValue::eighth: return 0.5;
            case NoteValue::quarter: return 1.0;
            case NoteValue::half: return 2.0;
            case NoteValue::whole: return 4.0;
        }
        return 1.0;
    }

    static NoteValue quarterToNoteValue(double durationQuarter)
    {
        if (durationQuarter <= 0.25 + 1.0e-6)
            return NoteValue::sixteenth;
        if (durationQuarter <= 0.5 + 1.0e-6)
            return NoteValue::eighth;
        if (durationQuarter <= 1.0 + 1.0e-6)
            return NoteValue::quarter;
        if (durationQuarter <= 2.0 + 1.0e-6)
            return NoteValue::half;
        return NoteValue::whole;
    }

    static void addRestGap(ScoreBar& bar, double gapStartQuarterInBar, double gapDurationQuarter)
    {
        if (gapDurationQuarter <= 1.0e-6)
            return;

        static constexpr double durations[] { 4.0, 2.0, 1.0, 0.5, 0.25 };
        double cursor = gapStartQuarterInBar;
        double remaining = gapDurationQuarter;

        while (remaining > 1.0e-6)
        {
            double chosen = 0.25;
            for (double d : durations)
            {
                if (d <= remaining + 1.0e-6)
                {
                    chosen = d;
                    break;
                }
            }

            ScoreNoteSymbol rest;
            rest.barNumber = bar.barNumber;
            rest.quarterInBar = cursor;
            rest.quarter = cursor;
            rest.durationQuarter = chosen;
            rest.value = quarterToNoteValue(chosen);
            rest.isRest = true;
            rest.tieIntoNextBar = false;
            bar.notes.push_back(rest);

            cursor += chosen;
            remaining -= chosen;
        }
    }

    static void insertRestsIntoBar(ScoreBar& bar)
    {
        const double barDurationQuarter = static_cast<double>(bar.numerator) * 4.0 / static_cast<double>(bar.denominator);
        if (barDurationQuarter <= 1.0e-6)
            return;

        std::vector<std::pair<double, double>> occupied;
        occupied.reserve(bar.notes.size());

        for (const auto& symbol : bar.notes)
        {
            if (symbol.isRest)
                continue;

            const double start = juce::jlimit(0.0, barDurationQuarter, symbol.quarterInBar);
            const double end = juce::jlimit(0.0, barDurationQuarter, start + juce::jmax(0.0, symbol.durationQuarter));
            if (end > start + 1.0e-6)
                occupied.push_back({ start, end });
        }

        if (occupied.empty())
        {
            addRestGap(bar, 0.0, barDurationQuarter);
            return;
        }

        std::sort(occupied.begin(), occupied.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        std::vector<std::pair<double, double>> merged;
        for (const auto& span : occupied)
        {
            if (merged.empty() || span.first > merged.back().second + 1.0e-6)
            {
                merged.push_back(span);
                continue;
            }

            merged.back().second = juce::jmax(merged.back().second, span.second);
        }

        double cursor = 0.0;
        for (const auto& span : merged)
        {
            if (span.first > cursor + 1.0e-6)
                addRestGap(bar, cursor, span.first - cursor);
            cursor = juce::jmax(cursor, span.second);
        }

        if (cursor < barDurationQuarter - 1.0e-6)
            addRestGap(bar, cursor, barDurationQuarter - cursor);
    }

    std::pair<int, int> signatureForBar(const TempoMap& map, int barNumber) const
    {
        const auto q = map.barToQuarterDownbeat(barNumber);
        const auto& sigs = map.getTimeSignatureEvents();
        if (sigs.empty())
            return { 4, 4 };

        int idx = 0;
        while (idx + 1 < static_cast<int>(sigs.size()) && sigs[(size_t) (idx + 1)].quarterAtTick <= q)
            ++idx;

        return { sigs[(size_t) idx].numerator, sigs[(size_t) idx].denominator };
    }

    int barToIndex(int barNumber) const
    {
        return barNumber - firstBar;
    }

    bool isValidIndex(int index) const
    {
        return index >= 0 && index < static_cast<int>(bars.size());
    }

    int firstBar = 1;
    int lastBar = 1;
    std::vector<ScoreBar> bars;
};
