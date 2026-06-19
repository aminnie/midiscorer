#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include "ScoreModel.h"

class ScoreRenderer final : public juce::Component
{
public:
    enum class ClefType
    {
        treble,
        bass
    };

    enum class ColorScheme
    {
        dark,
        light
    };

    void setScoreModel(const ScoreModel* modelPtr)
    {
        model = modelPtr;
        repaint();
    }

    void setCurrentBar(int bar)
    {
        currentBar = juce::jmax(1, bar);
        repaint();
    }

    void setClefType(ClefType type)
    {
        clefType = type;
        repaint();
    }

    void setColorScheme(ColorScheme scheme)
    {
        colorScheme = scheme;
        repaint();
    }

    void setKeySignature(bool hasSignature, int sharpsOrFlats)
    {
        hasKeySignature = hasSignature;
        keySharpsOrFlats = juce::jlimit(-7, 7, sharpsOrFlats);
        repaint();
    }

    void setLiveChordMarker(bool visible, int barNumber, double quarterInBar, const juce::String& chordText)
    {
        liveChordVisible = visible;
        liveChordBarNumber = juce::jmax(1, barNumber);
        liveChordQuarterInBar = juce::jmax(0.0, quarterInBar);
        liveChordText = chordText;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const bool isDark = colorScheme == ColorScheme::dark;
        const auto background = isDark ? juce::Colour(0xff14161b) : juce::Colour(0xfff7f7f7);
        const auto frame = isDark ? juce::Colours::white.withAlpha(0.15f) : juce::Colours::black.withAlpha(0.20f);
        const auto emptyText = isDark ? juce::Colours::lightgrey : juce::Colours::darkgrey;

        g.fillAll(background);
        g.setColour(frame);
        g.drawRect(getLocalBounds());

        if (model == nullptr || model->empty())
        {
            g.setColour(emptyText);
            g.drawFittedText("Load a MIDI file and select a track to render notation.",
                             getLocalBounds().reduced(16), juce::Justification::centred, 2);
            return;
        }

        const auto bars = model->getWindowBars(currentBar, 2, 2);
        if (bars.empty())
            return;

        auto bounds = getLocalBounds().reduced(12);
        const int barW = bounds.getWidth() / static_cast<int>(bars.size());
        int x = bounds.getX();

        for (size_t i = 0; i < bars.size(); ++i)
        {
            const auto& bar = bars[i];
            juce::Rectangle<int> barRect(x, bounds.getY(), barW, bounds.getHeight());
            drawBar(g, bar, barRect, bar.barNumber == currentBar, i == 0);
            x += barW;
        }
    }

private:
    static bool isBlackKeyPitchClass(int pitchClass)
    {
        const int pc = (pitchClass % 12 + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    juce::String accidentalTextForMidi(int midiNote) const
    {
        const int pc = (midiNote % 12 + 12) % 12;
        if (!isBlackKeyPitchClass(pc))
            return {};

        const bool preferFlats = hasKeySignature && keySharpsOrFlats < 0;
        return preferFlats ? "b" : "#";
    }

    static void drawLedgerLines(juce::Graphics& g, const juce::Rectangle<int>& staffRect, float x, float y)
    {
        const float topStaff = static_cast<float>(staffRect.getCentreY() - 22);
        const float bottomStaff = static_cast<float>(staffRect.getCentreY() + 22);
        const float ledgerW = 15.0f;

        if (y < topStaff)
        {
            for (float ly = topStaff - 11.0f; ly >= y - 1.0f; ly -= 11.0f)
                g.drawLine(x - ledgerW * 0.5f, ly, x + ledgerW * 0.5f, ly, 1.0f);
        }
        else if (y > bottomStaff)
        {
            for (float ly = bottomStaff + 11.0f; ly <= y + 1.0f; ly += 11.0f)
                g.drawLine(x - ledgerW * 0.5f, ly, x + ledgerW * 0.5f, ly, 1.0f);
        }
    }

    static float staffYForMidi(int midiNote, const juce::Rectangle<int>& area, ClefType clefType)
    {
        const int referenceMidi = clefType == ClefType::bass ? 50 : 71;
        const float center = static_cast<float>(area.getCentreY());
        const float step = 4.0f;
        return center - (static_cast<float>(midiNote) - static_cast<float>(referenceMidi)) * step * 0.5f;
    }

    static float durationWidth(NoteValue value)
    {
        switch (value)
        {
            case NoteValue::sixteenth: return 8.0f;
            case NoteValue::eighth: return 10.0f;
            case NoteValue::quarter: return 12.0f;
            case NoteValue::half: return 16.0f;
            case NoteValue::whole: return 18.0f;
        }
        return 12.0f;
    }

    static void drawRestSymbol(juce::Graphics& g, float x, float centerY, NoteValue value)
    {
        switch (value)
        {
            case NoteValue::whole:
                g.fillRect(juce::Rectangle<float>(x - 6.0f, centerY - 7.0f, 12.0f, 3.0f));
                break;
            case NoteValue::half:
                g.fillRect(juce::Rectangle<float>(x - 6.0f, centerY - 2.0f, 12.0f, 3.0f));
                break;
            case NoteValue::quarter:
            {
                juce::Path p;
                p.startNewSubPath(x - 2.0f, centerY - 10.0f);
                p.lineTo(x + 3.0f, centerY - 2.0f);
                p.lineTo(x - 1.0f, centerY + 4.0f);
                p.lineTo(x + 3.0f, centerY + 10.0f);
                g.strokePath(p, juce::PathStrokeType(1.6f));
                break;
            }
            case NoteValue::eighth:
            {
                juce::Path stem;
                stem.startNewSubPath(x, centerY - 8.0f);
                stem.lineTo(x, centerY + 8.0f);
                stem.addArc(x - 1.0f, centerY + 2.0f, 8.0f, 7.0f, 3.3f, 5.6f, true);
                g.strokePath(stem, juce::PathStrokeType(1.4f));
                break;
            }
            case NoteValue::sixteenth:
            {
                juce::Path stem;
                stem.startNewSubPath(x, centerY - 8.0f);
                stem.lineTo(x, centerY + 8.0f);
                stem.addArc(x - 1.0f, centerY + 1.0f, 8.0f, 6.0f, 3.3f, 5.6f, true);
                stem.addArc(x - 1.0f, centerY + 6.0f, 8.0f, 6.0f, 3.3f, 5.6f, true);
                g.strokePath(stem, juce::PathStrokeType(1.4f));
                break;
            }
        }
    }

    static int keySignatureReserveWidth(bool hasSignature, int sharpsOrFlats)
    {
        if (!hasSignature || sharpsOrFlats == 0)
            return 0;

        const int symbolCount = juce::jlimit(0, 7, std::abs(sharpsOrFlats));
        return 8 + symbolCount * 9;
    }

    void drawKeySignature(juce::Graphics& g, const juce::Rectangle<int>& staffRect, int reserveWidth, juce::Colour colour) const
    {
        if (!hasKeySignature || keySharpsOrFlats == 0 || reserveWidth <= 0)
            return;

        const bool isSharp = keySharpsOrFlats > 0;
        const int symbolCount = juce::jlimit(0, 7, std::abs(keySharpsOrFlats));
        const int lineSpacing = 11;
        const int halfStep = lineSpacing / 2;
        const int centerY = staffRect.getCentreY();

        // Positions in half-staff steps from center for standard key signature order.
        static constexpr std::array<int, 7> trebleSharpOffsets = { 4, 1, 5, 2, -1, 3, 0 };
        static constexpr std::array<int, 7> bassSharpOffsets = { 2, -1, 3, 0, -3, 1, -2 };
        static constexpr std::array<int, 7> trebleFlatOffsets = { 0, 3, -1, 2, -2, 1, -3 };
        static constexpr std::array<int, 7> bassFlatOffsets = { -2, 1, -3, 0, -4, -1, -5 };

        const auto& offsets = isSharp
            ? (clefType == ClefType::bass ? bassSharpOffsets : trebleSharpOffsets)
            : (clefType == ClefType::bass ? bassFlatOffsets : trebleFlatOffsets);
        const auto symbol = isSharp ? "#" : "b";

        g.setColour(colour);
        g.setFont(13.0f);
        int x = staffRect.getX() + 10;
        const int maxX = staffRect.getX() + reserveWidth;
        for (int i = 0; i < symbolCount; ++i)
        {
            const int y = centerY - offsets[(size_t) i] * halfStep - 8;
            g.drawText(symbol, juce::Rectangle<int>(x, y, 10, 16), juce::Justification::centred);
            x += 9;
            if (x > maxX)
                break;
        }
    }

    void drawBar(juce::Graphics& g,
                 const ScoreBar& bar,
                 const juce::Rectangle<int>& barRect,
                 bool isCurrent,
                 bool isFirstVisibleBar) const
    {
        const bool isDark = colorScheme == ColorScheme::dark;
        const auto barFill = isDark
            ? (isCurrent ? juce::Colour(0xff233a59) : juce::Colour(0xff1a1f28))
            : (isCurrent ? juce::Colour(0xffdbe9ff) : juce::Colour(0xffececec));
        const auto barBorder = isDark ? juce::Colours::white.withAlpha(isCurrent ? 0.5f : 0.22f)
                                      : juce::Colours::black.withAlpha(isCurrent ? 0.40f : 0.25f);
        const auto labelColour = isDark ? juce::Colours::lightgrey.withAlpha(0.9f)
                                        : juce::Colours::black.withAlpha(0.8f);
        const auto chordColour = isDark ? juce::Colours::yellow.withAlpha(0.95f)
                                        : juce::Colour(0xff3d2c00);
        const auto staffLineColour = isDark ? juce::Colours::whitesmoke.withAlpha(0.7f)
                                            : juce::Colours::black.withAlpha(0.45f);
        const auto noteColour = isDark ? juce::Colours::white : juce::Colours::black;
        const auto beatGuideColour = isDark ? juce::Colours::white.withAlpha(0.10f)
                                            : juce::Colours::black.withAlpha(0.08f);
        const auto tieColour = isDark ? juce::Colours::lightblue.withAlpha(0.9f)
                                      : juce::Colour(0xff1f4e8a).withAlpha(0.75f);

        g.setColour(barFill);
        g.fillRect(barRect.reduced(1));

        g.setColour(barBorder);
        g.drawRect(barRect);

        const int chordTop = barRect.getY() + 22;
        const int staffTop = barRect.getY() + 44;
        const int staffHeight = barRect.getHeight() - 58;
        juce::Rectangle<int> staffRect(barRect.getX() + 6, staffTop, barRect.getWidth() - 12, staffHeight);
        const int keySigWidth = isFirstVisibleBar ? keySignatureReserveWidth(hasKeySignature, keySharpsOrFlats) : 0;

        auto barHeader = barRect;
        barHeader = barHeader.removeFromTop(18);
        barHeader = barHeader.reduced(8, 0);
        g.setColour(labelColour);
        g.setFont(13.0f);
        auto barTextArea = barHeader.removeFromLeft(100);
        auto clefTextArea = barHeader.removeFromRight(84);
        clefTextArea = clefTextArea.withTrimmedRight(6);
        g.drawText("Bar " + juce::String(bar.barNumber), barTextArea, juce::Justification::centredLeft);
        g.drawText(clefType == ClefType::bass ? "Bass" : "Treble", clefTextArea, juce::Justification::centredRight);

        const auto chordText = bar.chords.empty() ? "-" : bar.chords.front().symbol;
        g.setColour(chordColour);
        g.setFont(14.0f);
        g.drawText(chordText,
                   juce::Rectangle<int>(staffRect.getX() + 6, chordTop, staffRect.getWidth() - 12, 20),
                   juce::Justification::centredLeft);

        g.setColour(staffLineColour);
        const int lineSpacing = 11;
        for (int i = 0; i < 5; ++i)
        {
            const int y = staffRect.getCentreY() - 2 * lineSpacing + i * lineSpacing;
            g.drawLine((float) staffRect.getX(), (float) y, (float) staffRect.getRight(), (float) y, 1.0f);
        }

        if (keySigWidth > 0)
            drawKeySignature(g, staffRect, keySigWidth, noteColour);

        const double qPerBar = static_cast<double>(bar.numerator) * 4.0 / static_cast<double>(bar.denominator);
        const float left = static_cast<float>(staffRect.getX() + 6 + keySigWidth);
        const float width = static_cast<float>(juce::jmax(20, staffRect.getWidth() - 12 - keySigWidth));
        const int beatCount = juce::jmax(1, bar.numerator);

        if (liveChordVisible && bar.barNumber == liveChordBarNumber && liveChordText.isNotEmpty())
        {
            const double clampedQuarter = juce::jlimit(0.0, juce::jmax(0.0, qPerBar), liveChordQuarterInBar);
            const float markerX = left + static_cast<float>((clampedQuarter / juce::jmax(0.25, qPerBar)) * width);
            const int textWidth = 84;
            const int textX = juce::jlimit(staffRect.getX(), staffRect.getRight() - textWidth, static_cast<int>(markerX) - textWidth / 2);
            const int textY = chordTop + 18;
            g.setColour(colorScheme == ColorScheme::dark ? juce::Colours::lightgreen.withAlpha(0.95f)
                                                         : juce::Colour(0xff1f5c1f).withAlpha(0.95f));
            g.setFont(13.0f);
            g.drawText(liveChordText, juce::Rectangle<int>(textX, textY, textWidth, 16), juce::Justification::centred);
        }

        g.setColour(beatGuideColour);
        for (int beat = 1; beat < beatCount; ++beat)
        {
            const float beatX = left + (static_cast<float>(beat) / static_cast<float>(beatCount)) * width;
            g.drawLine(beatX, static_cast<float>(staffRect.getY()), beatX, static_cast<float>(staffRect.getBottom()), 1.0f);
        }

        for (const auto& note : bar.notes)
        {
            const double beatPos = juce::jmax(0.0, note.quarterInBar);
            const float x = left + static_cast<float>((beatPos / juce::jmax(0.25, qPerBar)) * width);
            const float restCenterY = static_cast<float>(staffRect.getCentreY());

            if (note.isRest)
            {
                g.setColour(noteColour);
                drawRestSymbol(g, x + 4.0f, restCenterY, note.value);
                continue;
            }

            const float y = staffYForMidi(note.midiNote, staffRect, clefType);
            const float w = durationWidth(note.value);
            const float h = 7.0f;

            g.setColour(noteColour);
            drawLedgerLines(g, staffRect, x + w * 0.5f, y);
            g.fillEllipse(x, y - h * 0.5f, w, h);

            if (note.value != NoteValue::whole)
            {
                const float stemX = x + w;
                g.drawLine(stemX, y, stemX, y - 24.0f, 1.2f);
            }

            if (note.value == NoteValue::eighth || note.value == NoteValue::sixteenth)
                g.drawLine(x + w, y - 24.0f, x + w + 6.0f, y - 20.0f, 1.1f);
            if (note.value == NoteValue::sixteenth)
                g.drawLine(x + w, y - 18.0f, x + w + 6.0f, y - 14.0f, 1.1f);

            const auto accidentalText = accidentalTextForMidi(note.midiNote);
            if (accidentalText.isNotEmpty())
            {
                g.setFont(12.0f);
                g.drawText(accidentalText,
                           juce::Rectangle<int>(static_cast<int>(x - 10.0f), static_cast<int>(y - 8.0f), 8, 16),
                           juce::Justification::centred);
                g.setColour(noteColour);
            }

            if (note.tieIntoNextBar)
            {
                g.setColour(tieColour);
                juce::Path tiePath;
                tiePath.addArc(x + w - 2.0f, y + 5.0f, 16.0f, 8.0f, 0.2f, 2.9f, true);
                g.strokePath(tiePath, juce::PathStrokeType(1.0f));
            }
        }

        for (size_t i = 1; i < bar.notes.size(); ++i)
        {
            const auto& leftNote = bar.notes[i - 1];
            const auto& rightNote = bar.notes[i];
            if (leftNote.isRest || rightNote.isRest)
                continue;
            const bool beamedLeft = leftNote.value == NoteValue::eighth || leftNote.value == NoteValue::sixteenth;
            const bool beamedRight = rightNote.value == NoteValue::eighth || rightNote.value == NoteValue::sixteenth;
            if (!beamedLeft || !beamedRight)
                continue;

            if ((rightNote.quarterInBar - leftNote.quarterInBar) > 0.75)
                continue;

            const float xA = left + static_cast<float>((leftNote.quarterInBar / juce::jmax(0.25, qPerBar)) * width)
                + durationWidth(leftNote.value);
            const float xB = left + static_cast<float>((rightNote.quarterInBar / juce::jmax(0.25, qPerBar)) * width)
                + durationWidth(rightNote.value);
            const float yA = staffYForMidi(leftNote.midiNote, staffRect, clefType) - 24.0f;
            const float yB = staffYForMidi(rightNote.midiNote, staffRect, clefType) - 24.0f;

            g.setColour(noteColour);
            g.drawLine(xA, yA, xB, yB, 2.0f);
        }
    }

    const ScoreModel* model = nullptr;
    int currentBar = 1;
    ClefType clefType = ClefType::treble;
    ColorScheme colorScheme = ColorScheme::dark;
    bool hasKeySignature = false;
    int keySharpsOrFlats = 0;
    bool liveChordVisible = false;
    int liveChordBarNumber = 1;
    double liveChordQuarterInBar = 0.0;
    juce::String liveChordText;
};
