#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <functional>
#include "ScoreModel.h"

class ScoreRenderer final : public juce::Component
{
public:
    enum class ClefType
    {
        treble,
        bass,
        drum
    };

    enum class ColorScheme
    {
        dark,
        light
    };

    enum class DisplayOctaveShift
    {
        downOne = -1,
        normal = 0,
        upOne = 1
    };

    struct BarStripPaintOptions
    {
        bool highlightCurrentBar = false;
        bool includeLiveChordMarker = false;
        int currentBarForHighlight = 1;
    };

    void setScoreModel(const ScoreModel* modelPtr)
    {
        model = modelPtr;
        repaint();
    }

    void setCurrentBar(int bar)
    {
        if (model != nullptr && !model->empty())
            currentBar = juce::jlimit(model->getFirstBar(), model->getLastBar(), bar);
        else
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

    ColorScheme getColorScheme() const
    {
        return colorScheme;
    }

    void setDisplayOctaveShift(DisplayOctaveShift shift)
    {
        displayOctaveShift = shift;
        repaint();
    }

    DisplayOctaveShift getDisplayOctaveShift() const
    {
        return displayOctaveShift;
    }

    static int applyDisplayOctaveShiftForView(int rawMidiNote, int octaveShiftSteps, bool isDrumClef)
    {
        if (isDrumClef || octaveShiftSteps == 0)
            return juce::jlimit(0, 127, rawMidiNote);

        return juce::jlimit(0, 127, rawMidiNote + octaveShiftSteps * 12);
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

    void setStaffLabel(const juce::String& label)
    {
        staffLabel = label;
        repaint();
    }

    void setBarClickCallback(std::function<void(int)> callback)
    {
        onBarClicked = std::move(callback);
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

        const int clampedCenterBar = juce::jlimit(model->getFirstBar(), model->getLastBar(), currentBar);
        const auto bars = getCurrentWindowBars();
        if (bars.empty())
        {
            g.setColour(emptyText);
            g.drawFittedText("No visible bars in current score window.",
                             getLocalBounds().reduced(16), juce::Justification::centred, 2);
            return;
        }

        BarStripPaintOptions options;
        options.highlightCurrentBar = true;
        options.includeLiveChordMarker = true;
        options.currentBarForHighlight = clampedCenterBar;
        paintBarStrip(g, getLocalBounds().reduced(12), bars, options);
    }

    void paintBarStrip(juce::Graphics& g,
                       juce::Rectangle<int> bounds,
                       const std::vector<ScoreBar>& bars,
                       const BarStripPaintOptions& options = {}) const
    {
        if (bars.empty())
            return;

        const int barW = bounds.getWidth() / static_cast<int>(bars.size());
        if (barW <= 0)
            return;

        int x = bounds.getX();
        for (size_t i = 0; i < bars.size(); ++i)
        {
            const auto& bar = bars[i];
            juce::Rectangle<int> barRect(x, bounds.getY(), barW, bounds.getHeight());
            const bool isCurrent = options.highlightCurrentBar && bar.barNumber == options.currentBarForHighlight;
            drawBar(g, bar, barRect, isCurrent, i == 0, options.includeLiveChordMarker);
            x += barW;
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (model == nullptr || model->empty() || onBarClicked == nullptr)
            return;

        const int bar = getBarNumberAtX(event.x);
        if (bar > 0)
            onBarClicked(bar);
    }

private:
    int getBarNumberAtX(int x) const
    {
        if (model == nullptr || model->empty())
            return -1;

        const auto bars = getCurrentWindowBars();
        if (bars.empty())
            return -1;

        auto bounds = getLocalBounds().reduced(12);
        const int barW = bounds.getWidth() / static_cast<int>(bars.size());
        if (barW <= 0)
            return -1;

        int barIndex = (x - bounds.getX()) / barW;
        barIndex = juce::jlimit(0, static_cast<int>(bars.size()) - 1, barIndex);
        return bars[(size_t) barIndex].barNumber;
    }

    std::vector<ScoreBar> getCurrentWindowBars() const
    {
        if (model == nullptr || model->empty())
            return {};

        const int clampedCenterBar = juce::jlimit(model->getFirstBar(), model->getLastBar(), currentBar);
        auto bars = model->getWindowBars(clampedCenterBar, 2, 2);
        if (bars.empty())
            bars = model->getWindowBars(model->getLastBar(), 2, 2);
        return bars;
    }

    struct SpelledPitch
    {
        int letter = 0;      // 0..6 => C..B
        int accidental = 0;  // -1 flat, 0 natural, +1 sharp
        int octave = 4;      // scientific pitch octave
    };

    enum class SpellingMode
    {
        sharps,
        flats,
        mixedInC
    };

    static bool isBlackKeyPitchClass(int pitchClass)
    {
        const int pc = (pitchClass % 12 + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    SpellingMode getSpellingMode() const
    {
        if (hasKeySignature && keySharpsOrFlats < 0)
            return SpellingMode::flats;
        if (hasKeySignature && keySharpsOrFlats > 0)
            return SpellingMode::sharps;
        return SpellingMode::mixedInC;
    }

    SpelledPitch spellMidiPitch(int midiNote) const
    {
        static constexpr std::array<int, 12> sharpLetters = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
        static constexpr std::array<int, 12> sharpAccids =  { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
        static constexpr std::array<int, 12> flatLetters =  { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
        static constexpr std::array<int, 12> flatAccids =   { 0,-1, 0,-1, 0, 0,-1, 0,-1, 0,-1, 0 };
        static constexpr std::array<int, 12> mixedLetters = { 0, 0, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };
        static constexpr std::array<int, 12> mixedAccids =  { 0, 1, 0,-1, 0, 0, 1, 0,-1, 0,-1, 0 };

        const int note = juce::jlimit(0, 127, midiNote);
        const int pc = note % 12;
        const int octave = note / 12 - 1;
        const auto mode = getSpellingMode();

        SpelledPitch spelled;
        spelled.octave = octave;
        switch (mode)
        {
            case SpellingMode::sharps:
                spelled.letter = sharpLetters[(size_t) pc];
                spelled.accidental = sharpAccids[(size_t) pc];
                break;
            case SpellingMode::flats:
                spelled.letter = flatLetters[(size_t) pc];
                spelled.accidental = flatAccids[(size_t) pc];
                break;
            case SpellingMode::mixedInC:
                spelled.letter = mixedLetters[(size_t) pc];
                spelled.accidental = mixedAccids[(size_t) pc];
                break;
        }

        return spelled;
    }

    juce::String accidentalTextForMidi(int midiNote) const
    {
        if (clefType == ClefType::drum)
            return {};

        const auto spelled = spellMidiPitch(midiNote);
        if (spelled.accidental == 0)
            return {};
        return spelled.accidental < 0 ? "b" : "#";
    }

    int displayMidiNote(int rawMidiNote) const
    {
        return applyDisplayOctaveShiftForView(rawMidiNote,
                                              static_cast<int>(displayOctaveShift),
                                              clefType == ClefType::drum);
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

    float staffYForMidi(int midiNote, const juce::Rectangle<int>& area, ClefType clef) const
    {
        if (clef == ClefType::drum)
        {
            // Practical GM-style mapping: kick low, snare center, cymbals high.
            const float center = static_cast<float>(area.getCentreY());
            const float lineSpacing = 11.0f;
            switch (midiNote)
            {
                case 35: [[fallthrough]];
                case 36: return center + lineSpacing * 1.5f; // kick
                case 38: [[fallthrough]];
                case 40: return center; // snare
                case 41: [[fallthrough]];
                case 43: return center + lineSpacing; // low tom
                case 45: [[fallthrough]];
                case 47: return center + lineSpacing * 0.5f; // mid tom
                case 48: [[fallthrough]];
                case 50: return center - lineSpacing * 0.5f; // high tom
                case 42: [[fallthrough]];
                case 44: return center - lineSpacing * 1.5f; // closed/pedal hat
                case 46: return center - lineSpacing * 2.0f; // open hat
                case 49: [[fallthrough]];
                case 57: return center - lineSpacing * 2.5f; // crash
                case 51: [[fallthrough]];
                case 53: [[fallthrough]];
                case 59: return center - lineSpacing * 2.2f; // ride
                default:
                {
                    const float mapped = center - (static_cast<float>(midiNote) - 43.0f) * 2.2f;
                    const float top = static_cast<float>(area.getCentreY() - 33);
                    const float bottom = static_cast<float>(area.getCentreY() + 33);
                    return juce::jlimit(top, bottom, mapped);
                }
            }
        }

        const auto spelled = spellMidiPitch(midiNote);
        const int diatonicIndex = spelled.octave * 7 + spelled.letter;
        const int referenceDiatonicIndex = clef == ClefType::bass
            ? (3 * 7 + 1) // D3 on bass middle line
            : (4 * 7 + 6); // B4 on treble middle line

        const float center = static_cast<float>(area.getCentreY());
        constexpr float diatonicStepPx = 5.5f; // line-space distance (half of 11 px staff line gap)
        return center - static_cast<float>(diatonicIndex - referenceDiatonicIndex) * diatonicStepPx;
    }

    static bool isCymbalOrHatMidi(int midiNote)
    {
        switch (midiNote)
        {
            case 42: [[fallthrough]];
            case 44: [[fallthrough]];
            case 46: [[fallthrough]];
            case 49: [[fallthrough]];
            case 51: [[fallthrough]];
            case 52: [[fallthrough]];
            case 53: [[fallthrough]];
            case 55: [[fallthrough]];
            case 57: [[fallthrough]];
            case 59:
                return true;
            default:
                return false;
        }
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
        return 11 + symbolCount * 12;
    }

    static int clefReserveWidth(ClefType type)
    {
        if (type == ClefType::treble)
            return 48;
        if (type == ClefType::bass)
            return 40;
        return 0;
    }

    static void drawSharpGlyph(juce::Graphics& g, int x, int y, float scale = 1.0f)
    {
        const float x0 = static_cast<float>(x);
        const float y0 = static_cast<float>(y);
        const float s = juce::jmax(0.2f, scale);
        g.drawLine(x0 + 5.0f * s, y0 + 1.0f * s,  x0 + 4.0f * s,  y0 + 19.0f * s, 1.3f * s);
        g.drawLine(x0 + 11.0f * s, y0 + 0.0f * s, x0 + 10.0f * s, y0 + 18.0f * s, 1.3f * s);
        g.drawLine(x0 + 2.0f * s, y0 + 7.0f * s,  x0 + 14.0f * s, y0 + 5.0f * s,  1.3f * s);
        g.drawLine(x0 + 1.0f * s, y0 + 13.0f * s, x0 + 13.0f * s, y0 + 11.0f * s, 1.3f * s);
    }

    void drawClefSymbol(juce::Graphics& g, const juce::Rectangle<int>& staffRect, int reserveWidth, juce::Colour colour) const
    {
        if (reserveWidth <= 0)
            return;

        juce::String symbol;
        if (clefType == ClefType::treble)
            symbol = juce::String::charToString(static_cast<juce_wchar>(0x1D11E)); // G clef
        else if (clefType == ClefType::bass)
            symbol = juce::String::charToString(static_cast<juce_wchar>(0x1D122)); // F clef
        else
            return;

        g.setColour(colour);
        g.setFont(52.0f);
        g.drawText(symbol,
                   juce::Rectangle<int>(staffRect.getX() + 2, staffRect.getY() - 16, reserveWidth, staffRect.getHeight() + 22),
                   juce::Justification::centredLeft);
    }

    void drawKeySignature(juce::Graphics& g,
                          const juce::Rectangle<int>& staffRect,
                          int xOffset,
                          int reserveWidth,
                          juce::Colour colour) const
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
        g.setFont(17.0f);
        int x = staffRect.getX() + xOffset + 8;
        const int maxX = staffRect.getX() + xOffset + reserveWidth;
        for (int i = 0; i < symbolCount; ++i)
        {
            const int y = centerY - offsets[(size_t) i] * halfStep - 11;
            if (isSharp)
                drawSharpGlyph(g, x, y, 1.0f);
            else
                g.drawText(symbol, juce::Rectangle<int>(x, y, 18, 21), juce::Justification::centred, false);
            x += 12;
            if (x > maxX)
                break;
        }
    }

    void drawBar(juce::Graphics& g,
                 const ScoreBar& bar,
                 const juce::Rectangle<int>& barRect,
                 bool isCurrent,
                 bool isFirstVisibleBar,
                 bool includeLiveChordMarker) const
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
        const bool isDrumStaff = clefType == ClefType::drum;
        const bool showStartSymbols = isFirstVisibleBar && !isDrumStaff;
        const int clefWidth = showStartSymbols ? clefReserveWidth(clefType) : 0;
        const int keySigWidth = showStartSymbols ? keySignatureReserveWidth(hasKeySignature, keySharpsOrFlats) : 0;

        auto barHeader = barRect;
        barHeader = barHeader.removeFromTop(18);
        barHeader = barHeader.reduced(8, 0);
        g.setColour(labelColour);
        g.setFont(13.0f);
        auto barTextArea = barHeader.removeFromLeft(100);
        auto clefTextArea = barHeader.removeFromRight(84);
        clefTextArea = clefTextArea.withTrimmedRight(6);
        g.drawText("Bar " + juce::String(bar.barNumber), barTextArea, juce::Justification::centredLeft);
        auto headerLabel = staffLabel.trim();
        if (headerLabel.isEmpty())
            headerLabel = clefType == ClefType::bass ? "Bass"
                        : (clefType == ClefType::drum ? "Drum" : "Treble");
        g.drawText(headerLabel, clefTextArea, juce::Justification::centredRight);

        g.setColour(staffLineColour);
        const int lineSpacing = 11;
        for (int i = 0; i < 5; ++i)
        {
            const int y = staffRect.getCentreY() - 2 * lineSpacing + i * lineSpacing;
            g.drawLine((float) staffRect.getX(), (float) y, (float) staffRect.getRight(), (float) y, 1.0f);
        }

        if (clefWidth > 0)
            drawClefSymbol(g, staffRect, clefWidth, noteColour);
        if (keySigWidth > 0)
            drawKeySignature(g, staffRect, clefWidth, keySigWidth, noteColour);

        const double qPerBar = static_cast<double>(bar.numerator) * 4.0 / static_cast<double>(bar.denominator);
        const int startSymbolWidth = clefWidth + keySigWidth;
        const float left = static_cast<float>(staffRect.getX() + 6 + startSymbolWidth);
        const float width = static_cast<float>(juce::jmax(20, staffRect.getWidth() - 12 - startSymbolWidth));
        const int beatCount = juce::jmax(1, bar.numerator);

        g.setColour(chordColour);
        g.setFont(14.0f);
        if (bar.chords.empty())
        {
            g.drawText("-",
                       juce::Rectangle<int>(staffRect.getX() + 6, chordTop, staffRect.getWidth() - 12, 20),
                       juce::Justification::centredLeft);
        }
        else
        {
            constexpr int chordLabelWidth = 86;
            bool firstLabelPlaced = false;
            int previousRight = staffRect.getX() + 6;
            for (const auto& chord : bar.chords)
            {
                int textX = staffRect.getX() + 6;
                if (!firstLabelPlaced || chord.quarter <= 1.0e-6)
                {
                    // Anchor the first chord at the left edge to align with the bar counter header.
                    textX = barRect.getX() + 8;
                }
                else
                {
                    const double clampedQuarter = juce::jlimit(0.0, juce::jmax(0.0, qPerBar), chord.quarter);
                    const float chordX = left + static_cast<float>((clampedQuarter / juce::jmax(0.25, qPerBar)) * width);
                    textX = juce::jlimit(staffRect.getX(), staffRect.getRight() - chordLabelWidth,
                                         static_cast<int>(chordX) - chordLabelWidth / 2);
                    textX = juce::jmax(textX, previousRight + 2);
                }

                g.drawText(chord.symbol,
                           juce::Rectangle<int>(textX, chordTop, chordLabelWidth, 20),
                           juce::Justification::centred);
                firstLabelPlaced = true;
                previousRight = textX + chordLabelWidth;
            }
        }

        if (includeLiveChordMarker
            && liveChordVisible
            && bar.barNumber == liveChordBarNumber
            && liveChordText.isNotEmpty())
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

            const int displayNote = displayMidiNote(note.midiNote);
            const float y = staffYForMidi(displayNote, staffRect, clefType);
            const float w = durationWidth(note.value);
            const float h = 7.0f;

            g.setColour(noteColour);
            if (!isDrumStaff)
            {
                drawLedgerLines(g, staffRect, x + w * 0.5f, y);
                g.fillEllipse(x, y - h * 0.5f, w, h);
            }
            else if (isCymbalOrHatMidi(displayNote))
            {
                const float cx = x + w * 0.5f;
                const float r = 3.2f;
                g.drawLine(cx - r, y - r, cx + r, y + r, 1.4f);
                g.drawLine(cx - r, y + r, cx + r, y - r, 1.4f);
            }
            else
            {
                g.fillEllipse(x, y - h * 0.5f, w, h);
            }

            if (note.value != NoteValue::whole)
            {
                const float stemX = x + w;
                g.drawLine(stemX, y, stemX, y - 24.0f, 1.2f);
            }

            if (note.value == NoteValue::eighth || note.value == NoteValue::sixteenth)
                g.drawLine(x + w, y - 24.0f, x + w + 6.0f, y - 20.0f, 1.1f);
            if (note.value == NoteValue::sixteenth)
                g.drawLine(x + w, y - 18.0f, x + w + 6.0f, y - 14.0f, 1.1f);

            const auto accidentalText = accidentalTextForMidi(displayNote);
            if (accidentalText.isNotEmpty())
            {
                const int accidentalX = static_cast<int>(x - 11.0f);
                const int accidentalY = static_cast<int>(y - 8.0f);
                if (accidentalText == "#")
                    drawSharpGlyph(g, accidentalX, accidentalY, 0.58f);
                else
                {
                    g.setFont(12.0f);
                    g.drawText(accidentalText,
                               juce::Rectangle<int>(accidentalX, accidentalY, 12, 16),
                               juce::Justification::centred,
                               false);
                }
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
            const float yA = staffYForMidi(displayMidiNote(leftNote.midiNote), staffRect, clefType) - 24.0f;
            const float yB = staffYForMidi(displayMidiNote(rightNote.midiNote), staffRect, clefType) - 24.0f;

            g.setColour(noteColour);
            g.drawLine(xA, yA, xB, yB, 2.0f);
        }
    }

    const ScoreModel* model = nullptr;
    int currentBar = 1;
    ClefType clefType = ClefType::treble;
    ColorScheme colorScheme = ColorScheme::dark;
    DisplayOctaveShift displayOctaveShift = DisplayOctaveShift::normal;
    bool hasKeySignature = false;
    int keySharpsOrFlats = 0;
    bool liveChordVisible = false;
    int liveChordBarNumber = 1;
    double liveChordQuarterInBar = 0.0;
    juce::String liveChordText;
    juce::String staffLabel;
    std::function<void(int)> onBarClicked;
};
