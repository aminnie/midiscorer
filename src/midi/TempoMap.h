#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <vector>

struct TempoMetaEvent
{
    double tick = 0.0;
    double bpm = 120.0;
};

struct TimeSignatureMetaEvent
{
    double tick = 0.0;
    int numerator = 4;
    int denominator = 4;
};

class TempoMap
{
public:
    struct TempoEvent
    {
        double tick = 0.0;
        double bpm = 120.0;
        double timeSec = 0.0;
        double quarterAtTick = 0.0;
    };

    struct TimeSignatureEvent
    {
        double tick = 0.0;
        int numerator = 4;
        int denominator = 4;
        double quarterAtTick = 0.0;
        int barAtStart = 1;
        double beatsIntoBarAtStart = 0.0;
    };

    bool build(double ppqValue,
               std::vector<TempoMetaEvent> tempos,
               std::vector<TimeSignatureMetaEvent> signatures,
               double endTick)
    {
        if (ppqValue <= 0.0)
            return false;

        ppq = ppqValue;
        totalTicks = juce::jmax(0.0, endTick);

        if (tempos.empty() || tempos.front().tick > 0.0)
            tempos.insert(tempos.begin(), TempoMetaEvent {});
        if (signatures.empty() || signatures.front().tick > 0.0)
            signatures.insert(signatures.begin(), TimeSignatureMetaEvent {});

        std::sort(tempos.begin(), tempos.end(), [](const auto& a, const auto& b) { return a.tick < b.tick; });
        std::sort(signatures.begin(), signatures.end(), [](const auto& a, const auto& b) { return a.tick < b.tick; });

        tempoEvents.clear();
        timeSignatureEvents.clear();

        for (const auto& t : tempos)
        {
            if (!tempoEvents.empty() && std::abs(tempoEvents.back().tick - t.tick) < 1.0e-6)
            {
                tempoEvents.back().bpm = juce::jmax(1.0e-6, t.bpm);
                continue;
            }

            tempoEvents.push_back({ t.tick, juce::jmax(1.0e-6, t.bpm), 0.0, 0.0 });
        }

        tempoEvents.front().tick = 0.0;
        tempoEvents.front().timeSec = 0.0;
        tempoEvents.front().quarterAtTick = 0.0;

        for (size_t i = 1; i < tempoEvents.size(); ++i)
        {
            const auto& prev = tempoEvents[i - 1];
            auto& cur = tempoEvents[i];
            const auto dtick = juce::jmax(0.0, cur.tick - prev.tick);
            const auto dquarter = dtick / ppq;
            cur.quarterAtTick = prev.quarterAtTick + dquarter;
            cur.timeSec = prev.timeSec + dquarter * 60.0 / juce::jmax(1.0e-6, prev.bpm);
        }

        for (const auto& s : signatures)
        {
            const int denom = s.denominator <= 0 ? 4 : s.denominator;
            const int numer = s.numerator <= 0 ? 4 : s.numerator;

            if (!timeSignatureEvents.empty() && std::abs(timeSignatureEvents.back().tick - s.tick) < 1.0e-6)
            {
                timeSignatureEvents.back().numerator = numer;
                timeSignatureEvents.back().denominator = denom;
                continue;
            }

            timeSignatureEvents.push_back({ s.tick, numer, denom, 0.0, 1, 0.0 });
        }

        timeSignatureEvents.front().tick = 0.0;
        timeSignatureEvents.front().quarterAtTick = 0.0;
        timeSignatureEvents.front().barAtStart = 1;
        timeSignatureEvents.front().beatsIntoBarAtStart = 0.0;

        for (size_t i = 1; i < timeSignatureEvents.size(); ++i)
        {
            auto& cur = timeSignatureEvents[i];
            const auto& prev = timeSignatureEvents[i - 1];

            cur.quarterAtTick = tickToQuarter(cur.tick);
            const double deltaQuarter = juce::jmax(0.0, cur.quarterAtTick - prev.quarterAtTick);
            const double quartersPerBar = static_cast<double>(prev.numerator) * 4.0 / static_cast<double>(prev.denominator);
            const double totalQuarterIntoBar = prev.beatsIntoBarAtStart + deltaQuarter;
            const int barsAdvanced = static_cast<int>(std::floor((totalQuarterIntoBar + 1.0e-9) / quartersPerBar));

            cur.barAtStart = juce::jmax(1, prev.barAtStart + barsAdvanced);
            cur.beatsIntoBarAtStart = totalQuarterIntoBar - static_cast<double>(barsAdvanced) * quartersPerBar;
        }

        return true;
    }

    double getPPQ() const { return ppq; }
    double getTotalTicks() const { return totalTicks; }

    double tickToQuarter(double tick) const
    {
        return juce::jmax(0.0, tick / juce::jmax(1.0e-6, ppq));
    }

    double quarterToTick(double quarter) const
    {
        return juce::jmax(0.0, quarter * ppq);
    }

    double tickToSeconds(double tick) const
    {
        if (tempoEvents.empty())
            return 0.0;

        const auto queryTick = juce::jmax(0.0, tick);
        int idx = 0;
        while (idx + 1 < static_cast<int>(tempoEvents.size()) && tempoEvents[(size_t) (idx + 1)].tick <= queryTick)
            ++idx;

        const auto& ev = tempoEvents[(size_t) idx];
        const auto dquarter = juce::jmax(0.0, (queryTick - ev.tick) / ppq);
        return ev.timeSec + dquarter * 60.0 / juce::jmax(1.0e-6, ev.bpm);
    }

    double secondsToQuarter(double sec) const
    {
        if (tempoEvents.empty())
            return 0.0;

        const auto querySec = juce::jmax(0.0, sec);
        int idx = 0;
        while (idx + 1 < static_cast<int>(tempoEvents.size()) && tempoEvents[(size_t) (idx + 1)].timeSec <= querySec)
            ++idx;

        const auto& ev = tempoEvents[(size_t) idx];
        const auto dsec = juce::jmax(0.0, querySec - ev.timeSec);
        return ev.quarterAtTick + dsec * juce::jmax(1.0e-6, ev.bpm) / 60.0;
    }

    int quarterToBar(double quarter) const
    {
        if (timeSignatureEvents.empty())
            return 1;

        const auto q = juce::jmax(0.0, quarter);
        int idx = 0;
        while (idx + 1 < static_cast<int>(timeSignatureEvents.size())
               && timeSignatureEvents[(size_t) (idx + 1)].quarterAtTick <= q)
        {
            ++idx;
        }

        const auto& sig = timeSignatureEvents[(size_t) idx];
        const double deltaQuarter = juce::jmax(0.0, q - sig.quarterAtTick);
        const double quartersPerBar = static_cast<double>(sig.numerator) * 4.0 / static_cast<double>(sig.denominator);
        const double totalQuarterIntoBar = sig.beatsIntoBarAtStart + deltaQuarter;
        const int barsAdvanced = static_cast<int>(std::floor((totalQuarterIntoBar + 1.0e-9) / quartersPerBar));
        return juce::jmax(1, sig.barAtStart + barsAdvanced);
    }

    int secondsToBar(double sec) const
    {
        return quarterToBar(secondsToQuarter(sec));
    }

    double barToQuarterDownbeat(int barNumber) const
    {
        if (timeSignatureEvents.empty())
            return 0.0;

        const int target = juce::jmax(1, barNumber);
        int idx = 0;
        while (idx + 1 < static_cast<int>(timeSignatureEvents.size())
               && timeSignatureEvents[(size_t) (idx + 1)].barAtStart <= target)
        {
            ++idx;
        }

        const auto& sig = timeSignatureEvents[(size_t) idx];
        const auto barsFromSig = juce::jmax(0, target - sig.barAtStart);
        const double quartersPerBar = static_cast<double>(sig.numerator) * 4.0 / static_cast<double>(sig.denominator);
        const double deltaQuarter = static_cast<double>(barsFromSig) * quartersPerBar;
        return sig.quarterAtTick + deltaQuarter;
    }

    double barToSecondsDownbeat(int barNumber) const
    {
        return tickToSeconds(quarterToTick(barToQuarterDownbeat(barNumber)));
    }

    const std::vector<TempoEvent>& getTempoEvents() const { return tempoEvents; }
    const std::vector<TimeSignatureEvent>& getTimeSignatureEvents() const { return timeSignatureEvents; }

    bool hasMultipleTempoEvents() const
    {
        if (tempoEvents.size() <= 1)
            return false;

        const double referenceBpm = tempoEvents.front().bpm;
        for (size_t i = 1; i < tempoEvents.size(); ++i)
        {
            if (std::abs(tempoEvents[i].bpm - referenceBpm) > 1.0e-6)
                return true;
        }
        return false;
    }

private:
    double ppq = 960.0;
    double totalTicks = 0.0;
    std::vector<TempoEvent> tempoEvents;
    std::vector<TimeSignatureEvent> timeSignatureEvents;
};
