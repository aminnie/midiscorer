#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include "../midi/MidiProjectLoader.h"
#include "../notation/Quantizer.h"
#include "../notation/ScoreModel.h"
#include "../notation/ScoreRenderer.h"
#include "../harmony/ChordDetector.h"
#include "../playback/PlaybackController.h"

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent()
    {
        addAndMakeVisible(loadButton);
        loadButton.setButtonText("Load MIDI");
        loadButton.onClick = [this] { loadMidiFile(); };

        addAndMakeVisible(staff1TrackLabel);
        staff1TrackLabel.setText("Staff 1", juce::dontSendNotification);
        staff1TrackLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(staff1TrackSelector);
        staff1TrackSelector.onChange = [this] { rebuildAllStaffs(); };
        addAndMakeVisible(staff1ClefSelector);
        staff1ClefSelector.addItem("Treble", 1);
        staff1ClefSelector.addItem("Bass", 2);
        staff1ClefSelector.addItem("Drum", 3);
        staff1ClefSelector.setSelectedId(1, juce::dontSendNotification);
        staff1ClefSelector.onChange = [this] { rebuildAllStaffs(); };

        addAndMakeVisible(staff2TrackLabel);
        staff2TrackLabel.setText("Staff 2", juce::dontSendNotification);
        staff2TrackLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(staff2TrackSelector);
        staff2TrackSelector.onChange = [this] { rebuildAllStaffs(); };
        addAndMakeVisible(staff2ClefSelector);
        staff2ClefSelector.addItem("Treble", 1);
        staff2ClefSelector.addItem("Bass", 2);
        staff2ClefSelector.addItem("Drum", 3);
        staff2ClefSelector.setSelectedId(1, juce::dontSendNotification);
        staff2ClefSelector.onChange = [this] { rebuildAllStaffs(); };

        addAndMakeVisible(staff3TrackLabel);
        staff3TrackLabel.setText("Staff 3", juce::dontSendNotification);
        staff3TrackLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(staff3TrackSelector);
        staff3TrackSelector.onChange = [this] { rebuildAllStaffs(); };
        addAndMakeVisible(staff3ClefSelector);
        staff3ClefSelector.addItem("Treble", 1);
        staff3ClefSelector.addItem("Bass", 2);
        staff3ClefSelector.addItem("Drum", 3);
        staff3ClefSelector.setSelectedId(1, juce::dontSendNotification);
        staff3ClefSelector.onChange = [this] { rebuildAllStaffs(); };

        addAndMakeVisible(transportToggleButton);
        transportToggleButton.setButtonText("Start");
        transportToggleButton.onClick = [this] { onTransportToggleClicked(); };

        addAndMakeVisible(accidentalSelector);
        accidentalSelector.addItem("Sharp names", 1);
        accidentalSelector.addItem("Flat names", 2);
        accidentalSelector.setSelectedId(1, juce::dontSendNotification);
        accidentalSelector.onChange = [this] { rebuildAllStaffs(); };
        addAndMakeVisible(accidentalHelpButton);
        accidentalHelpButton.setButtonText("?");
        accidentalHelpButton.onClick = [this] { showAccidentalHelpModal(); };

        addAndMakeVisible(aliasSelector);
        aliasSelector.addItem("Chord text plain", 1);
        aliasSelector.addItem("Chord text jazz", 2);
        aliasSelector.setSelectedId(1, juce::dontSendNotification);
        aliasSelector.onChange = [this] { rebuildAllStaffs(); };

        addAndMakeVisible(chordTracksLabel);
        chordTracksLabel.setText("Chord Tracks", juce::dontSendNotification);
        chordTracksLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(scoreColorToggle);
        scoreColorToggle.setButtonText("Light Score");
        scoreColorToggle.setToggleState(true, juce::dontSendNotification);
        scoreColorToggle.onClick = [this] { applyScoreColorScheme(); };
        scoreColorToggle.setTooltip("Toggle score display between white-on-black and black-on-white.");

        addAndMakeVisible(globalTransposeLabel);
        globalTransposeLabel.setText("Transpose", juce::dontSendNotification);
        globalTransposeLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(globalTransposeInput);
        globalTransposeInput.setInputRestrictions(3, "-0123456789");
        globalTransposeInput.setText("0", juce::dontSendNotification);
        globalTransposeInput.onReturnKey = [this] { rebuildAllStaffs(); };
        globalTransposeInput.onFocusLost = [this] { rebuildAllStaffs(); };
        addAndMakeVisible(transposeHelpButton);
        transposeHelpButton.setButtonText("?");
        transposeHelpButton.onClick = [this] { showTransposeHelpModal(); };

        addAndMakeVisible(continueButton);
        continueButton.setButtonText("Continue");
        continueButton.onClick = [this] { continuePlaybackFromBar(); };

        addAndMakeVisible(continueBarLabel);
        continueBarLabel.setText("Bar", juce::dontSendNotification);
        continueBarLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(continueBarInput);
        continueBarInput.setInputRestrictions(5, "0123456789");
        continueBarInput.setText("1", juce::dontSendNotification);
        continueBarInput.setJustification(juce::Justification::centred);
        continueBarInput.onReturnKey = [this] { continuePlaybackFromBar(); };
        continueBarInput.setTooltip("Continue playback from this bar after pressing Stop.");

        addAndMakeVisible(savePresetButton);
        savePresetButton.setButtonText("Save Preset");
        savePresetButton.onClick = [this] { saveUiPreset(); };

        addAndMakeVisible(loadPresetButton);
        loadPresetButton.setButtonText("Load Preset");
        loadPresetButton.onClick = [this] { (void) loadUiPreset(); };

        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(keyOverrideLabel);
        keyOverrideLabel.setText("Key:", juce::dontSendNotification);
        keyOverrideLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(keyOverrideInput);
        keyOverrideInput.setInputRestrictions(6, "ABCDEFGabcdefg#bBmMnNiI");
        keyOverrideInput.setText({}, juce::dontSendNotification);
        keyOverrideInput.onReturnKey = [this] { applyKeyOverrideFromInput(true); rebuildAllStaffs(); };
        keyOverrideInput.onFocusLost = [this] { applyKeyOverrideFromInput(true); rebuildAllStaffs(); };
        addAndMakeVisible(keyHelpButton);
        keyHelpButton.setButtonText("?");
        keyHelpButton.onClick = [this] { showKeyHelpModal(); };

        addAndMakeVisible(tempoOverrideLabel);
        tempoOverrideLabel.setText("Tempo:", juce::dontSendNotification);
        tempoOverrideLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(tempoOverrideInput);
        tempoOverrideInput.setInputRestrictions(6, "0123456789.");
        tempoOverrideInput.setText({}, juce::dontSendNotification);
        tempoOverrideInput.setTooltip("Override playback tempo BPM (e.g., 96 or 120.5). Leave blank to use detected tempo.");
        tempoOverrideInput.onReturnKey = [this] { applyTempoOverrideFromInput(true); };
        tempoOverrideInput.onFocusLost = [this] { applyTempoOverrideFromInput(true); };
        addAndMakeVisible(tempoHelpButton);
        tempoHelpButton.setButtonText("?");
        tempoHelpButton.onClick = [this] { showTempoHelpModal(); };

        addAndMakeVisible(transportLabel);
        transportLabel.setText("Bar 1", juce::dontSendNotification);
        transportLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(keyLabel);
        keyLabel.setText("Key: n/a", juce::dontSendNotification);
        keyLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(midiMetaLabel);
        midiMetaLabel.setText("Sig: n/a  Tempo: n/a", juce::dontSendNotification);
        midiMetaLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(scoreRenderer);
        scoreRenderer.setScoreModel(&scoreModel1);
        scoreRenderer.setClefType(ScoreRenderer::ClefType::treble);
        addAndMakeVisible(scoreRenderer2);
        scoreRenderer2.setScoreModel(&scoreModel2);
        scoreRenderer2.setClefType(ScoreRenderer::ClefType::treble);
        addAndMakeVisible(scoreRenderer3);
        scoreRenderer3.setScoreModel(&scoreModel3);
        scoreRenderer3.setClefType(ScoreRenderer::ClefType::treble);
        applyScoreColorScheme();
        setStatusMessage("Load a MIDI file to begin.");

        updateTransportControls();
        setSize(1280, 720);
        startTimerHz(30);
    }

    ~MainComponent() override = default;

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        auto row = area.removeFromTop(34);

        loadButton.setBounds(row.removeFromLeft(120).reduced(4, 0));
        transportToggleButton.setBounds(row.removeFromLeft(106).reduced(4, 0));
        continueButton.setBounds(row.removeFromLeft(100).reduced(4, 0));
        continueBarLabel.setBounds(row.removeFromLeft(34));
        continueBarInput.setBounds(row.removeFromLeft(64).reduced(4, 0));
        accidentalSelector.setBounds(row.removeFromLeft(120).reduced(4, 0));
        accidentalHelpButton.setBounds(row.removeFromLeft(28).reduced(4, 0));
        aliasSelector.setBounds(row.removeFromLeft(136).reduced(4, 0));
        globalTransposeLabel.setBounds(row.removeFromLeft(74));
        globalTransposeInput.setBounds(row.removeFromLeft(48).reduced(4, 0));
        transposeHelpButton.setBounds(row.removeFromLeft(28).reduced(4, 0));
        scoreColorToggle.setBounds(row.removeFromLeft(106).reduced(4, 0));
        chordTracksLabel.setBounds(row.removeFromLeft(110));

        area.removeFromTop(6);
        auto selectorRow = area.removeFromTop(26);
        constexpr int staffSectionWidth = 338;
        constexpr int staffSectionGap = 6;
        auto section1 = selectorRow.removeFromLeft(juce::jmin(staffSectionWidth, selectorRow.getWidth()));
        selectorRow.removeFromLeft(juce::jmin(staffSectionGap, selectorRow.getWidth()));
        auto section2 = selectorRow.removeFromLeft(juce::jmin(staffSectionWidth, selectorRow.getWidth()));
        selectorRow.removeFromLeft(juce::jmin(staffSectionGap, selectorRow.getWidth()));
        auto section3 = selectorRow.removeFromLeft(juce::jmin(staffSectionWidth, selectorRow.getWidth()));
        layoutStaffControls(section1, staff1TrackLabel, staff1TrackSelector, staff1ClefSelector);
        layoutStaffControls(section2, staff2TrackLabel, staff2TrackSelector, staff2ClefSelector);
        layoutStaffControls(section3, staff3TrackLabel, staff3TrackSelector, staff3ClefSelector);
        staff2TrackLabel.setBounds(staff2TrackLabel.getBounds().translated(6, 0));
        staff3TrackLabel.setBounds(staff3TrackLabel.getBounds().translated(6, 0));

        area.removeFromTop(6);
        auto chordTracksArea = area.removeFromTop(getChordTracksLayoutHeight(area.getWidth()));
        layoutChordTrackButtons(chordTracksArea);

        area.removeFromTop(6);
        auto statusRow = area.removeFromTop(24);
        savePresetButton.setBounds(statusRow.removeFromLeft(100).reduced(4, 0));
        loadPresetButton.setBounds(statusRow.removeFromLeft(100).reduced(4, 0));
        tempoOverrideLabel.setBounds(statusRow.removeFromLeft(50));
        tempoOverrideInput.setBounds(statusRow.removeFromLeft(72).reduced(4, 0));
        tempoHelpButton.setBounds(statusRow.removeFromLeft(28).reduced(4, 0));
        keyOverrideLabel.setBounds(statusRow.removeFromLeft(36));
        keyOverrideInput.setBounds(statusRow.removeFromLeft(68).reduced(4, 0));
        keyHelpButton.setBounds(statusRow.removeFromLeft(28).reduced(4, 0));
        statusLabel.setBounds(statusRow);
        area.removeFromTop(8);

        const int laneGap = 4;
        auto lane1 = area.removeFromTop((area.getHeight() - laneGap * 2) / 3);
        area.removeFromTop(laneGap);
        auto lane2 = area.removeFromTop((area.getHeight() - laneGap) / 2);
        area.removeFromTop(laneGap);
        auto lane3 = area;

        scoreRenderer.setBounds(lane1);
        scoreRenderer2.setBounds(lane2);
        scoreRenderer3.setBounds(lane3);
    }

private:
    struct LiveChordState
    {
        int lastEighthIndex = std::numeric_limits<int>::min();
        juce::String lastDisplayedChord;
        bool hasMarker = false;
        int markerBar = 1;
        double markerQuarterInBar = 0.0;
    };

    struct ParsedKey
    {
        int tonicPc = 0;
        int sharpsOrFlats = 0;
        bool isMinor = false;
        juce::String displayText;
    };

    void updateWindowTitle()
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        {
            const auto title = project.file.existsAsFile()
                ? ("MidiScorer - " + project.file.getFileName())
                : juce::String("MidiScorer");
            window->setName(title);
        }
    }

    static int getClampedTranspose(const juce::TextEditor& input)
    {
        return juce::jlimit(-24, 24, input.getText().trim().getIntValue());
    }

    static juce::String formatTempoBpm(double bpm)
    {
        const int rounded = static_cast<int>(std::round(bpm));
        return std::abs(bpm - static_cast<double>(rounded)) < 0.05
            ? juce::String(rounded) + " BPM"
            : juce::String(bpm, 1) + " BPM";
    }

    std::optional<double> parseTempoOverrideText(const juce::String& input) const
    {
        const auto trimmed = input.trim();
        if (trimmed.isEmpty())
            return std::nullopt;

        const double bpm = trimmed.getDoubleValue();
        if (!std::isfinite(bpm) || bpm < 10.0 || bpm > 400.0)
            return std::nullopt;
        return bpm;
    }

    std::optional<double> getDetectedTempoBpm() const
    {
        const auto& tempos = project.tempoMap.getTempoEvents();
        if (tempos.empty())
            return std::nullopt;
        return tempos.front().bpm;
    }

    void syncPlaybackTempoOverride()
    {
        playbackController.setTempoOverrideBpm(tempoOverrideBpm, getDetectedTempoBpm());
    }

    static int tonicPcFromSignature(int sharpsOrFlats, bool isMinor)
    {
        static constexpr std::array<int, 15> majorTonicPcs = { 11, 6, 1, 8, 3, 10, 5, 0, 7, 2, 9, 4, 11, 6, 1 };
        static constexpr std::array<int, 15> minorTonicPcs = { 8, 3, 10, 5, 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10 };
        const int idx = juce::jlimit(0, 14, sharpsOrFlats + 7);
        return isMinor ? minorTonicPcs[(size_t) idx] : majorTonicPcs[(size_t) idx];
    }

    static std::optional<ParsedKey> parseKeyText(const juce::String& input)
    {
        auto raw = input.trim().removeCharacters(" ");
        if (raw.isEmpty())
            return std::nullopt;

        bool isMinor = false;
        if (raw.endsWithIgnoreCase("m") && raw.length() > 1)
        {
            isMinor = true;
            raw = raw.dropLastCharacters(1);
        }

        if (raw.isEmpty())
            return std::nullopt;

        const auto root = raw.substring(0, 1).toUpperCase();
        juce::String accidental;
        if (raw.length() >= 2)
        {
            const auto c = raw.substring(1, 2);
            if (c == "#" || c.equalsIgnoreCase("b"))
                accidental = c == "#" ? "#" : "b";
        }

        const auto normalizedKey = root + accidental + (isMinor ? "m" : "");
        static constexpr const char* majorKeys[15]
            = { "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#" };
        static constexpr const char* minorKeys[15]
            = { "Abm", "Ebm", "Bbm", "Fm", "Cm", "Gm", "Dm", "Am", "Em", "Bm", "F#m", "C#m", "G#m", "D#m", "A#m" };
        const auto& keys = isMinor ? minorKeys : majorKeys;

        for (int i = 0; i < 15; ++i)
        {
            const juce::String candidate(keys[(size_t) i]);
            if (!candidate.equalsIgnoreCase(normalizedKey))
                continue;

            ParsedKey parsed;
            parsed.sharpsOrFlats = i - 7;
            parsed.isMinor = isMinor;
            parsed.tonicPc = tonicPcFromSignature(parsed.sharpsOrFlats, parsed.isMinor);
            parsed.displayText = candidate;
            return parsed;
        }

        return std::nullopt;
    }

    void applyKeyOverrideFromInput(bool userInitiated)
    {
        const auto text = keyOverrideInput.getText().trim();
        if (text.isEmpty())
        {
            keyOverride = std::nullopt;
            if (userInitiated)
                setStatusMessage("Key override cleared (using detected key).");
            return;
        }

        const auto parsed = parseKeyText(text);
        if (!parsed.has_value())
        {
            keyOverride = std::nullopt;
            keyOverrideInput.setText({}, juce::dontSendNotification);
            if (userInitiated)
                setStatusMessage("Invalid key override. Reverted to detected key.");
            return;
        }

        keyOverride = parsed;
        keyOverrideInput.setText(parsed->displayText, juce::dontSendNotification);
        if (userInitiated)
            setStatusMessage("Key override set to " + parsed->displayText + ".");
    }

    void applyTempoOverrideFromInput(bool userInitiated)
    {
        const auto text = tempoOverrideInput.getText().trim();
        if (text.isEmpty())
        {
            tempoOverrideBpm = std::nullopt;
            syncPlaybackTempoOverride();
            if (userInitiated)
                setStatusMessage("Tempo override cleared (using detected tempo).");
            return;
        }

        const auto parsed = parseTempoOverrideText(text);
        if (!parsed.has_value())
        {
            tempoOverrideBpm = std::nullopt;
            tempoOverrideInput.setText({}, juce::dontSendNotification);
            syncPlaybackTempoOverride();
            if (userInitiated)
                setStatusMessage("Invalid tempo override. Enter a value from 10 to 400 BPM.");
            return;
        }

        tempoOverrideBpm = parsed;
        tempoOverrideInput.setText(juce::String(parsed.value(), 1), juce::dontSendNotification);
        syncPlaybackTempoOverride();
        if (userInitiated)
            setStatusMessage("Tempo override set to " + formatTempoBpm(parsed.value()) + ".");
    }

    int getDetectedKeyTonicPc() const
    {
        if (!project.hasKeySignature)
            return 0;
        // tonicPcFromSignature expects "isMinor", while MIDI metadata stores "isMajor".
        const int tonicPc = tonicPcFromSignature(project.keySharpsOrFlats, !project.keyIsMajor);
        return tonicPc;
    }

    int getKeyOverrideTransposeSemitones() const
    {
        if (!keyOverride.has_value())
            return 0;

        const int detectedTonic = getDetectedKeyTonicPc();
        int delta = keyOverride->tonicPc - detectedTonic;
        while (delta > 6)
            delta -= 12;
        while (delta < -6)
            delta += 12;
        return delta;
    }

    int getEffectiveTransposeSemitones(bool normalizeText = false)
    {
        return getGlobalTransposeSemitones(normalizeText) + getKeyOverrideTransposeSemitones();
    }

    ChordDetector::NamingOptions getChordNamingOptions() const
    {
        ChordDetector::NamingOptions namingOptions;
        namingOptions.accidentalPreference = accidentalSelector.getSelectedId() == 2
            ? ChordDetector::AccidentalPreference::preferFlats
            : ChordDetector::AccidentalPreference::preferSharps;
        namingOptions.jazzAliasStyle = aliasSelector.getSelectedId() == 2
            ? ChordDetector::JazzAliasStyle::jazzSymbols
            : ChordDetector::JazzAliasStyle::plain;
        return namingOptions;
    }

    void resetLiveChordState()
    {
        for (auto& state : liveChordStates)
            state = {};

        scoreRenderer.setLiveChordMarker(false, 1, 0.0, {});
        scoreRenderer2.setLiveChordMarker(false, 1, 0.0, {});
        scoreRenderer3.setLiveChordMarker(false, 1, 0.0, {});
    }

    int getGlobalTransposeSemitones(bool normalizeText = false)
    {
        const int clamped = getClampedTranspose(globalTransposeInput);
        if (normalizeText)
            globalTransposeInput.setText(juce::String(clamped), juce::dontSendNotification);
        return clamped;
    }

    static ScoreRenderer::ClefType getClefTypeFromCombo(const juce::ComboBox& combo)
    {
        if (combo.getSelectedId() == 2)
            return ScoreRenderer::ClefType::bass;
        if (combo.getSelectedId() == 3)
            return ScoreRenderer::ClefType::drum;
        return ScoreRenderer::ClefType::treble;
    }

    static void layoutStaffControls(juce::Rectangle<int> area,
                                    juce::Label& label,
                                    juce::ComboBox& trackSelector,
                                    juce::ComboBox& clefSelector)
    {
        label.setBounds(area.removeFromLeft(52));
        trackSelector.setBounds(area.removeFromLeft(208).reduced(4, 0));
        clefSelector.setBounds(area.removeFromLeft(78).reduced(4, 0));
    }

    int getChordTracksLayoutHeight(int availableWidth) const
    {
        const int labelWidth = 110;
        const int xPadding = 6;
        const int yPadding = 4;
        const int rowHeight = 22;
        const int usableWidth = juce::jmax(120, availableWidth - labelWidth - xPadding * 2);

        int rows = 1;
        int rowX = 0;
        juce::Font measureFont(13.0f);
        for (const auto* button : chordTrackButtons)
        {
            const int buttonWidth = juce::jlimit(84, 190, measureFont.getStringWidth(button->getButtonText()) + 28);
            if (rowX > 0 && rowX + buttonWidth > usableWidth)
            {
                ++rows;
                rowX = 0;
            }
            rowX += buttonWidth + xPadding;
        }

        return yPadding * 2 + rows * rowHeight;
    }

    void layoutChordTrackButtons(juce::Rectangle<int> area)
    {
        auto labelArea = area.removeFromLeft(110);
        chordTracksLabel.setBounds(labelArea);

        const int xPadding = 6;
        const int rowHeight = 22;
        int x = area.getX();
        int y = area.getY() + 2;
        const int right = area.getRight();
        juce::Font measureFont(13.0f);

        for (auto* button : chordTrackButtons)
        {
            const int w = juce::jlimit(84, 190, measureFont.getStringWidth(button->getButtonText()) + 28);
            if (x > area.getX() && x + w > right)
            {
                x = area.getX();
                y += rowHeight;
            }
            button->setBounds(x, y, w, rowHeight - 2);
            x += w + xPadding;
        }
    }

    void refreshChordTrackButtons()
    {
        for (auto* button : chordTrackButtons)
            removeChildComponent(button);
        chordTrackButtons.clear(true);

        for (int i = 0; i < static_cast<int>(project.tracks.size()); ++i)
        {
            auto* button = chordTrackButtons.add(new juce::ToggleButton(project.tracks[(size_t) i].name));
            button->setToggleState(i < 5, juce::dontSendNotification);
            button->onClick = [this]() { rebuildAllStaffs(); };
            addAndMakeVisible(button);
        }

        // Buttons are created dynamically after MIDI load; force an immediate relayout
        // so they appear without requiring the user to manually resize the window.
        resized();
        repaint();
    }

    juce::String buildChordTrackSelectionCsv() const
    {
        juce::StringArray selected;
        for (int i = 0; i < chordTrackButtons.size(); ++i)
        {
            if (chordTrackButtons[i]->getToggleState())
                selected.add(juce::String(i));
        }
        return selected.joinIntoString(",");
    }

    void applyChordTrackSelectionCsv(const juce::String& csv)
    {
        for (auto* button : chordTrackButtons)
            button->setToggleState(false, juce::dontSendNotification);

        juce::StringArray tokens;
        tokens.addTokens(csv, ",", "");
        tokens.trim();
        tokens.removeEmptyStrings();

        for (const auto& token : tokens)
        {
            const int idx = token.getIntValue();
            if (idx >= 0 && idx < chordTrackButtons.size())
                chordTrackButtons[idx]->setToggleState(true, juce::dontSendNotification);
        }

        bool anySelected = false;
        for (auto* button : chordTrackButtons)
            anySelected = anySelected || button->getToggleState();

        if (!anySelected && chordTrackButtons.size() > 0)
            chordTrackButtons[0]->setToggleState(true, juce::dontSendNotification);
    }

    juce::File getPresetFilePath() const
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MidiScorer");
        if (!dir.exists())
            dir.createDirectory();
        return dir.getChildFile("ui_preset.json");
    }

    juce::String getSongPresetTempoKey() const
    {
        if (!project.file.existsAsFile())
            return {};
        return project.file.getFullPathName().replaceCharacter('\\', '/').toLowerCase();
    }

    juce::String getSongTempoOverrideFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetTempoKey();
        if (songKey.isEmpty() || !preset.hasProperty("tempoOverridesBySong"))
            return {};

        const auto overridesVar = preset.getProperty("tempoOverridesBySong");
        auto* overridesObj = overridesVar.getDynamicObject();
        if (overridesObj == nullptr)
            return {};

        const juce::Identifier songId(songKey);
        if (!overridesObj->hasProperty(songId))
            return {};

        return overridesObj->getProperty(songId).toString().trim();
    }

    void saveUiPreset()
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("staff1Track", staff1TrackSelector.getSelectedItemIndex());
        obj->setProperty("staff2Track", staff2TrackSelector.getSelectedItemIndex());
        obj->setProperty("staff3Track", staff3TrackSelector.getSelectedItemIndex());
        obj->setProperty("staff1Clef", staff1ClefSelector.getSelectedId());
        obj->setProperty("staff2Clef", staff2ClefSelector.getSelectedId());
        obj->setProperty("staff3Clef", staff3ClefSelector.getSelectedId());
        obj->setProperty("globalTranspose", getGlobalTransposeSemitones(true));
        obj->setProperty("chordTrackSelection", buildChordTrackSelectionCsv());
        obj->setProperty("accidental", accidentalSelector.getSelectedId());
        obj->setProperty("alias", aliasSelector.getSelectedId());
        obj->setProperty("scoreLightMode", scoreColorToggle.getToggleState());
        obj->setProperty("keyOverrideEnabled", keyOverride.has_value());
        obj->setProperty("keyOverrideText", keyOverride.has_value() ? keyOverride->displayText : juce::String());
        obj->setProperty("tempoOverrideEnabled", tempoOverrideBpm.has_value());
        obj->setProperty("tempoOverrideText", tempoOverrideInput.getText().trim());

        auto tempoOverridesBySong = std::make_unique<juce::DynamicObject>();
        const auto file = getPresetFilePath();
        if (file.existsAsFile())
        {
            const auto existingJsonText = file.loadFileAsString();
            juce::var existingParsed;
            const auto existingParseResult = juce::JSON::parse(existingJsonText, existingParsed);
            if (!existingParseResult.failed() && existingParsed.isObject())
            {
                if (auto* existingObj = existingParsed.getDynamicObject())
                {
                    const auto existingOverridesVar = existingObj->getProperty("tempoOverridesBySong");
                    if (auto* existingOverridesObj = existingOverridesVar.getDynamicObject())
                    {
                        for (const auto& prop : existingOverridesObj->getProperties())
                            tempoOverridesBySong->setProperty(prop.name, prop.value);
                    }
                }
            }
        }

        const auto songKey = getSongPresetTempoKey();
        if (songKey.isNotEmpty())
        {
            const juce::Identifier songId(songKey);
            const auto songTempoText = tempoOverrideInput.getText().trim();
            if (songTempoText.isNotEmpty())
                tempoOverridesBySong->setProperty(songId, songTempoText);
            else
                tempoOverridesBySong->removeProperty(songId);
        }
        obj->setProperty("tempoOverridesBySong", juce::var(tempoOverridesBySong.release()));

        const juce::var payload(obj.release());
        if (file.replaceWithText(juce::JSON::toString(payload)))
            setStatusMessage("Preset saved: " + file.getFullPathName());
        else
            setStatusMessage("Failed to save preset.");
    }

    bool loadUiPreset(bool silent = false)
    {
        const auto file = getPresetFilePath();
        if (!file.existsAsFile())
        {
            if (!silent)
                setStatusMessage("Preset file not found.");
            return false;
        }

        const auto jsonText = file.loadFileAsString();
        juce::var parsed;
        const auto parseResult = juce::JSON::parse(jsonText, parsed);
        if (parseResult.failed() || !parsed.isObject())
        {
            if (!silent)
                setStatusMessage("Preset parse failed.");
            return false;
        }

        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return false;

        auto getIntProperty = [obj](const juce::Identifier& key, int fallback) -> int
        {
            if (!obj->hasProperty(key))
                return fallback;
            return static_cast<int>(obj->getProperty(key));
        };

        auto applyTrackSelection = [this](juce::ComboBox& box, int idx)
        {
            const int clamped = juce::jlimit(0, juce::jmax(0, box.getNumItems() - 1), idx);
            if (box.getNumItems() > 0)
                box.setSelectedItemIndex(clamped, juce::dontSendNotification);
        };

        applyTrackSelection(staff1TrackSelector, getIntProperty("staff1Track", 0));
        applyTrackSelection(staff2TrackSelector, getIntProperty("staff2Track", 0));
        applyTrackSelection(staff3TrackSelector, getIntProperty("staff3Track", 0));

        staff1ClefSelector.setSelectedId(getIntProperty("staff1Clef", 1), juce::dontSendNotification);
        staff2ClefSelector.setSelectedId(getIntProperty("staff2Clef", 1), juce::dontSendNotification);
        staff3ClefSelector.setSelectedId(getIntProperty("staff3Clef", 1), juce::dontSendNotification);
        globalTransposeInput.setText(juce::String(juce::jlimit(-24, 24, getIntProperty("globalTranspose", 0))), juce::dontSendNotification);
        if (obj->hasProperty("chordTrackSelection"))
            applyChordTrackSelectionCsv(obj->getProperty("chordTrackSelection").toString());
        else
            applyChordTrackSelectionCsv("0,1,2,3,4");
        accidentalSelector.setSelectedId(getIntProperty("accidental", 1), juce::dontSendNotification);
        aliasSelector.setSelectedId(getIntProperty("alias", 1), juce::dontSendNotification);
        scoreColorToggle.setToggleState(getIntProperty("scoreLightMode", 0) != 0, juce::dontSendNotification);
        const bool keyOverrideEnabled = getIntProperty("keyOverrideEnabled", 0) != 0;
        if (keyOverrideEnabled && obj->hasProperty("keyOverrideText"))
            keyOverrideInput.setText(obj->getProperty("keyOverrideText").toString(), juce::dontSendNotification);
        else
            keyOverrideInput.setText({}, juce::dontSendNotification);
        applyKeyOverrideFromInput(false);

        const auto songTempoOverride = getSongTempoOverrideFromPreset(*obj);
        if (songTempoOverride.isNotEmpty())
            tempoOverrideInput.setText(songTempoOverride, juce::dontSendNotification);
        else
        {
            const bool tempoOverrideEnabled = getIntProperty("tempoOverrideEnabled", 0) != 0;
            if (tempoOverrideEnabled && obj->hasProperty("tempoOverrideText"))
                tempoOverrideInput.setText(obj->getProperty("tempoOverrideText").toString(), juce::dontSendNotification);
            else
                tempoOverrideInput.setText({}, juce::dontSendNotification);
        }
        applyTempoOverrideFromInput(false);
        applyScoreColorScheme();

        rebuildAllStaffs();
        if (!silent)
            setStatusMessage("Preset loaded.");
        return true;
    }

    std::vector<MidiNoteEvent> collectChordAnalysisNotes(int selectedTrackIndex)
    {
        std::vector<MidiNoteEvent> mergedNotes;
        juce::Array<int> selectedIndices;
        for (int i = 0; i < chordTrackButtons.size(); ++i)
        {
            if (chordTrackButtons[i]->getToggleState())
                selectedIndices.add(i);
        }

        if (selectedIndices.isEmpty() && selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(project.tracks.size()))
            selectedIndices.add(selectedTrackIndex);

        size_t reserveCount = 0;
        for (int idx : selectedIndices)
            reserveCount += project.tracks[(size_t) idx].notes.size();
        mergedNotes.reserve(reserveCount);

        for (int idx : selectedIndices)
        {
            const auto& notes = project.tracks[(size_t) idx].notes;
            mergedNotes.insert(mergedNotes.end(), notes.begin(), notes.end());
        }

        const int transposeSemitones = getEffectiveTransposeSemitones();
        if (transposeSemitones != 0)
        {
            for (auto& note : mergedNotes)
            {
                // Preserve General MIDI percussion mapping on channel 10.
                if (note.channel == 10)
                    continue;
                note.noteNumber = juce::jlimit(0, 127, note.noteNumber + transposeSemitones);
            }
        }

        return mergedNotes;
    }

    void timerCallback() override
    {
        if (!playbackController.isPlaying())
            return;

        if (playbackController.hasReachedEnd())
        {
            stopPlayback(false);
            setStatusMessage("Playback finished.");
            return;
        }

        const int bar = playbackController.getCurrentBar();
        scoreRenderer.setCurrentBar(bar);
        scoreRenderer2.setCurrentBar(bar);
        scoreRenderer3.setCurrentBar(bar);
        transportLabel.setText("Bar " + juce::String(bar), juce::dontSendNotification);
        displayedBar = bar;

        if (!project.tracks.empty())
        {
            const auto namingOptions = getChordNamingOptions();
            const double elapsedSec = playbackController.getElapsedSeconds();
            const double quarter = project.tempoMap.secondsToQuarter(elapsedSec);
            const int eighthIndex = static_cast<int>(std::floor(quarter * 2.0 + 1.0e-6));
            const double windowQuarterStart = static_cast<double>(eighthIndex) / 2.0;
            const double windowQuarterEnd = windowQuarterStart + 0.5;
            const double windowSecStart = project.tempoMap.tickToSeconds(project.tempoMap.quarterToTick(windowQuarterStart));
            const double windowSecEnd = project.tempoMap.tickToSeconds(project.tempoMap.quarterToTick(windowQuarterEnd));

            auto updateLiveChordForStaff = [&](int staffIndex, ScoreRenderer& renderer)
            {
                auto& state = liveChordStates[(size_t) staffIndex];
                if (state.lastEighthIndex == eighthIndex)
                    return;
                state.lastEighthIndex = eighthIndex;

                const auto& notes = liveChordNotesByStaff[(size_t) staffIndex];
                const auto chord = ChordDetector::detectInWindow(notes, windowSecStart, windowSecEnd, namingOptions);
                if (chord.isEmpty())
                {
                    if (state.hasMarker)
                        renderer.setLiveChordMarker(false, state.markerBar, state.markerQuarterInBar, {});
                    state.lastDisplayedChord.clear();
                    state.hasMarker = false;
                    return;
                }

                const int markerBar = project.tempoMap.quarterToBar(windowQuarterStart);
                const double markerQuarterInBar = juce::jmax(0.0, windowQuarterStart - project.tempoMap.barToQuarterDownbeat(markerBar));
                const bool movedMarker = markerBar != state.markerBar
                    || std::abs(markerQuarterInBar - state.markerQuarterInBar) > 1.0e-6;
                if (chord == state.lastDisplayedChord && state.hasMarker && !movedMarker)
                    return;

                state.lastDisplayedChord = chord;
                state.hasMarker = true;
                state.markerBar = markerBar;
                state.markerQuarterInBar = markerQuarterInBar;
                renderer.setLiveChordMarker(true, state.markerBar, state.markerQuarterInBar, chord);
            };

            updateLiveChordForStaff(0, scoreRenderer);
            updateLiveChordForStaff(1, scoreRenderer2);
            updateLiveChordForStaff(2, scoreRenderer3);
        }

        refreshStatusMessage();
    }

    void loadMidiFile()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select a MIDI file",
            juce::File(),
            "*.mid;*.midi",
            false,
            false,
            this);

        const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (!file.existsAsFile())
                return;

            juce::String error;
            MidiProjectData loaded;
            if (!loader.load(file, loaded, error))
            {
                setStatusMessage("Load failed: " + error);
                updateWindowTitle();
                return;
            }

            project = std::move(loaded);
            updateWindowTitle();
            refreshTrackSelectors();
            refreshChordTrackButtons();
            playbackController.setTempoMap(&project.tempoMap, project.totalDurationSec);
            tempoOverrideBpm = std::nullopt;
            tempoOverrideInput.setText({}, juce::dontSendNotification);
            syncPlaybackTempoOverride();
            continueArmed = false;
            continueBarInput.setText("1", juce::dontSendNotification);
            resetLiveChordState();
            updateTransportControls();
            keyLabel.setText("Key: " + (keyOverride.has_value() ? keyOverride->displayText : project.getKeyDisplayText()),
                             juce::dontSendNotification);
            midiMetaLabel.setText(buildMidiMetaText(), juce::dontSendNotification);
            assignDefaultStaffSelections();
            const bool autoPresetLoaded = loadUiPreset(true);
            if (!autoPresetLoaded)
                rebuildAllStaffs();
            setStatusMessage(autoPresetLoaded
                ? ("Loaded: " + project.file.getFileName() + " (auto preset applied)")
                : ("Loaded: " + project.file.getFileName()));
        });
    }

    void refreshTrackSelectors()
    {
        staff1TrackSelector.clear(juce::dontSendNotification);
        staff2TrackSelector.clear(juce::dontSendNotification);
        staff3TrackSelector.clear(juce::dontSendNotification);
        int itemId = 1;
        for (const auto& track : project.tracks)
        {
            staff1TrackSelector.addItem(track.name, itemId);
            staff2TrackSelector.addItem(track.name, itemId);
            staff3TrackSelector.addItem(track.name, itemId);
            ++itemId;
        }
    }

    void assignDefaultStaffSelections()
    {
        const int trackCount = static_cast<int>(project.tracks.size());
        if (trackCount <= 0)
            return;

        staff1TrackSelector.setSelectedItemIndex(0, juce::dontSendNotification);
        staff2TrackSelector.setSelectedItemIndex(juce::jmin(1, trackCount - 1), juce::dontSendNotification);
        staff3TrackSelector.setSelectedItemIndex(juce::jmin(2, trackCount - 1), juce::dontSendNotification);
    }

    void clearStaff(ScoreModel& model, ScoreRenderer& renderer)
    {
        model.clear();
        renderer.setKeySignature(false, 0);
        renderer.setStaffLabel({});
        renderer.repaint();
    }

    void rebuildStaff(int staffIndex,
                      juce::ComboBox& selector,
                      juce::ComboBox& clefSelector,
                      ScoreModel& model,
                      ScoreRenderer& renderer)
    {
        const int index = selector.getSelectedItemIndex();
        if (index < 0 || index >= static_cast<int>(project.tracks.size()))
        {
            clearStaff(model, renderer);
            liveChordNotesByStaff[(size_t) staffIndex].clear();
            return;
        }

        const auto& track = project.tracks[(size_t) index];
        auto transposedNotes = track.notes;
        const auto clefType = getClefTypeFromCombo(clefSelector);
        const int transposeSemitones = getEffectiveTransposeSemitones(true);
        if (transposeSemitones != 0 && clefType != ScoreRenderer::ClefType::drum)
        {
            for (auto& note : transposedNotes)
                note.noteNumber = juce::jlimit(0, 127, note.noteNumber + transposeSemitones);
        }

        auto quantized = Quantizer::quantizeTrack(transposedNotes, project.tempoMap);
        const auto chordAnalysisNotes = collectChordAnalysisNotes(index);
        liveChordNotesByStaff[(size_t) staffIndex] = chordAnalysisNotes;
        const auto namingOptions = getChordNamingOptions();

        auto chords = ChordDetector::detect(chordAnalysisNotes, project.tempoMap, project.maxBar, namingOptions);
        model.build(project.tempoMap, quantized, chords, project.maxBar);
        renderer.setStaffLabel(track.name);
        renderer.setClefType(clefType);
        renderer.setCurrentBar(playbackController.getCurrentBar());
    }

    void rebuildAllStaffs()
    {
        if (project.tracks.empty())
        {
            clearStaff(scoreModel1, scoreRenderer);
            clearStaff(scoreModel2, scoreRenderer2);
            clearStaff(scoreModel3, scoreRenderer3);
            resetLiveChordState();
            setStatusMessage("Load a MIDI file to begin.");
            return;
        }

        const bool useOverride = keyOverride.has_value();
        const bool hasKeySignature = useOverride ? true : project.hasKeySignature;
        const int keySharpsOrFlats = useOverride ? keyOverride->sharpsOrFlats : project.keySharpsOrFlats;
        scoreRenderer.setKeySignature(hasKeySignature, keySharpsOrFlats);
        scoreRenderer2.setKeySignature(hasKeySignature, keySharpsOrFlats);
        scoreRenderer3.setKeySignature(hasKeySignature, keySharpsOrFlats);

        rebuildStaff(0, staff1TrackSelector, staff1ClefSelector, scoreModel1, scoreRenderer);
        rebuildStaff(1, staff2TrackSelector, staff2ClefSelector, scoreModel2, scoreRenderer2);
        rebuildStaff(2, staff3TrackSelector, staff3ClefSelector, scoreModel3, scoreRenderer3);
        resetLiveChordState();

        const int bar = playbackController.isPlaying() ? playbackController.getCurrentBar() : 1;
        scoreRenderer.setCurrentBar(bar);
        scoreRenderer2.setCurrentBar(bar);
        scoreRenderer3.setCurrentBar(bar);
        transportLabel.setText("Bar " + juce::String(bar), juce::dontSendNotification);
        displayedBar = bar;

        int selectedTrackCount = 0;
        for (auto* button : chordTrackButtons)
            selectedTrackCount += button->getToggleState() ? 1 : 0;
        keyLabel.setText("Key: " + (keyOverride.has_value() ? keyOverride->displayText : project.getKeyDisplayText()),
                         juce::dontSendNotification);
        midiMetaLabel.setText(buildMidiMetaText(), juce::dontSendNotification);
        setStatusMessage("Staffs ready  | Chords from " + juce::String(selectedTrackCount) + " selected tracks");
    }

    void applyScoreColorScheme()
    {
        const auto scheme = scoreColorToggle.getToggleState()
            ? ScoreRenderer::ColorScheme::light
            : ScoreRenderer::ColorScheme::dark;
        scoreRenderer.setColorScheme(scheme);
        scoreRenderer2.setColorScheme(scheme);
        scoreRenderer3.setColorScheme(scheme);
    }

    static void showInfoModal(juce::Component* parent, const juce::String& title, const juce::String& message)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            title,
            message,
            "OK",
            parent);
    }

    void showTransposeHelpModal()
    {
        showInfoModal(this,
                      "Transpose Help",
                      "Enter semitones from -24 to +24 to shift all staffs and chord analysis.\n\n"
                      "Examples:\n"
                      "  3  moves C up to Eb\n"
                      " -3  moves C down to A\n\n"
                      "Drum clef staffs and GM channel 10 percussion are not transposed.\n"
                      "This value stacks with the Key override.");
    }

    void showKeyHelpModal()
    {
        showInfoModal(this,
                      "Key Override Help",
                      "Override the song key for notation and transposition.\n\n"
                      "Examples: C, Eb, F#, Cm, Ebm\n"
                      "Use b for flats (Eb, Bb) and # for sharps (F#).\n"
                      "Add m for minor keys.\n\n"
                      "Leave blank to use the key from the MIDI file.\n"
                      "When set, the app transposes by the difference between the MIDI key and your override.");
    }

    void showTempoHelpModal()
    {
        showInfoModal(this,
                      "Tempo Override Help",
                      "Enter a BPM value from 10 to 400 to override playback tempo.\n\n"
                      "Leave blank to use the tempo map from the MIDI file.\n"
                      "Per-song tempo overrides are saved in your preset profile.");
    }

    void showAccidentalHelpModal()
    {
        showInfoModal(this,
                      "Accidental Names Help",
                      "Choose whether chords and accidentals prefer sharp or flat spelling.\n\n"
                      "This affects display only (for example D# vs Eb).\n"
                      "It does not change pitch or transposition.");
    }

    void startPlayback()
    {
        if (project.tracks.empty())
            return;

        playbackController.playFromStart();
        continueArmed = false;
        resetLiveChordState();
        scoreRenderer.setCurrentBar(1);
        scoreRenderer2.setCurrentBar(1);
        scoreRenderer3.setCurrentBar(1);
        transportLabel.setText("Bar 1", juce::dontSendNotification);
        displayedBar = 1;
        updateTransportControls();
        setStatusMessage(tempoOverrideBpm.has_value()
            ? ("Playback running (tempo override " + formatTempoBpm(tempoOverrideBpm.value()) + ").")
            : juce::String("Playback running (tempo mapped from MIDI)."));
    }

    void continuePlaybackFromBar()
    {
        if (project.tracks.empty() || !continueArmed)
            return;

        const int requestedBar = continueBarInput.getText().trim().getIntValue();
        const int clampedBar = juce::jlimit(1, juce::jmax(1, project.maxBar), juce::jmax(1, requestedBar));
        continueBarInput.setText(juce::String(clampedBar), juce::dontSendNotification);

        playbackController.playFromBar(clampedBar);
        continueArmed = false;
        resetLiveChordState();
        scoreRenderer.setCurrentBar(clampedBar);
        scoreRenderer2.setCurrentBar(clampedBar);
        scoreRenderer3.setCurrentBar(clampedBar);
        transportLabel.setText("Bar " + juce::String(clampedBar), juce::dontSendNotification);
        displayedBar = clampedBar;
        updateTransportControls();
        setStatusMessage("Playback continuing from bar " + juce::String(clampedBar) + ".");
    }

    void stopPlayback(bool userInitiated)
    {
        if (userInitiated && !project.tracks.empty())
        {
            const int currentBar = juce::jmax(1, playbackController.getCurrentBar());
            continueBarInput.setText(juce::String(currentBar), juce::dontSendNotification);
        }

        playbackController.stop();
        continueArmed = userInitiated && !project.tracks.empty();
        updateTransportControls();
    }

    void onTransportToggleClicked()
    {
        if (playbackController.isPlaying())
            stopPlayback(true);
        else
            startPlayback();
    }

    void refreshTransportToggleButtonText()
    {
        transportToggleButton.setButtonText(playbackController.isPlaying() ? "Stop" : "Start");
    }

    void updateTransportControls()
    {
        const bool hasProject = !project.tracks.empty();
        const bool isPlaying = playbackController.isPlaying();
        transportToggleButton.setEnabled(hasProject);
        refreshTransportToggleButtonText();
        continueButton.setEnabled(hasProject && !isPlaying && continueArmed);
        continueBarInput.setEnabled(hasProject && !isPlaying && continueArmed);
    }

    juce::String buildMidiMetaText() const
    {
        if (project.tracks.empty())
            return "Sig: n/a  Tempo: n/a";

        const auto& sigs = project.tempoMap.getTimeSignatureEvents();
        const auto& tempos = project.tempoMap.getTempoEvents();

        juce::String signatureText = "n/a";
        if (!sigs.empty())
            signatureText = juce::String(sigs.front().numerator) + "/" + juce::String(sigs.front().denominator);

        juce::String tempoText = "n/a";
        if (!tempos.empty())
        {
            const double detectedBpm = tempos.front().bpm;
            tempoText = formatTempoBpm(detectedBpm);
            if (tempoOverrideBpm.has_value())
                tempoText = formatTempoBpm(tempoOverrideBpm.value()) + " (detected " + formatTempoBpm(detectedBpm) + ")";
        }

        return "Sig: " + signatureText + "  Tempo: " + tempoText;
    }

    juce::String buildStatusDetailsText() const
    {
        const auto keyText = keyOverride.has_value()
            ? ("KeySrc: override (" + keyOverride->displayText + ")")
            : ("KeySrc: detected (" + project.getKeyDisplayText() + ")");
        const auto barText = project.tracks.empty()
            ? juce::String("Bar: n/a")
            : ("Bar: " + juce::String(juce::jmax(1, displayedBar)) + "/" + juce::String(juce::jmax(1, project.maxBar)));
        return buildMidiMetaText() + "  | " + keyText + "  | " + barText;
    }

    void setStatusMessage(const juce::String& message)
    {
        statusMessageBase = message;
        refreshStatusMessage();
    }

    void refreshStatusMessage()
    {
        const auto prefix = statusMessageBase.isNotEmpty() ? statusMessageBase : juce::String("Ready");
        statusLabel.setText(prefix + "  | " + buildStatusDetailsText(), juce::dontSendNotification);
    }

    juce::Label titleLabel;
    juce::TextButton loadButton;
    juce::Label staff1TrackLabel;
    juce::ComboBox staff1TrackSelector;
    juce::ComboBox staff1ClefSelector;
    juce::Label staff2TrackLabel;
    juce::ComboBox staff2TrackSelector;
    juce::ComboBox staff2ClefSelector;
    juce::Label staff3TrackLabel;
    juce::ComboBox staff3TrackSelector;
    juce::ComboBox staff3ClefSelector;
    juce::TextButton transportToggleButton;
    juce::ComboBox accidentalSelector;
    juce::TextButton accidentalHelpButton;
    juce::ComboBox aliasSelector;
    juce::Label chordTracksLabel;
    juce::OwnedArray<juce::ToggleButton> chordTrackButtons;
    juce::ToggleButton scoreColorToggle;
    juce::Label globalTransposeLabel;
    juce::TextEditor globalTransposeInput;
    juce::TextButton transposeHelpButton;
    juce::TextButton continueButton;
    juce::Label continueBarLabel;
    juce::TextEditor continueBarInput;
    juce::TextButton savePresetButton;
    juce::TextButton loadPresetButton;
    juce::Label statusLabel;
    juce::Label tempoOverrideLabel;
    juce::TextEditor tempoOverrideInput;
    juce::TextButton tempoHelpButton;
    juce::Label keyOverrideLabel;
    juce::TextEditor keyOverrideInput;
    juce::TextButton keyHelpButton;
    juce::Label transportLabel;
    juce::Label keyLabel;
    juce::Label midiMetaLabel;
    ScoreRenderer scoreRenderer;
    ScoreRenderer scoreRenderer2;
    ScoreRenderer scoreRenderer3;

    MidiProjectLoader loader;
    MidiProjectData project;
    ScoreModel scoreModel1;
    ScoreModel scoreModel2;
    ScoreModel scoreModel3;
    PlaybackController playbackController;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool continueArmed = false;
    int displayedBar = 1;
    juce::String statusMessageBase;
    std::optional<double> tempoOverrideBpm;
    std::optional<ParsedKey> keyOverride;
    std::array<std::vector<MidiNoteEvent>, 3> liveChordNotesByStaff;
    std::array<LiveChordState, 3> liveChordStates;
};
