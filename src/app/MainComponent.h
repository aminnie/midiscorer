#pragma once

#include <JuceHeader.h>
#include "StaffDisplayOctaveSelector.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <vector>
#include "../midi/MidiProjectLoader.h"
#include "../notation/Quantizer.h"
#include "../notation/ScoreModel.h"
#include "../notation/ScoreRenderer.h"
#include "../harmony/ChordDetector.h"
#include "../playback/PlaybackController.h"
#include "../playback/MidiFilePlaybackEngineAdapter.h"
#include "../playback/MidiOutputDevice.h"
#include "../playback/TrackMixProcessor.h"
#include "../playback/TrackMixState.h"
#include "../playback/TrackMixMidiSeed.h"
#include "PresetFileStore.h"
#include "KeyOverrideTranspose.h"
#include "ScorePdfExporter.h"
#include "ScoreRebuildService.h"
#include "TransportCoordinator.h"
#include "WorkingDirectoryCopy.h"

class MainComponent final : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            private juce::Timer
{
public:
    MainComponent()
    {
        addAndMakeVisible(loadButton);
        loadButton.setButtonText("Load MIDI");
        loadButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        loadButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8aa3b6));
        loadButton.setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(0xff8aa3b6).interpolatedWith(juce::Colours::black, 0.10f));
        loadButton.onClick = [this] { loadMidiFile(); };

        addAndMakeVisible(staff1TrackLabel);
        staff1TrackLabel.setText("Staff 1", juce::dontSendNotification);
        staff1TrackLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(staff1TrackSelector);
        staff1TrackSelector.onChange = [this] { rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
        addAndMakeVisible(staff1ClefSelector);
        staff1ClefSelector.addItem("Treble", 1);
        staff1ClefSelector.addItem("Bass", 2);
        staff1ClefSelector.addItem("Drum", 3);
        staff1ClefSelector.setSelectedId(1, juce::dontSendNotification);
        staff1ClefSelector.onChange = [this]
        {
            rebuildAllStaffs();
            applyStaffDisplayOctaveFromSelectors(true);
            refreshSavePresetButtonDirtyStyle();
        };
        addAndMakeVisible(staff1OctaveSelector);
        staff1OctaveSelector.onChange = [this]
        {
            applyStaffDisplayOctaveFromSelectors(true);
            refreshSavePresetButtonDirtyStyle();
        };
        staff1OctaveSelector.setTooltip("Display-only octave shift for Staff 1. MIDI playback is unchanged.");

        addAndMakeVisible(staff2TrackLabel);
        staff2TrackLabel.setText("Staff 2 ", juce::dontSendNotification);
        staff2TrackLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(staff2TrackSelector);
        staff2TrackSelector.onChange = [this] { rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
        addAndMakeVisible(staff2ClefSelector);
        staff2ClefSelector.addItem("Treble", 1);
        staff2ClefSelector.addItem("Bass", 2);
        staff2ClefSelector.addItem("Drum", 3);
        staff2ClefSelector.setSelectedId(1, juce::dontSendNotification);
        staff2ClefSelector.onChange = [this]
        {
            rebuildAllStaffs();
            applyStaffDisplayOctaveFromSelectors(true);
            refreshSavePresetButtonDirtyStyle();
        };
        addAndMakeVisible(staff2OctaveSelector);
        staff2OctaveSelector.onChange = [this]
        {
            applyStaffDisplayOctaveFromSelectors(true);
            refreshSavePresetButtonDirtyStyle();
        };
        staff2OctaveSelector.setTooltip("Display-only octave shift for Staff 2. MIDI playback is unchanged.");

        addAndMakeVisible(staff3TrackLabel);
        staff3TrackLabel.setText("Staff 3 ", juce::dontSendNotification);
        staff3TrackLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(staff3TrackSelector);
        staff3TrackSelector.onChange = [this] { rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
        addAndMakeVisible(staff3ClefSelector);
        staff3ClefSelector.addItem("Treble", 1);
        staff3ClefSelector.addItem("Bass", 2);
        staff3ClefSelector.addItem("Drum", 3);
        staff3ClefSelector.setSelectedId(1, juce::dontSendNotification);
        staff3ClefSelector.onChange = [this]
        {
            rebuildAllStaffs();
            applyStaffDisplayOctaveFromSelectors(true);
            refreshSavePresetButtonDirtyStyle();
        };
        addAndMakeVisible(staff3OctaveSelector);
        staff3OctaveSelector.onChange = [this]
        {
            applyStaffDisplayOctaveFromSelectors(true);
            refreshSavePresetButtonDirtyStyle();
        };
        staff3OctaveSelector.setTooltip("Display-only octave shift for Staff 3. MIDI playback is unchanged.");

        addAndMakeVisible(transportToggleButton);
        transportToggleButton.setButtonText("Start");
        transportToggleButton.onClick = [this] { onTransportToggleClicked(); };

        addAndMakeVisible(hideScoreControlsButton);
        hideScoreControlsButton.setTooltip("Hide/Unhide score controls");
        hideScoreControlsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8aa3b6));
        hideScoreControlsButton.setColour(juce::TextButton::buttonOnColourId,
                                          juce::Colour(0xff8aa3b6).interpolatedWith(juce::Colours::black, 0.10f));
        hideScoreControlsButton.setDirection(HideControlArrowButton::Direction::down);
        hideScoreControlsButton.onClick = [this] { setScoreControlRowsHidden(!scoreControlRowsHidden); };

        addAndMakeVisible(accidentalLabel);
        accidentalLabel.setText("Names", juce::dontSendNotification);
        accidentalLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(accidentalSelector);
        accidentalSelector.addItem("Sharp", 1);
        accidentalSelector.addItem("Flat", 2);
        accidentalSelector.setSelectedId(1, juce::dontSendNotification);
        accidentalSelector.onChange = [this] { rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
        addAndMakeVisible(accidentalHelpButton);
        accidentalHelpButton.setButtonText("?");
        accidentalHelpButton.onClick = [this] { showAccidentalHelpModal(); };

        addAndMakeVisible(aliasLabel);
        aliasLabel.setText("Text", juce::dontSendNotification);
        aliasLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(aliasSelector);
        aliasSelector.addItem("Plain", 1);
        aliasSelector.addItem("Jazz", 2);
        aliasSelector.setSelectedId(1, juce::dontSendNotification);
        aliasSelector.onChange = [this] { rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };

        addAndMakeVisible(chordResolutionLabel);
        chordResolutionLabel.setText("Grid", juce::dontSendNotification);
        chordResolutionLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(chordResolutionSelector);
        chordResolutionSelector.addItem("1/4", static_cast<int>(ChordDetector::DetectionResolution::quarter));
        chordResolutionSelector.addItem("1/8", static_cast<int>(ChordDetector::DetectionResolution::eighth));
        chordResolutionSelector.setSelectedId(static_cast<int>(ChordDetector::DetectionResolution::quarter), juce::dontSendNotification);
        chordResolutionSelector.setTooltip("Choose chord detection grid size. Quarter is steadier for passing tones; eighth is more reactive.");
        chordResolutionSelector.onChange = [this]
        {
            resetLiveChordState();
            rebuildAllStaffs();
            refreshSavePresetButtonDirtyStyle();
        };

        addAndMakeVisible(chordComplexityLabel);
        chordComplexityLabel.setText("Level", juce::dontSendNotification);
        chordComplexityLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(autoChordLabel);
        autoChordLabel.setText("Auto Chords:", juce::dontSendNotification);
        autoChordLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(chordComplexitySelector);
        chordComplexitySelector.addItem("Simple", static_cast<int>(ChordDetector::ChordComplexity::simple));
        chordComplexitySelector.addItem("Standard", static_cast<int>(ChordDetector::ChordComplexity::standard));
        chordComplexitySelector.addItem("Rich", static_cast<int>(ChordDetector::ChordComplexity::rich));
        chordComplexitySelector.setSelectedId(static_cast<int>(ChordDetector::ChordComplexity::rich), juce::dontSendNotification);
        chordComplexitySelector.setTooltip("Choose the chord vocabulary complexity used for chord naming.");
        chordComplexitySelector.onChange = [this]
        {
            resetLiveChordState();
            rebuildAllStaffs();
            refreshSavePresetButtonDirtyStyle();
        };

        addAndMakeVisible(chordTracksLabel);
        chordTracksLabel.setText("Chord Tracks:", juce::dontSendNotification);
        chordTracksLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(globalTransposeLabel);
        globalTransposeLabel.setText("Transpose", juce::dontSendNotification);
        globalTransposeLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(globalTransposeInput);
        globalTransposeInput.setInputRestrictions(3, "-0123456789");
        globalTransposeInput.setText("0", juce::dontSendNotification);
        globalTransposeInput.onReturnKey = [this] { rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
        globalTransposeInput.onFocusLost = [this] { rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
        addAndMakeVisible(transposeHelpButton);
        transposeHelpButton.setButtonText("?");
        transposeHelpButton.onClick = [this] { showTransposeHelpModal(); };

        addAndMakeVisible(continueButton);
        continueButton.setButtonText("Continue");
        continueButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        continueButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        continueButton.setColour(juce::TextButton::buttonColourId, juce::Colours::antiquewhite);
        continueButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lightgrey);
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

        addAndMakeVisible(loopEnabledToggle);
        loopEnabledToggle.setButtonText("Loop");
        loopEnabledToggle.onClick = [this]
        {
            loopEnabled = loopEnabledToggle.getToggleState();
            saveUiPreset(false);
            refreshStatusMessage();
        };

        addAndMakeVisible(loopStartLabel);
        loopStartLabel.setText("A", juce::dontSendNotification);
        loopStartLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(loopStartInput);
        loopStartInput.setInputRestrictions(5, "0123456789");
        loopStartInput.setText("1", juce::dontSendNotification);
        loopStartInput.setJustification(juce::Justification::centred);
        loopStartInput.onReturnKey = [this] { applyLoopBoundsFromInputs(); };
        loopStartInput.onFocusLost = [this] { applyLoopBoundsFromInputs(); };

        addAndMakeVisible(loopEndLabel);
        loopEndLabel.setText("B", juce::dontSendNotification);
        loopEndLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(loopEndInput);
        loopEndInput.setInputRestrictions(5, "0123456789");
        loopEndInput.setText("1", juce::dontSendNotification);
        loopEndInput.setJustification(juce::Justification::centred);
        loopEndInput.onReturnKey = [this] { applyLoopBoundsFromInputs(); };
        loopEndInput.onFocusLost = [this] { applyLoopBoundsFromInputs(); };

        addAndMakeVisible(openRecentButton);
        openRecentButton.setButtonText("Load Recent");
        openRecentButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        openRecentButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        openRecentButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8aa3b6));
        openRecentButton.setColour(juce::TextButton::buttonOnColourId,
                                   juce::Colour(0xff8aa3b6).interpolatedWith(juce::Colours::black, 0.10f));
        openRecentButton.onClick = [this] { openRecentFileList(); };

        addAndMakeVisible(savePresetButton);
        savePresetButton.setButtonText("Save Preset");
        savePresetButton.onClick = [this] { saveUiPreset(); };
        applySavePresetButtonCleanStyle();

        addAndMakeVisible(loadPresetButton);
        loadPresetButton.setButtonText("Load Preset");
        loadPresetButton.onClick = [this] { (void) loadUiPreset(); };

        addAndMakeVisible(exportPdfButton);
        exportPdfButton.setButtonText("Export PDF");
        exportPdfButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        exportPdfButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        exportPdfButton.setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
        exportPdfButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::antiquewhite);
        exportPdfButton.onClick = [this] { exportScorePdf(); };

        addAndMakeVisible(exportPdfModeSelector);
        exportPdfModeSelector.addItem("All", static_cast<int>(PdfExportMode::allActiveStaffs));
        exportPdfModeSelector.addItem("First", static_cast<int>(PdfExportMode::staff1Only));
        exportPdfModeSelector.setSelectedId(static_cast<int>(PdfExportMode::allActiveStaffs), juce::dontSendNotification);
        exportPdfModeSelector.onChange = [this]
        {
            saveUiPreset(false);
            refreshSavePresetButtonDirtyStyle();
        };
        exportPdfModeSelector.setTooltip("Select whether PDF export includes all staffs or only the first staff.");

        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(assignLabel);
        assignLabel.setText("Assign", juce::dontSendNotification);
        assignLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(assignToggle);
        assignToggle.setButtonText({});
        assignToggle.setToggleState(true, juce::dontSendNotification);
        assignToggle.onClick = [this]
        {
            if (assignToggle.getToggleState())
            {
                keyOverrideProfileOnly = keyOverride.has_value();
                suppressProfileOnlyExitUntilKeyChange = false;
                syncKeyTransposeReferenceToOverrideOrMidi();
                keyTransposeAppliedSemitones = 0;
                rebuildAllStaffs();
            }
            else if (keyOverrideProfileOnly)
            {
                // Do not immediately exit profile-only mode on uncheck.
                // Wait until the user actually changes the key text.
                suppressProfileOnlyExitUntilKeyChange = true;
            }
            refreshSavePresetButtonDirtyStyle();
        };

        addAndMakeVisible(keyOverrideLabel);
        keyOverrideLabel.setText("Key:", juce::dontSendNotification);
        keyOverrideLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(keyOverrideInput);
        keyOverrideInput.setInputRestrictions(6, "ABCDEFGabcdefg#bBmMnNiI");
        keyOverrideInput.setText({}, juce::dontSendNotification);
        keyOverrideInput.onReturnKey = [this] { applyKeyOverrideFromInput(true); rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
        keyOverrideInput.onFocusLost = [this] { applyKeyOverrideFromInput(true); rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
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
        tempoOverrideInput.onReturnKey = [this] { applyTempoOverrideFromInput(true); refreshSavePresetButtonDirtyStyle(); };
        tempoOverrideInput.onFocusLost = [this] { applyTempoOverrideFromInput(true); refreshSavePresetButtonDirtyStyle(); };
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
        scoreRenderer.setBarClickCallback([this](int bar)
        {
            if (playbackController.isPlaying())
                pausePlayback();
            seekToBarExternal(bar);
        });
        addAndMakeVisible(scoreRenderer2);
        scoreRenderer2.setScoreModel(&scoreModel2);
        scoreRenderer2.setClefType(ScoreRenderer::ClefType::treble);
        scoreRenderer2.setBarClickCallback([this](int bar)
        {
            if (playbackController.isPlaying())
                pausePlayback();
            seekToBarExternal(bar);
        });
        addAndMakeVisible(scoreRenderer3);
        scoreRenderer3.setScoreModel(&scoreModel3);
        scoreRenderer3.setClefType(ScoreRenderer::ClefType::treble);
        scoreRenderer3.setBarClickCallback([this](int bar)
        {
            if (playbackController.isPlaying())
                pausePlayback();
            seekToBarExternal(bar);
        });
        applyStaffDisplayOctaveFromSelectors(true);
        applyScoreColorScheme();
        setStatusMessage("Load a MIDI file to begin.");
        loadLastMidiDirectoryFromPreset();
        loadWorkingDirectoryFromPreset();
        loadRecentFilesFromPreset();
        loadStartupSettingsFromPreset();
        ensureWorkingDirectoryInitialized();
        refreshRecentFilesButtonState();
        loadSelectedMidiOutputFromConfig();

        updateTransportControls();
        applyScoreControlRowsVisibility();
        refreshHideScoreControlsButtonText();
        setSize(1280, 720);
        startTimerHz(30);
    }

    ~MainComponent() override = default;

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        auto row = area.removeFromTop(34);

        loadButton.setBounds(row.removeFromLeft(108).reduced(4, 0));
        transportToggleButton.setBounds(row.removeFromLeft(92).reduced(4, 0));
        continueButton.setBounds(row.removeFromLeft(100).reduced(4, 0));
        continueBarLabel.setBounds(row.removeFromLeft(34));
        continueBarInput.setBounds(row.removeFromLeft(64).reduced(4, 0));
        loopEnabledToggle.setBounds(row.removeFromLeft(64).reduced(4, 0));
        loopStartLabel.setBounds(row.removeFromLeft(18));
        loopStartInput.setBounds(row.removeFromLeft(44).reduced(4, 0));
        loopEndLabel.setBounds(row.removeFromLeft(18));
        loopEndInput.setBounds(row.removeFromLeft(44).reduced(4, 0));
        savePresetButton.setBounds(row.removeFromLeft(100).reduced(4, 0));
        loadPresetButton.setBounds(row.removeFromLeft(100).reduced(4, 0));
        exportPdfButton.setBounds(row.removeFromLeft(108).reduced(4, 0));
        row.removeFromLeft(juce::jmin(4, row.getWidth()));
        exportPdfModeSelector.setBounds(row.removeFromLeft(juce::jmin(72, row.getWidth())).reduced(4, 0));
        openRecentButton.setBounds(row.removeFromLeft(126).reduced(4, 0));
        hideScoreControlsButton.setBounds(row.removeFromRight(96).reduced(4, 0));
        chordTracksLabel.setBounds(row);

        if (!scoreControlRowsHidden)
        {
            area.removeFromTop(6);
            auto selectorRow = area.removeFromTop(26);
            constexpr int staffSectionWidth = 378;
            constexpr int staffSectionGap = 6;
            auto section1 = selectorRow.removeFromLeft(juce::jmin(staffSectionWidth, selectorRow.getWidth()));
            selectorRow.removeFromLeft(juce::jmin(staffSectionGap, selectorRow.getWidth()));
            auto section2 = selectorRow.removeFromLeft(juce::jmin(staffSectionWidth, selectorRow.getWidth()));
            selectorRow.removeFromLeft(juce::jmin(staffSectionGap, selectorRow.getWidth()));
            auto section3 = selectorRow.removeFromLeft(juce::jmin(staffSectionWidth, selectorRow.getWidth()));
            layoutStaffControls(section1, staff1TrackLabel, staff1TrackSelector, staff1OctaveSelector, staff1ClefSelector);
            layoutStaffControls(section2, staff2TrackLabel, staff2TrackSelector, staff2OctaveSelector, staff2ClefSelector);
            layoutStaffControls(section3, staff3TrackLabel, staff3TrackSelector, staff3OctaveSelector, staff3ClefSelector);
            constexpr int staff2TrackSelectorShift = 8;
            constexpr int staff3TrackControlsShift = 10;
            staff2TrackLabel.setBounds(staff2TrackLabel.getBounds().translated(6, 0));
            staff2TrackSelector.setBounds(staff2TrackSelector.getBounds().translated(staff2TrackSelectorShift, 0));
            staff3TrackLabel.setBounds(staff3TrackLabel.getBounds().translated(staff3TrackControlsShift, 0));
            staff3TrackSelector.setBounds(staff3TrackSelector.getBounds().translated(staff3TrackControlsShift, 0));

            area.removeFromTop(6);
            auto chordTracksArea = area.removeFromTop(getChordTracksLayoutHeight(area.getWidth()));
            layoutChordTrackButtons(chordTracksArea);

            area.removeFromTop(6);
            auto statusRow = area.removeFromTop(24);
            tempoOverrideLabel.setBounds(statusRow.removeFromLeft(50));
            tempoOverrideInput.setBounds(statusRow.removeFromLeft(72).reduced(4, 0));
            tempoHelpButton.setBounds(statusRow.removeFromLeft(28).reduced(4, 0));
            assignLabel.setBounds(statusRow.removeFromLeft(48));
            assignToggle.setBounds(statusRow.removeFromLeft(30).reduced(4, 0));
            keyOverrideLabel.setBounds(statusRow.removeFromLeft(36));
            keyOverrideInput.setBounds(statusRow.removeFromLeft(68).reduced(4, 0));
            keyHelpButton.setBounds(statusRow.removeFromLeft(28).reduced(4, 0));
            globalTransposeLabel.setBounds(statusRow.removeFromLeft(74));
            globalTransposeInput.setBounds(statusRow.removeFromLeft(48).reduced(4, 0));
            transposeHelpButton.setBounds(statusRow.removeFromLeft(28).reduced(4, 0));
            statusLabel.setBounds(statusRow);
        }
        area.removeFromTop(8);
        const bool showStaff1 = isStaffDisplayed(staff1TrackSelector);
        const bool showStaff2 = isStaffDisplayed(staff2TrackSelector);
        const bool showStaff3 = isStaffDisplayed(staff3TrackSelector);
        scoreRenderer.setVisible(showStaff1);
        scoreRenderer2.setVisible(showStaff2);
        scoreRenderer3.setVisible(showStaff3);

        std::vector<ScoreRenderer*> activeRenderers;
        activeRenderers.reserve(3);
        if (showStaff1)
            activeRenderers.push_back(&scoreRenderer);
        if (showStaff2)
            activeRenderers.push_back(&scoreRenderer2);
        if (showStaff3)
            activeRenderers.push_back(&scoreRenderer3);

        if (activeRenderers.empty())
        {
            scoreRenderer.setBounds(juce::Rectangle<int>());
            scoreRenderer2.setBounds(juce::Rectangle<int>());
            scoreRenderer3.setBounds(juce::Rectangle<int>());
            return;
        }

        const int laneGap = 4;
        const int laneCount = static_cast<int>(activeRenderers.size());
        const int totalGap = laneGap * juce::jmax(0, laneCount - 1);
        const int dynamicLaneHeight = juce::jmax(1, (area.getHeight() - totalGap) / laneCount);
        const int twoStaffHeight = juce::jmax(1, (area.getHeight() - laneGap) / 2);
        const int laneHeight = juce::jmin(dynamicLaneHeight, twoStaffHeight);
        const int usedHeight = laneHeight * laneCount + totalGap;
        int y = area.getY() + juce::jmax(0, (area.getHeight() - usedHeight) / 2);
        for (int i = 0; i < laneCount; ++i)
        {
            activeRenderers[(size_t) i]->setBounds(area.getX(), y, area.getWidth(), laneHeight);
            y += laneHeight + laneGap;
        }

        if (!showStaff1)
            scoreRenderer.setBounds(juce::Rectangle<int>());
        if (!showStaff2)
            scoreRenderer2.setBounds(juce::Rectangle<int>());
        if (!showStaff3)
            scoreRenderer3.setBounds(juce::Rectangle<int>());
    }

    bool hasLoadedProject() const
    {
        return !project.tracks.empty();
    }

    bool isPlaybackRunning() const
    {
        return playbackPositionSource != nullptr && playbackPositionSource->isPlaying();
    }

    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        return findFirstMidiFile(files).existsAsFile();
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        const auto file = findFirstMidiFile(files);
        if (!file.existsAsFile())
            return;

        loadMidiFileFromPath(file);
    }

    int getCurrentPlaybackBar() const
    {
        return playbackPositionSource != nullptr ? playbackPositionSource->getCurrentBar() : 1;
    }

    int getMaximumBar() const
    {
        return juce::jmax(1, project.maxBar);
    }

    juce::String getLoadedMidiFileName() const
    {
        return project.file.existsAsFile() ? project.file.getFileName() : juce::String();
    }

    juce::String getLoadedMidiFilePath() const
    {
        return project.file.existsAsFile() ? project.file.getFullPathName() : juce::String();
    }

    bool isLoadedMidiInWorkingDirectory() const
    {
        if (!project.file.existsAsFile() || !workingDirectory.isDirectory())
            return false;
        return project.file.getParentDirectory().getFullPathName().equalsIgnoreCase(workingDirectory.getFullPathName());
    }

    bool isStartupResumeEnabled() const
    {
        return startupResumeEnabled;
    }

    void setStartupResumeEnabled(bool enabled)
    {
        if (startupResumeEnabled == enabled)
            return;

        startupResumeEnabled = enabled;
        saveStartupResumeEnabledToPreset();
    }

    juce::String getWorkingDirectoryPath() const
    {
        return workingDirectory.isDirectory() ? workingDirectory.getFullPathName() : juce::String();
    }

    bool setWorkingDirectoryPath(const juce::String& path, juce::String& error)
    {
        const auto trimmed = path.trim();
        if (trimmed.isEmpty())
        {
            error = "Working directory path cannot be empty.";
            return false;
        }

        juce::File candidate(trimmed);
        if (candidate.existsAsFile())
        {
            error = "Working directory path points to a file.";
            return false;
        }

        if (!candidate.exists())
        {
            if (!candidate.createDirectory())
            {
                error = "Could not create working directory.";
                return false;
            }
        }

        if (!candidate.isDirectory())
        {
            error = "Working directory is invalid.";
            return false;
        }

        workingDirectory = candidate;
        saveWorkingDirectoryToPreset();
        return true;
    }

    bool isScoreLightMode() const
    {
        return scoreLightMode;
    }

    void setScoreLightMode(bool light)
    {
        if (scoreLightMode == light)
            return;

        scoreLightMode = light;
        applyScoreColorScheme();
        refreshSavePresetButtonDirtyStyle();
    }

    void runStartupResumeIfEnabled()
    {
        if (!startupResumeEnabled)
            return;

        juce::File resumeFile(lastLoadedMidiPath);
        if (!resumeFile.existsAsFile() && !recentMidiFiles.empty())
            resumeFile = juce::File(recentMidiFiles.front());

        if (!resumeFile.existsAsFile())
        {
            setStatusMessage("Startup resume enabled but no valid last/recent MIDI file was found.");
            return;
        }

        loadMidiFileFromPath(resumeFile, true);

        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis]()
        {
            if (safeThis != nullptr)
                safeThis->updateWindowTitle();
        });
    }

    enum class TrackMixSaveStatus
    {
        saved,
        pending,
        failed
    };

    int getTrackMixTrackCount() const
    {
        return static_cast<int>(project.tracks.size());
    }

    bool isTrackMixEligible(int trackIndex) const
    {
        if (trackIndex < 0 || trackIndex >= static_cast<int>(project.tracks.size()))
            return false;
        return trackHasChordSourceContent(project.tracks[(size_t) trackIndex]);
    }

    juce::String getTrackDisplayName(int trackIndex) const
    {
        if (trackIndex < 0 || trackIndex >= static_cast<int>(project.tracks.size()))
            return {};
        return project.tracks[(size_t) trackIndex].name;
    }

    int getTrackMixVolume(int trackIndex) const
    {
        return trackMixState.getVolume(trackIndex);
    }

    int getTrackMixReverb(int trackIndex) const
    {
        return trackMixState.getReverb(trackIndex);
    }

    int getTrackMixExpression(int trackIndex) const
    {
        return trackMixState.getExpression(trackIndex);
    }

    int getTrackMixChannel(int trackIndex) const
    {
        return trackMixState.getChannel(trackIndex);
    }

    bool isTrackMuted(int trackIndex) const
    {
        return trackMixState.isMuted(trackIndex);
    }

    bool isTrackSolo(int trackIndex) const
    {
        return trackMixState.isSolo(trackIndex);
    }

    void setTrackMixVolume(int trackIndex, int volume)
    {
        if (!trackMixState.isValidTrack(trackIndex))
            return;
        trackMixState.setVolume(trackIndex, volume);
        onTrackMixStateChanged();
    }

    void setTrackMixReverb(int trackIndex, int reverb)
    {
        if (!trackMixState.isValidTrack(trackIndex))
            return;
        trackMixState.setReverb(trackIndex, reverb);
        onTrackMixStateChanged();
    }

    void setTrackMixExpression(int trackIndex, int expression)
    {
        if (!trackMixState.isValidTrack(trackIndex))
            return;
        trackMixState.setExpression(trackIndex, expression);
        onTrackMixStateChanged();
    }

    void setTrackMixChannel(int trackIndex, int channel)
    {
        if (!trackMixState.isValidTrack(trackIndex))
            return;
        trackMixState.setChannel(trackIndex, channel);
        onTrackMixStateChanged();
    }

    void setTrackMuted(int trackIndex, bool muted)
    {
        if (!trackMixState.isValidTrack(trackIndex))
            return;
        trackMixState.setMuted(trackIndex, muted);
        onTrackMixStateChanged();
    }

    void setTrackSolo(int trackIndex, bool solo)
    {
        if (!trackMixState.isValidTrack(trackIndex))
            return;
        trackMixState.setSolo(trackIndex, solo);
        onTrackMixStateChanged();
    }

    TrackMixSaveStatus getTrackMixSaveStatus() const
    {
        return trackMixSaveStatus;
    }

    juce::String getTrackMixSaveStatusText() const
    {
        if (getSongPresetKey().isEmpty())
            return {};

        switch (trackMixSaveStatus)
        {
            case TrackMixSaveStatus::pending: return "Mix settings changed - saving...";
            case TrackMixSaveStatus::failed:  return "Mix settings save failed - use Save Preset to retry";
            case TrackMixSaveStatus::saved:   return "Mix settings saved";
        }

        return {};
    }

    std::vector<MidiOutputDeviceInfo> getAvailableMidiOutputs() const
    {
        return MidiOutputDevice::enumerateAvailableOutputs();
    }

    juce::String getSelectedMidiOutputIdentifier() const
    {
        return midiOutputDevice.getSelectedIdentifier();
    }

    juce::String getSelectedMidiOutputName() const
    {
        return midiOutputDevice.getSelectedName();
    }

    juce::String getMidiOutputRestoreWarning() const
    {
        return midiOutputRestoreWarning;
    }

    bool selectMidiOutputDevice(const juce::String& identifier, bool persistSelection, juce::String& error)
    {
        const bool opened = midiOutputDevice.openByIdentifier(identifier, error);
        if (!opened)
            return false;

        midiOutputRestoreWarning.clear();
        if (persistSelection)
            saveSelectedMidiOutputToConfig();
        return true;
    }

    void clearMidiOutputDevice()
    {
        midiOutputDevice.close();
        midiOutputRestoreWarning.clear();
        saveSelectedMidiOutputToConfig();
    }

    void startPlaybackFromStart()
    {
        startPlayback();
    }

    void pausePlayback()
    {
        if (!playbackController.isPlaying())
            return;

        const int currentBar = juce::jmax(1, playbackController.getCurrentBar());
        continueBarInput.setText(juce::String(currentBar), juce::dontSendNotification);
        playbackController.pause();
        continueArmed = true;
        midiOutputDevice.sendAllNotesOff();
        updateTransportControls();
        setStatusMessage("Playback paused at bar " + juce::String(currentBar) + ".");
    }

    void stopPlaybackAndReset()
    {
        stopPlayback(true);
    }

    void continuePlaybackFromBarExternal(int bar)
    {
        if (project.tracks.empty())
            return;

        continueArmed = true;
        continueBarInput.setText(juce::String(bar), juce::dontSendNotification);
        continuePlaybackFromBar();
    }

    void seekToBarExternal(int bar)
    {
        if (project.tracks.empty())
            return;

        const int clampedBar = juce::jlimit(1, juce::jmax(1, project.maxBar), bar);
        playbackController.seekToSecond(project.tempoMap.barToSecondsDownbeat(clampedBar));
        midiPlaybackEngine.seekToPlaybackTime(project.tempoMap.barToSecondsDownbeat(clampedBar));
        scoreRenderer.setCurrentBar(clampedBar);
        scoreRenderer2.setCurrentBar(clampedBar);
        scoreRenderer3.setCurrentBar(clampedBar);
        transportLabel.setText("Bar " + juce::String(clampedBar), juce::dontSendNotification);
        displayedBar = clampedBar;
        continueBarInput.setText(juce::String(clampedBar), juce::dontSendNotification);
        continueArmed = true;
        refreshStatusMessage();
    }

private:
    class HideControlArrowButton final : public juce::TextButton
    {
    public:
        enum class Direction
        {
            up,
            down
        };

        HideControlArrowButton()
        {
            setButtonText({});
            setClickingTogglesState(false);
        }

        void setDirection(Direction newDirection)
        {
            direction = newDirection;
            repaint();
        }

        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(0.5f);
            const bool isEnabledNow = isEnabled();
            const bool isPressed = shouldDrawButtonAsDown;
            const auto background = isPressed ? findColour(juce::TextButton::buttonOnColourId)
                                              : findColour(juce::TextButton::buttonColourId);
            const float corner = 4.4f;

            const auto borderColour = isPressed ? juce::Colours::black.withAlpha(0.70f)
                                                : juce::Colours::black.withAlpha(0.55f);
            const auto highlightColour = isPressed ? juce::Colours::white.withAlpha(0.04f)
                                                   : juce::Colours::white.withAlpha(0.10f);

            g.setColour(isEnabledNow ? background : background.withAlpha(0.80f));
            g.fillRoundedRectangle(bounds, corner);

            auto topHalf = bounds;
            topHalf.setHeight(bounds.getHeight() * 0.45f);
            g.setColour(highlightColour);
            g.fillRoundedRectangle(topHalf, corner);

            if (isEnabledNow && shouldDrawButtonAsHighlighted && !isPressed)
            {
                g.setColour(juce::Colours::white.withAlpha(0.08f));
                g.fillRoundedRectangle(bounds, corner);
            }

            g.setColour(borderColour);
            g.drawRoundedRectangle(bounds, corner, 1.1f);

            const float verticalNudge = isPressed ? 0.8f : 0.0f;
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY() + verticalNudge;
            const float halfWidth = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.23f;
            const float halfHeight = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.20f;

            juce::Path arrowPath;
            if (direction == Direction::up)
            {
                arrowPath.startNewSubPath(cx - halfWidth, cy + halfHeight);
                arrowPath.lineTo(cx, cy - halfHeight);
                arrowPath.lineTo(cx + halfWidth, cy + halfHeight);
            }
            else
            {
                arrowPath.startNewSubPath(cx - halfWidth, cy - halfHeight);
                arrowPath.lineTo(cx, cy + halfHeight);
                arrowPath.lineTo(cx + halfWidth, cy - halfHeight);
            }

            const auto arrowColour = isEnabledNow ? juce::Colours::white : juce::Colours::lightgrey.withAlpha(0.55f);
            g.setColour(arrowColour);
            g.strokePath(arrowPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

    private:
        Direction direction = Direction::down;
    };

    struct LiveChordState
    {
        int lastWindowIndex = std::numeric_limits<int>::min();
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

    struct ScoreSongSettingsSnapshot
    {
        std::array<int, 3> staffTrackSelection = { 0, 0, 0 };
        std::array<int, 3> staffClefSelection = { 1, 1, 1 };
        std::array<int, 3> staffDisplayOctaveSelection = { 1, 1, 1 };
        juce::String chordTrackSelectionCsv;
        int accidentalSelection = 1;
        int aliasSelection = 1;
        int chordComplexitySelection = static_cast<int>(ChordDetector::ChordComplexity::rich);
        int chordResolutionSelection = static_cast<int>(ChordDetector::DetectionResolution::quarter);
        int transposeSemitones = 0;
        juce::String keyOverrideText;
        juce::String tempoOverrideText;
        bool scoreLightMode = true;
        int pdfExportModeId = 1;

        bool operator==(const ScoreSongSettingsSnapshot& other) const
        {
            return staffTrackSelection == other.staffTrackSelection
                && staffClefSelection == other.staffClefSelection
                && staffDisplayOctaveSelection == other.staffDisplayOctaveSelection
                && chordTrackSelectionCsv == other.chordTrackSelectionCsv
                && accidentalSelection == other.accidentalSelection
                && aliasSelection == other.aliasSelection
                && chordComplexitySelection == other.chordComplexitySelection
                && chordResolutionSelection == other.chordResolutionSelection
                && transposeSemitones == other.transposeSemitones
                && keyOverrideText == other.keyOverrideText
                && tempoOverrideText == other.tempoOverrideText
                && scoreLightMode == other.scoreLightMode
                && pdfExportModeId == other.pdfExportModeId;
        }
    };

    enum class PdfExportMode
    {
        allActiveStaffs = 1,
        staff1Only = 2
    };

    static PdfExportMode pdfExportModeFromSelectorId(int selectedId)
    {
        return selectedId == static_cast<int>(PdfExportMode::staff1Only)
            ? PdfExportMode::staff1Only
            : PdfExportMode::allActiveStaffs;
    }

    static int normalizeChordResolutionSelectorId(int selectedId)
    {
        return juce::jlimit(static_cast<int>(ChordDetector::DetectionResolution::quarter),
                            static_cast<int>(ChordDetector::DetectionResolution::eighth),
                            selectedId);
    }

    static int normalizeChordComplexitySelectorId(int selectedId)
    {
        return juce::jlimit(static_cast<int>(ChordDetector::ChordComplexity::simple),
                            static_cast<int>(ChordDetector::ChordComplexity::rich),
                            selectedId);
    }

    ChordDetector::DetectionResolution getChordDetectionResolution() const
    {
        const int normalized = normalizeChordResolutionSelectorId(chordResolutionSelector.getSelectedId());
        return normalized == static_cast<int>(ChordDetector::DetectionResolution::eighth)
            ? ChordDetector::DetectionResolution::eighth
            : ChordDetector::DetectionResolution::quarter;
    }

    ChordDetector::ChordComplexity getChordComplexity() const
    {
        const int normalized = normalizeChordComplexitySelectorId(chordComplexitySelector.getSelectedId());
        if (normalized == static_cast<int>(ChordDetector::ChordComplexity::simple))
            return ChordDetector::ChordComplexity::simple;
        if (normalized == static_cast<int>(ChordDetector::ChordComplexity::standard))
            return ChordDetector::ChordComplexity::standard;
        return ChordDetector::ChordComplexity::rich;
    }

    static int normalizeStaffDisplayOctaveSelectorId(int selectedId)
    {
        return juce::jlimit(1, 3, selectedId);
    }

    static ScoreRenderer::DisplayOctaveShift staffDisplayOctaveShiftFromSelectorId(int selectedId)
    {
        const int normalized = normalizeStaffDisplayOctaveSelectorId(selectedId);
        if (normalized == 2)
            return ScoreRenderer::DisplayOctaveShift::upOne;
        if (normalized == 3)
            return ScoreRenderer::DisplayOctaveShift::downOne;
        return ScoreRenderer::DisplayOctaveShift::normal;
    }

    static void applyStaffDisplayOctaveState(juce::ComboBox& octaveSelector,
                                             const juce::ComboBox& clefSelector,
                                             ScoreRenderer& renderer,
                                             bool normalizeSelector)
    {
        const bool isDrumClef = clefSelector.getSelectedId() == 3;
        if (isDrumClef)
        {
            octaveSelector.setEnabled(false);
            if (normalizeSelector)
                octaveSelector.setSelectedId(1, juce::dontSendNotification);
            renderer.setDisplayOctaveShift(ScoreRenderer::DisplayOctaveShift::normal);
            return;
        }

        const int normalized = normalizeStaffDisplayOctaveSelectorId(octaveSelector.getSelectedId());
        octaveSelector.setEnabled(true);
        if (normalizeSelector)
            octaveSelector.setSelectedId(normalized, juce::dontSendNotification);
        renderer.setDisplayOctaveShift(staffDisplayOctaveShiftFromSelectorId(normalized));
    }

    void applyStaffDisplayOctaveFromSelectors(bool normalizeSelectors)
    {
        applyStaffDisplayOctaveState(staff1OctaveSelector, staff1ClefSelector, scoreRenderer, normalizeSelectors);
        applyStaffDisplayOctaveState(staff2OctaveSelector, staff2ClefSelector, scoreRenderer2, normalizeSelectors);
        applyStaffDisplayOctaveState(staff3OctaveSelector, staff3ClefSelector, scoreRenderer3, normalizeSelectors);
    }

    void updateWindowTitle()
    {
        auto* window = dynamic_cast<juce::DocumentWindow*>(getTopLevelComponent());
        if (window == nullptr)
            window = findParentComponentOfClass<juce::DocumentWindow>();
        if (window == nullptr)
            return;

        const auto title = project.file.existsAsFile()
            ? ("MidiScorer - " + project.file.getFileName())
            : juce::String("MidiScorer");
        window->setName(title);
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
        const bool wasProfileOnly = keyOverrideProfileOnly;
        const auto text = keyOverrideInput.getText().trim();
        if (text.isEmpty())
        {
            keyOverride = std::nullopt;
            if (userInitiated)
            {
                keyOverrideProfileOnly = false;
                suppressProfileOnlyExitUntilKeyChange = false;
                assignToggle.setToggleState(true, juce::dontSendNotification);
            }
            keyTransposeReferenceTonicPc = getMidiDetectedTonicPc();
            keyTransposeAppliedSemitones = 0;
            if (userInitiated)
                setStatusMessage("Key override cleared (using detected key).");
            return;
        }

        const auto parsed = parseKeyText(text);
        if (!parsed.has_value())
        {
            keyOverride = std::nullopt;
            if (userInitiated)
            {
                keyOverrideProfileOnly = false;
                suppressProfileOnlyExitUntilKeyChange = false;
                keyOverrideInput.setText({}, juce::dontSendNotification);
                assignToggle.setToggleState(true, juce::dontSendNotification);
            }
            if (userInitiated)
                setStatusMessage("Invalid key override. Reverted to detected key.");
            return;
        }

        const bool keyChanged = !keyOverride.has_value()
            || !keyOverride->displayText.equalsIgnoreCase(parsed->displayText);
        keyOverride = parsed;
        keyOverrideInput.setText(parsed->displayText, juce::dontSendNotification);
        if (userInitiated)
        {
            if (assignToggle.getToggleState())
            {
                keyOverrideProfileOnly = true;
                suppressProfileOnlyExitUntilKeyChange = false;
            }
            else if (wasProfileOnly && suppressProfileOnlyExitUntilKeyChange && !keyChanged)
            {
                // Preserve profile-only mode through the immediate follow-up key
                // event after uncheck. Exit only when the key text actually changes.
                keyOverrideProfileOnly = true;
            }
            else
            {
                keyOverrideProfileOnly = false;
                suppressProfileOnlyExitUntilKeyChange = false;
            }
        }
        if (userInitiated)
            setStatusMessage("Key override set to " + parsed->displayText + ".");

        if (assignToggle.getToggleState() || keyOverrideProfileOnly)
        {
            syncKeyTransposeReferenceToOverrideOrMidi();
            keyTransposeAppliedSemitones = 0;
        }
        else if (keyChanged)
        {
            keyTransposeAppliedSemitones = KeyOverrideTranspose::appliedSemitonesAfterKeyChange(
                keyTransposeAppliedSemitones,
                keyTransposeReferenceTonicPc,
                parsed->tonicPc);
            pendingKeyTransposeReferenceTonicPc = parsed->tonicPc;
        }
    }

    int getMidiDetectedTonicPc() const
    {
        return KeyOverrideTranspose::midiTonicPc(project.hasKeySignature,
                                                project.keySharpsOrFlats,
                                                project.keyIsMajor);
    }

    void syncKeyTransposeReferenceToOverrideOrMidi()
    {
        if (keyOverride.has_value())
            keyTransposeReferenceTonicPc = keyOverride->tonicPc;
        else
            keyTransposeReferenceTonicPc = getMidiDetectedTonicPc();
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

    int getKeyOverrideTransposeSemitones() const
    {
        if (assignToggle.getToggleState() || keyOverrideProfileOnly || !keyOverride.has_value())
            return 0;

        return keyTransposeAppliedSemitones;
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
        namingOptions.complexity = getChordComplexity();
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

    static void layoutStaffControls(juce::Rectangle<int> area,
                                    juce::Label& label,
                                    juce::ComboBox& trackSelector,
                                    juce::ComboBox& octaveSelector,
                                    juce::ComboBox& clefSelector)
    {
        label.setBounds(area.removeFromLeft(52));
        trackSelector.setBounds(area.removeFromLeft(160).reduced(2, 0));
        octaveSelector.setBounds(area.removeFromLeft(64).reduced(1, 0));
        clefSelector.setBounds(area.removeFromRight(82));
    }

    void refreshHideScoreControlsButtonText()
    {
        hideScoreControlsButton.setDirection(scoreControlRowsHidden
                                                 ? HideControlArrowButton::Direction::down
                                                 : HideControlArrowButton::Direction::up);
    }

    void applyScoreControlRowsVisibility()
    {
        const bool visible = !scoreControlRowsHidden;
        auto setVisible = [visible](juce::Component& component)
        {
            component.setVisible(visible);
            component.setEnabled(visible);
        };

        // Staff selector row.
        setVisible(staff1TrackLabel);
        setVisible(staff1TrackSelector);
        setVisible(staff1OctaveSelector);
        setVisible(staff1ClefSelector);
        setVisible(staff2TrackLabel);
        setVisible(staff2TrackSelector);
        setVisible(staff2OctaveSelector);
        setVisible(staff2ClefSelector);
        setVisible(staff3TrackLabel);
        setVisible(staff3TrackSelector);
        setVisible(staff3OctaveSelector);
        setVisible(staff3ClefSelector);

        // Chord controls row.
        setVisible(autoChordLabel);
        setVisible(chordComplexityLabel);
        setVisible(chordComplexitySelector);
        setVisible(chordResolutionLabel);
        setVisible(chordResolutionSelector);
        setVisible(accidentalLabel);
        setVisible(accidentalSelector);
        setVisible(accidentalHelpButton);
        setVisible(aliasLabel);
        setVisible(aliasSelector);
        setVisible(chordTracksLabel);
        for (auto* button : chordTrackButtons)
            if (button != nullptr)
                setVisible(*button);

        // Tempo/status row.
        setVisible(tempoOverrideLabel);
        setVisible(tempoOverrideInput);
        setVisible(tempoHelpButton);
        setVisible(assignLabel);
        setVisible(assignToggle);
        setVisible(keyOverrideLabel);
        setVisible(keyOverrideInput);
        setVisible(keyHelpButton);
        setVisible(globalTransposeLabel);
        setVisible(globalTransposeInput);
        setVisible(transposeHelpButton);
        setVisible(statusLabel);
    }

    void setScoreControlRowsHidden(bool hidden)
    {
        if (scoreControlRowsHidden == hidden)
            return;

        scoreControlRowsHidden = hidden;
        applyScoreControlRowsVisibility();
        refreshHideScoreControlsButtonText();
        resized();
    }

    static bool isStaffDisplayed(const juce::ComboBox& trackSelector)
    {
        // Selector index 0 is "No Display".
        return trackSelector.getSelectedItemIndex() > 0;
    }

    ScoreSongSettingsSnapshot buildCurrentScoreSongSettingsSnapshot() const
    {
        ScoreSongSettingsSnapshot snapshot;
        snapshot.staffTrackSelection = { staff1TrackSelector.getSelectedItemIndex(),
                                         staff2TrackSelector.getSelectedItemIndex(),
                                         staff3TrackSelector.getSelectedItemIndex() };
        snapshot.staffClefSelection = { staff1ClefSelector.getSelectedId(),
                                        staff2ClefSelector.getSelectedId(),
                                        staff3ClefSelector.getSelectedId() };
        snapshot.staffDisplayOctaveSelection = { staff1OctaveSelector.getSelectedId(),
                                                 staff2OctaveSelector.getSelectedId(),
                                                 staff3OctaveSelector.getSelectedId() };
        snapshot.chordTrackSelectionCsv = buildChordTrackSelectionCsv();
        snapshot.accidentalSelection = accidentalSelector.getSelectedId();
        snapshot.aliasSelection = aliasSelector.getSelectedId();
        snapshot.chordComplexitySelection = normalizeChordComplexitySelectorId(chordComplexitySelector.getSelectedId());
        snapshot.chordResolutionSelection = normalizeChordResolutionSelectorId(chordResolutionSelector.getSelectedId());
        snapshot.transposeSemitones = getClampedTranspose(globalTransposeInput);
        snapshot.keyOverrideText = keyOverride.has_value() ? keyOverride->displayText : juce::String();
        snapshot.tempoOverrideText = tempoOverrideInput.getText().trim();
        snapshot.scoreLightMode = scoreLightMode;
        snapshot.pdfExportModeId = exportPdfModeSelector.getSelectedId();
        return snapshot;
    }

    void markCurrentScoreSongSettingsAsSaved()
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty())
        {
            savedScoreSongSettingsSnapshot.reset();
            savedScoreSongKey.clear();
            return;
        }

        savedScoreSongSettingsSnapshot = buildCurrentScoreSongSettingsSnapshot();
        savedScoreSongKey = songKey;
    }

    bool isScoreSongSettingsDirty() const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty())
            return false;

        if (!savedScoreSongSettingsSnapshot.has_value() || savedScoreSongKey != songKey)
            return false;

        return !(buildCurrentScoreSongSettingsSnapshot() == savedScoreSongSettingsSnapshot.value());
    }

    void applySavePresetButtonCleanStyle()
    {
        const auto baseOff = juce::Colours::lightgrey;
        const auto baseOn = juce::Colours::lightgrey;
        const auto baseText = juce::Colours::black;

        savePresetButton.setColour(juce::TextButton::buttonColourId, baseOff);
        savePresetButton.setColour(juce::TextButton::buttonOnColourId, baseOn);
        savePresetButton.setColour(juce::TextButton::textColourOffId, baseText);
        savePresetButton.setColour(juce::TextButton::textColourOnId, baseText);
        loadPresetButton.setColour(juce::TextButton::buttonColourId, baseOff);
        loadPresetButton.setColour(juce::TextButton::buttonOnColourId, baseOn);
        loadPresetButton.setColour(juce::TextButton::textColourOffId, baseText);
        loadPresetButton.setColour(juce::TextButton::textColourOnId, baseText);
    }

    void applySavePresetButtonDirtyStyle()
    {
        const auto pendingOff = juce::Colours::darkred;
        const auto pendingOn = juce::Colours::darkred.brighter();
        const auto pendingText = juce::Colours::white;

        savePresetButton.setColour(juce::TextButton::buttonColourId, pendingOff);
        savePresetButton.setColour(juce::TextButton::buttonOnColourId, pendingOn);
        savePresetButton.setColour(juce::TextButton::textColourOffId, pendingText);
        savePresetButton.setColour(juce::TextButton::textColourOnId, pendingText);
        loadPresetButton.setColour(juce::TextButton::buttonColourId, pendingOff);
        loadPresetButton.setColour(juce::TextButton::buttonOnColourId, pendingOn);
        loadPresetButton.setColour(juce::TextButton::textColourOffId, pendingText);
        loadPresetButton.setColour(juce::TextButton::textColourOnId, pendingText);
    }

    void refreshSavePresetButtonDirtyStyle()
    {
        if (isScoreSongSettingsDirty())
            applySavePresetButtonDirtyStyle();
        else
            applySavePresetButtonCleanStyle();
    }

    std::vector<TrackMixSettings> buildCurrentTrackMixSnapshot() const
    {
        std::vector<TrackMixSettings> snapshot;
        snapshot.reserve(static_cast<size_t>(trackMixState.getTrackCount()));
        for (int i = 0; i < trackMixState.getTrackCount(); ++i)
        {
            snapshot.push_back({
                trackMixState.getVolume(i),
                trackMixState.getExpression(i),
                trackMixState.getReverb(i),
                trackMixState.getChannel(i),
                trackMixState.isMuted(i),
                trackMixState.isSolo(i)
            });
        }
        return snapshot;
    }

    void markTrackMixAsSaved()
    {
        savedTrackMixSnapshot = buildCurrentTrackMixSnapshot();
        trackMixSaveStatus = TrackMixSaveStatus::saved;
    }

    bool isTrackMixDirty() const
    {
        return buildCurrentTrackMixSnapshot() != savedTrackMixSnapshot;
    }

    static bool trackHasChordSourceContent(const MidiTrackData& track)
    {
        if (!track.notes.empty())
            return true;

        for (int eventIndex = 0; eventIndex < track.sequence.getNumEvents(); ++eventIndex)
        {
            const auto* holder = track.sequence.getEventPointer(eventIndex);
            if (holder == nullptr)
                continue;

            if (!holder->message.isMetaEvent())
                return true;
        }

        return false;
    }

    void onTrackMixStateChanged()
    {
        if (playbackController.isPlaying())
            midiOutputDevice.sendAllNotesOff();
        trackMixSaveStatus = TrackMixSaveStatus::pending;
        scheduleDebouncedTrackMixPresetSave();
        refreshStatusMessage();
    }

    void scheduleDebouncedTrackMixPresetSave()
    {
        ++trackMixPresetSaveGeneration;
        const int generation = trackMixPresetSaveGeneration;
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::Timer::callAfterDelay(trackMixPresetAutosaveDelayMs, [safeThis, generation]()
        {
            if (safeThis == nullptr || generation != safeThis->trackMixPresetSaveGeneration)
                return;
            safeThis->saveUiPreset(false);
        });
    }

    void invalidateDebouncedTrackMixPresetSave()
    {
        ++trackMixPresetSaveGeneration;
    }

    int getChordTracksLayoutHeight(int availableWidth) const
    {
        const int tracksLabelWidth = 120;
        const int xPadding = 6;
        const int yPadding = 4;
        const int topRowHeight = 26;
        const int rowGap = 4;
        const int rowHeight = 22;
        const int usableWidth = juce::jmax(120, availableWidth - tracksLabelWidth - xPadding * 2);

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

        return yPadding * 2 + topRowHeight + rowGap + rows * rowHeight;
    }

    void layoutChordTrackButtons(juce::Rectangle<int> area)
    {
        const int xPadding = 6;
        const int rowHeight = 22;
        const int topRowHeight = 26;
        const int rowGap = 4;

        auto topRow = area.removeFromTop(topRowHeight);
        auto buttonsArea = area;
        buttonsArea.removeFromTop(rowGap);

        auto autoChordLabelBounds = topRow.removeFromLeft(82);
        auto chordComplexityLabelBounds = topRow.removeFromLeft(52);
        auto chordComplexityBounds = topRow.removeFromLeft(96).reduced(4, 0);
        chordComplexityBounds.setHeight(26);
        autoChordLabelBounds.setY(chordComplexityBounds.getY());
        autoChordLabelBounds.setHeight(chordComplexityBounds.getHeight());
        autoChordLabel.setBounds(autoChordLabelBounds);
        chordComplexityLabelBounds.setY(chordComplexityBounds.getY());
        chordComplexityLabelBounds.setHeight(chordComplexityBounds.getHeight());
        chordComplexityLabel.setBounds(chordComplexityLabelBounds);
        chordComplexitySelector.setBounds(chordComplexityBounds);

        auto chordResolutionLabelBounds = topRow.removeFromLeft(42);
        auto chordResolutionBounds = topRow.removeFromLeft(64).reduced(4, 0);
        chordResolutionBounds.setHeight(26);
        chordResolutionLabelBounds.setY(chordResolutionBounds.getY());
        chordResolutionLabelBounds.setHeight(chordResolutionBounds.getHeight());
        chordResolutionLabel.setBounds(chordResolutionLabelBounds);
        chordResolutionSelector.setBounds(chordResolutionBounds);

        auto accidentalLabelBounds = topRow.removeFromLeft(52);
        accidentalLabelBounds.setY(chordResolutionBounds.getY());
        accidentalLabelBounds.setHeight(chordResolutionBounds.getHeight());
        accidentalLabel.setBounds(accidentalLabelBounds);

        auto accidentalBounds = topRow.removeFromLeft(92).reduced(4, 0);
        accidentalBounds.setY(chordResolutionBounds.getY());
        accidentalBounds.setHeight(chordResolutionBounds.getHeight());
        accidentalSelector.setBounds(accidentalBounds);

        auto accidentalHelpBounds = topRow.removeFromLeft(28).reduced(4, 0);
        accidentalHelpBounds.setY(chordResolutionBounds.getY());
        accidentalHelpBounds.setHeight(chordResolutionBounds.getHeight());
        accidentalHelpButton.setBounds(accidentalHelpBounds);

        auto aliasLabelBounds = topRow.removeFromLeft(40);
        aliasLabelBounds.setY(chordResolutionBounds.getY());
        aliasLabelBounds.setHeight(chordResolutionBounds.getHeight());
        aliasLabel.setBounds(aliasLabelBounds);

        auto aliasBounds = topRow.removeFromLeft(88).reduced(4, 0);
        aliasBounds.setY(chordResolutionBounds.getY());
        aliasBounds.setHeight(chordResolutionBounds.getHeight());
        aliasSelector.setBounds(aliasBounds);

        auto chordTracksLabelBounds = buttonsArea.removeFromLeft(120);
        chordTracksLabelBounds.setY(buttonsArea.getY());
        chordTracksLabelBounds.setHeight(rowHeight);
        chordTracksLabel.setBounds(chordTracksLabelBounds);

        int x = buttonsArea.getX();
        int y = buttonsArea.getY();
        const int right = buttonsArea.getRight();
        juce::Font measureFont(13.0f);

        for (auto* button : chordTrackButtons)
        {
            const int w = juce::jlimit(84, 190, measureFont.getStringWidth(button->getButtonText()) + 28);
            if (x > buttonsArea.getX() && x + w > right)
            {
                x = buttonsArea.getX();
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
        chordTrackSourceIndices.clear();

        int visibleCount = 0;
        for (int i = 0; i < static_cast<int>(project.tracks.size()); ++i)
        {
            if (!trackHasChordSourceContent(project.tracks[(size_t) i]))
                continue;

            auto* button = chordTrackButtons.add(new juce::ToggleButton(project.tracks[(size_t) i].name));
            chordTrackSourceIndices.add(i);
            button->setToggleState(visibleCount < 5, juce::dontSendNotification);
            button->onClick = [this]() { rebuildAllStaffs(); refreshSavePresetButtonDirtyStyle(); };
            addAndMakeVisible(button);
            ++visibleCount;
        }

        applyScoreControlRowsVisibility();

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
                selected.add(juce::String(chordTrackSourceIndices[i]));
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
            const int trackIndex = token.getIntValue();
            for (int i = 0; i < chordTrackButtons.size(); ++i)
            {
                if (chordTrackSourceIndices[i] == trackIndex)
                    chordTrackButtons[i]->setToggleState(true, juce::dontSendNotification);
            }
        }

        bool anySelected = false;
        for (auto* button : chordTrackButtons)
            anySelected = anySelected || button->getToggleState();

        if (!anySelected && chordTrackButtons.size() > 0)
            chordTrackButtons[0]->setToggleState(true, juce::dontSendNotification);
    }

    void saveSelectedMidiOutputToConfig()
    {
        auto payloadObj = std::make_unique<juce::DynamicObject>();
        payloadObj->setProperty("selectedOutputIdentifier", midiOutputDevice.getSelectedIdentifier());
        payloadObj->setProperty("selectedOutputName", midiOutputDevice.getSelectedName());
        juce::String writeError;
        if (!PresetFileStore::writeTextFileAtomically(PresetFileStore::getMidiOutputConfigPath(),
                                                      juce::JSON::toString(juce::var(payloadObj.release())),
                                                      writeError))
        {
            setStatusMessage("Failed to save MIDI output selection.");
            juce::Logger::writeToLog("MidiScorer: could not save MIDI output config: " + writeError);
        }
    }

    void loadSelectedMidiOutputFromConfig()
    {
        const auto configFile = PresetFileStore::getMidiOutputConfigPath();
        if (!configFile.existsAsFile())
            return;

        juce::var parsed;
        const auto parseResult = juce::JSON::parse(configFile.loadFileAsString(), parsed);
        if (parseResult.failed() || !parsed.isObject())
            return;

        auto* object = parsed.getDynamicObject();
        if (object == nullptr || !object->hasProperty("selectedOutputIdentifier"))
            return;

        const auto outputIdentifier = object->getProperty("selectedOutputIdentifier").toString();
        if (outputIdentifier.isEmpty())
            return;

        juce::String error;
        if (!midiOutputDevice.openByIdentifier(outputIdentifier, error))
        {
            midiOutputRestoreWarning = "Saved MIDI output unavailable: " + error;
            juce::Logger::writeToLog("MidiScorer: could not restore MIDI output: " + error);
        }
        else
        {
            midiOutputRestoreWarning.clear();
        }
    }

    void loadLastMidiDirectoryFromPreset()
    {
        const auto file = PresetFileStore::getPresetFilePath();
        if (!file.existsAsFile())
            return;

        juce::var parsed;
        const auto parseResult = juce::JSON::parse(file.loadFileAsString(), parsed);
        if (parseResult.failed() || !parsed.isObject())
            return;

        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr || !obj->hasProperty("lastMidiDirectory"))
            return;

        const juce::File candidate(obj->getProperty("lastMidiDirectory").toString());
        if (candidate.isDirectory())
            lastMidiDirectory = candidate;
    }

    void saveLastMidiDirectoryToPreset()
    {
        if (!lastMidiDirectory.isDirectory())
            return;

        const auto directoryPath = lastMidiDirectory.getFullPathName();
        juce::String writeError;
        if (!PresetFileStore::mergeWritePreset(
                [&](juce::DynamicObject& obj)
                {
                    obj.setProperty("lastMidiDirectory", directoryPath);
                },
                writeError))
            juce::Logger::writeToLog("MidiScorer: could not save last MIDI directory: " + writeError);
    }

    void loadWorkingDirectoryFromPreset()
    {
        const auto file = PresetFileStore::getPresetFilePath();
        if (!file.existsAsFile())
            return;

        juce::var parsed;
        const auto parseResult = juce::JSON::parse(file.loadFileAsString(), parsed);
        if (parseResult.failed() || !parsed.isObject())
            return;

        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr || !obj->hasProperty("workingDirectory"))
            return;

        const juce::File candidate(obj->getProperty("workingDirectory").toString());
        if (candidate.isDirectory())
            workingDirectory = candidate;
    }

    void saveWorkingDirectoryToPreset()
    {
        if (!workingDirectory.isDirectory())
            return;

        const auto directoryPath = workingDirectory.getFullPathName();
        juce::String writeError;
        if (!PresetFileStore::mergeWritePreset(
                [&](juce::DynamicObject& obj)
                {
                    obj.setProperty("workingDirectory", directoryPath);
                },
                writeError))
            juce::Logger::writeToLog("MidiScorer: could not save working directory: " + writeError);
    }

    void ensureWorkingDirectoryInitialized()
    {
        if (workingDirectory.isDirectory())
            return;

        juce::File defaultDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("MidiScorer")
            .getChildFile("WorkingMidi");
        if (!defaultDir.exists())
            defaultDir.createDirectory();

        if (defaultDir.isDirectory())
        {
            workingDirectory = defaultDir;
            saveWorkingDirectoryToPreset();
        }
    }

    void loadStartupSettingsFromPreset()
    {
        const auto file = PresetFileStore::getPresetFilePath();
        if (!file.existsAsFile())
            return;

        juce::var parsed;
        const auto parseResult = juce::JSON::parse(file.loadFileAsString(), parsed);
        if (parseResult.failed() || !parsed.isObject())
            return;

        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return;

        startupResumeEnabled = static_cast<bool>(obj->getProperty("startupResumeEnabled"));
        if (obj->hasProperty("lastLoadedMidiPath"))
            lastLoadedMidiPath = obj->getProperty("lastLoadedMidiPath").toString().trim();
    }

    void saveStartupResumeEnabledToPreset()
    {
        const auto enabled = startupResumeEnabled;
        juce::String writeError;
        if (!PresetFileStore::mergeWritePreset(
                [&](juce::DynamicObject& obj)
                {
                    obj.setProperty("startupResumeEnabled", enabled);
                },
                writeError))
            juce::Logger::writeToLog("MidiScorer: could not save startup resume setting: " + writeError);
    }

    void saveLastLoadedMidiPathToPreset()
    {
        if (lastLoadedMidiPath.isEmpty())
            return;

        const auto path = lastLoadedMidiPath;
        juce::String writeError;
        if (!PresetFileStore::mergeWritePreset(
                [&](juce::DynamicObject& obj)
                {
                    obj.setProperty("lastLoadedMidiPath", path);
                },
                writeError))
            juce::Logger::writeToLog("MidiScorer: could not save last loaded MIDI path: " + writeError);
    }

    static juce::File findFirstMidiFile(const juce::StringArray& files)
    {
        for (const auto& path : files)
        {
            juce::File file(path);
            const auto ext = file.getFileExtension().toLowerCase();
            if (file.existsAsFile() && (ext == ".mid" || ext == ".midi"))
                return file;
        }
        return {};
    }

    void pushRecentMidiFile(const juce::File& midiFile)
    {
        if (!midiFile.existsAsFile())
            return;

        const auto normalized = midiFile.getFullPathName().replaceCharacter('\\', '/');
        recentMidiFiles.erase(std::remove_if(recentMidiFiles.begin(),
                                             recentMidiFiles.end(),
                                             [&](const juce::String& existing)
                                             {
                                                 return existing.equalsIgnoreCase(normalized);
                                             }),
                              recentMidiFiles.end());
        recentMidiFiles.insert(recentMidiFiles.begin(), normalized);
        if (recentMidiFiles.size() > 10)
            recentMidiFiles.resize(10);
        refreshRecentFilesButtonState();
        saveRecentFilesToPreset();
    }

    void refreshRecentFilesButtonState()
    {
        openRecentButton.setEnabled(!recentMidiFiles.empty());
    }

    void openRecentFileList()
    {
        if (recentMidiFiles.empty())
            return;

        juce::PopupMenu recentMenu;
        for (int i = 0; i < static_cast<int>(recentMidiFiles.size()); ++i)
        {
            const juce::File file(recentMidiFiles[(size_t) i]);
            const auto parentName = file.getParentDirectory().getFileName();
            const auto fileLabel = parentName.isNotEmpty()
                ? (file.getFileName() + "  [" + parentName + "]")
                : file.getFileName();
            recentMenu.addItem(i + 1, fileLabel);
        }
        juce::Component::SafePointer<MainComponent> safeThis(this);
        recentMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&openRecentButton),
                                 juce::ModalCallbackFunction::create([safeThis](int result)
        {
            if (safeThis == nullptr || result <= 0)
                return;

            const int index = result - 1;
            if (index < 0 || index >= static_cast<int>(safeThis->recentMidiFiles.size()))
                return;

            const juce::File file(safeThis->recentMidiFiles[(size_t) index]);
            if (!file.existsAsFile())
            {
                safeThis->setStatusMessage("Recent file missing: " + file.getFullPathName());
                return;
            }
            safeThis->loadMidiFileFromPath(file);
        }));
    }

    void loadRecentFilesFromPreset()
    {
        recentMidiFiles.clear();
        const auto file = PresetFileStore::getPresetFilePath();
        if (!file.existsAsFile())
            return;

        juce::var parsed;
        const auto parseResult = juce::JSON::parse(file.loadFileAsString(), parsed);
        if (parseResult.failed() || !parsed.isObject())
            return;
        auto* obj = parsed.getDynamicObject();
        if (obj == nullptr || !obj->hasProperty("recentMidiFiles"))
            return;
        auto* arr = obj->getProperty("recentMidiFiles").getArray();
        if (arr == nullptr)
            return;

        for (const auto& entry : *arr)
        {
            const auto path = entry.toString().trim();
            if (path.isNotEmpty())
                recentMidiFiles.push_back(path);
            if (recentMidiFiles.size() >= 10)
                break;
        }
        refreshRecentFilesButtonState();
    }

    void saveRecentFilesToPreset()
    {
        juce::Array<juce::var> recent;
        for (const auto& path : recentMidiFiles)
            recent.add(path);

        juce::String writeError;
        if (!PresetFileStore::mergeWritePreset(
                [&](juce::DynamicObject& obj)
                {
                    obj.setProperty("recentMidiFiles", juce::var(recent));
                },
                writeError))
            juce::Logger::writeToLog("MidiScorer: could not save recent MIDI files: " + writeError);
    }

    void applyLoopBoundsFromInputs(bool persist = true)
    {
        const int maxBar = juce::jmax(1, project.maxBar);
        loopStartBar = juce::jlimit(1, maxBar, juce::jmax(1, loopStartInput.getText().getIntValue()));
        loopEndBar = juce::jlimit(loopStartBar, maxBar, juce::jmax(1, loopEndInput.getText().getIntValue()));
        loopStartInput.setText(juce::String(loopStartBar), juce::dontSendNotification);
        loopEndInput.setText(juce::String(loopEndBar), juce::dontSendNotification);
        if (persist)
            saveUiPreset(false);
        refreshStatusMessage();
    }

    juce::File copyMidiToWorkingDirectoryIfNeeded(const juce::File& sourceFile, bool& copied)
    {
        copied = false;

        if (!sourceFile.existsAsFile() || !workingDirectory.isDirectory())
            return sourceFile;

        if (sourceFile.getParentDirectory().getFullPathName().equalsIgnoreCase(workingDirectory.getFullPathName()))
            return sourceFile;

        juce::File destination = workingDirectory.getChildFile(sourceFile.getFileName());
        if (destination.existsAsFile() && !destination.hasIdenticalContentTo(sourceFile))
        {
            destination = workingDirectory.getNonexistentChildFile(
                sourceFile.getFileNameWithoutExtension(),
                sourceFile.getFileExtension(),
                false);
        }

        if (destination.getFullPathName().equalsIgnoreCase(sourceFile.getFullPathName()))
            return sourceFile;

        if (!sourceFile.copyFileTo(destination))
        {
            setStatusMessage("Could not copy MIDI to working directory; loading original file.");
            return sourceFile;
        }

        copied = true;
        return destination;
    }

    void showRenameBeforeCopyModal(const juce::File& sourceFile,
                                   const juce::String& initialBaseName,
                                   const juce::String& validationError,
                                   const std::function<void(std::optional<juce::File>)>& onComplete)
    {
        if (!sourceFile.existsAsFile() || !workingDirectory.isDirectory())
        {
            onComplete(std::nullopt);
            return;
        }

        juce::String message = "Choose a filename for the working-directory copy.\n\n"
                               "Source: " + sourceFile.getFullPathName() + "\n"
                               "Target folder: " + workingDirectory.getFullPathName();
        if (validationError.isNotEmpty())
            message += "\n\n" + validationError;

        auto* alert = new juce::AlertWindow("Copy to Working Directory",
                                            message,
                                            juce::MessageBoxIconType::QuestionIcon,
                                            this);
        alert->addTextEditor("midiName",
                             WorkingDirectoryCopy::normalizeMidiBaseName(initialBaseName),
                             "Filename");
        alert->addButton("Copy & Load", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        juce::Component::SafePointer<MainComponent> safeThis(this);
        alert->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, alert, sourceFile, onComplete](int result)
        {
            if (safeThis == nullptr)
                return;

            if (result != 1)
            {
                onComplete(std::nullopt);
                return;
            }

            juce::String destinationError;
            const auto requestedBaseName = alert->getTextEditorContents("midiName");
            const auto destination = WorkingDirectoryCopy::buildWorkingDirectoryDestination(
                safeThis->workingDirectory,
                requestedBaseName,
                sourceFile.getFileExtension(),
                destinationError);

            if (!destination.has_value())
            {
                safeThis->showRenameBeforeCopyModal(sourceFile, requestedBaseName, destinationError, onComplete);
                return;
            }

            if (!sourceFile.copyFileTo(*destination))
            {
                showWarningModal(safeThis, "Copy to Working Directory", "Could not copy MIDI to working directory.");
                onComplete(std::nullopt);
                return;
            }

            onComplete(*destination);
        }), true);
    }

    void finishLoadMidiFileFromPath(const juce::File& fileToLoad, bool copiedToWorkingDirectory)
    {
        if (!fileToLoad.existsAsFile())
            return;

        lastMidiDirectory = fileToLoad.getParentDirectory();
        saveLastMidiDirectoryToPreset();
        pushRecentMidiFile(fileToLoad);

        juce::String error;
        MidiProjectData loaded;
        bool rejectedType0 = false;
        if (!loader.load(fileToLoad, loaded, error, &rejectedType0, false))
        {
            if (rejectedType0)
            {
                showWarningModal(this, "MIDI Type 0 Not Supported", error);
                setStatusMessage("Load cancelled: MIDI file type 0 is not supported.");
            }
            else
            {
                setStatusMessage("Load failed: " + error);
            }
            updateWindowTitle();
            return;
        }

        if (!midiPlaybackEngine.loadFromFile(fileToLoad, error))
        {
            setStatusMessage("Load failed: unable to prepare MIDI player (" + error + ")");
            updateWindowTitle();
            return;
        }

        project = std::move(loaded);
        initializeTrackMixFromMidi();
        updateWindowTitle();
        refreshTrackSelectors();
        refreshChordTrackButtons();
        playbackController.setTempoMap(&project.tempoMap, project.totalDurationSec);
        tempoOverrideBpm = std::nullopt;
        tempoOverrideInput.setText({}, juce::dontSendNotification);
        syncPlaybackTempoOverride();
        continueArmed = false;
        continueBarInput.setText("1", juce::dontSendNotification);
        loopStartBar = 1;
        loopEndBar = juce::jmax(1, project.maxBar);
        loopStartInput.setText("1", juce::dontSendNotification);
        loopEndInput.setText(juce::String(loopEndBar), juce::dontSendNotification);
        resetLiveChordState();
        updateTransportControls();
        keyLabel.setText("Key: " + (keyOverride.has_value() ? keyOverride->displayText : project.getKeyDisplayText()),
                         juce::dontSendNotification);
        midiMetaLabel.setText(buildMidiMetaText(), juce::dontSendNotification);
        assignDefaultStaffSelections();
        const bool autoPresetLoaded = loadUiPreset(true);
        if (!tempoOverrideBpm.has_value())
        {
            const auto detectedTempo = getDetectedTempoBpm();
            if (detectedTempo.has_value())
                tempoOverrideInput.setText(juce::String(detectedTempo.value(), 1), juce::dontSendNotification);
            else
                tempoOverrideInput.setText({}, juce::dontSendNotification);
        }
        if (!autoPresetLoaded)
        {
            rebuildAllStaffs();
            markCurrentScoreSongSettingsAsSaved();
            markTrackMixAsSaved();
            refreshSavePresetButtonDirtyStyle();
        }
        setStatusMessage(autoPresetLoaded
                             ? ("Loaded: " + project.file.getFileName()
                                + (copiedToWorkingDirectory ? " (copied to working directory, auto preset applied)"
                                                            : " (auto preset applied)"))
                             : ("Loaded: " + project.file.getFileName()
                                + (copiedToWorkingDirectory ? " (copied to working directory)" : "")));
        lastLoadedMidiPath = project.file.getFullPathName().replaceCharacter('\\', '/');
        saveLastLoadedMidiPathToPreset();
        updateWindowTitle();
    }

    void loadMidiFileFromPath(const juce::File& file, bool skipRenamePrompt = false)
    {
        if (!file.existsAsFile())
            return;

        invalidateDebouncedTrackMixPresetSave();

        if (!skipRenamePrompt && WorkingDirectoryCopy::needsCopyToWorkingDirectory(file, workingDirectory))
        {
            juce::Component::SafePointer<MainComponent> safeThis(this);
            showRenameBeforeCopyModal(file, file.getFileNameWithoutExtension(), {}, [safeThis](std::optional<juce::File> destination)
            {
                if (safeThis == nullptr)
                    return;

                if (!destination.has_value())
                {
                    safeThis->setStatusMessage("Load cancelled.");
                    return;
                }

                safeThis->finishLoadMidiFileFromPath(destination.value(), true);
            });
            return;
        }

        bool copiedToWorkingDirectory = false;
        const juce::File fileToLoad = copyMidiToWorkingDirectoryIfNeeded(file, copiedToWorkingDirectory);
        finishLoadMidiFileFromPath(fileToLoad, copiedToWorkingDirectory);
    }

    juce::String getSongPresetKey() const
    {
        return PresetFileStore::songKeyFromProjectFile(project.file);
    }

    juce::String getSongPresetTempoKey() const
    {
        return getSongPresetKey();
    }

    juce::String getSongTempoOverrideFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
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

    std::optional<int> getSongTransposeOverrideFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("transposeOverridesBySong"))
            return std::nullopt;

        auto* overridesObj = preset.getProperty("transposeOverridesBySong").getDynamicObject();
        if (overridesObj == nullptr)
            return std::nullopt;

        const juce::Identifier songId(songKey);
        if (!overridesObj->hasProperty(songId))
            return std::nullopt;

        return juce::jlimit(-24, 24, static_cast<int>(overridesObj->getProperty(songId)));
    }

    std::optional<juce::String> getSongKeyOverrideTextFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("keyOverridesBySong"))
            return std::nullopt;

        auto* overridesObj = preset.getProperty("keyOverridesBySong").getDynamicObject();
        if (overridesObj == nullptr)
            return std::nullopt;

        const juce::Identifier songId(songKey);
        if (!overridesObj->hasProperty(songId))
            return std::nullopt;

        return overridesObj->getProperty(songId).toString().trim();
    }

    std::optional<std::array<int, 3>> getSongStaffTrackSelectionsFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("staffTrackSelectionsBySong"))
            return std::nullopt;

        auto* bySongObj = preset.getProperty("staffTrackSelectionsBySong").getDynamicObject();
        if (bySongObj == nullptr)
            return std::nullopt;

        const juce::Identifier songId(songKey);
        if (!bySongObj->hasProperty(songId))
            return std::nullopt;

        auto* songSelectionObj = bySongObj->getProperty(songId).getDynamicObject();
        if (songSelectionObj == nullptr)
            return std::nullopt;

        std::array<int, 3> selections
            = { static_cast<int>(songSelectionObj->getProperty("staff1Track")),
                static_cast<int>(songSelectionObj->getProperty("staff2Track")),
                static_cast<int>(songSelectionObj->getProperty("staff3Track")) };
        return selections;
    }

    std::optional<juce::String> getSongChordTrackSelectionFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("chordTrackSelectionBySong"))
            return std::nullopt;

        auto* bySongObj = preset.getProperty("chordTrackSelectionBySong").getDynamicObject();
        if (bySongObj == nullptr)
            return std::nullopt;

        const juce::Identifier songId(songKey);
        if (!bySongObj->hasProperty(songId))
            return std::nullopt;

        return bySongObj->getProperty(songId).toString();
    }

    std::optional<int> getSongAccidentalSelectionFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("accidentalBySong"))
            return std::nullopt;

        auto* bySongObj = preset.getProperty("accidentalBySong").getDynamicObject();
        if (bySongObj == nullptr)
            return std::nullopt;

        const juce::Identifier songId(songKey);
        if (!bySongObj->hasProperty(songId))
            return std::nullopt;

        return juce::jlimit(1, 2, static_cast<int>(bySongObj->getProperty(songId)));
    }

    std::optional<int> getSongAliasSelectionFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("aliasBySong"))
            return std::nullopt;

        auto* bySongObj = preset.getProperty("aliasBySong").getDynamicObject();
        if (bySongObj == nullptr)
            return std::nullopt;

        const juce::Identifier songId(songKey);
        if (!bySongObj->hasProperty(songId))
            return std::nullopt;

        return juce::jlimit(1, 2, static_cast<int>(bySongObj->getProperty(songId)));
    }

    std::optional<int> getSongChordResolutionFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("chordResolutionBySong"))
            return std::nullopt;

        auto* bySongObj = preset.getProperty("chordResolutionBySong").getDynamicObject();
        if (bySongObj == nullptr)
            return std::nullopt;

        const juce::Identifier songId(songKey);
        if (!bySongObj->hasProperty(songId))
            return std::nullopt;

        return normalizeChordResolutionSelectorId(static_cast<int>(bySongObj->getProperty(songId)));
    }

    std::optional<int> getSongChordComplexityFromPreset(const juce::DynamicObject& preset) const
    {
        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("chordComplexityBySong"))
            return std::nullopt;

        auto* bySongObj = preset.getProperty("chordComplexityBySong").getDynamicObject();
        if (bySongObj == nullptr)
            return std::nullopt;

        const juce::Identifier songId(songKey);
        if (!bySongObj->hasProperty(songId))
            return std::nullopt;

        return normalizeChordComplexitySelectorId(static_cast<int>(bySongObj->getProperty(songId)));
    }

    void applyTrackMixFromPreset(const juce::DynamicObject& preset)
    {
        initializeTrackMixFromMidi();

        const auto songKey = getSongPresetKey();
        if (songKey.isEmpty() || !preset.hasProperty("trackMixBySong"))
            return;

        auto* mixBySong = preset.getProperty("trackMixBySong").getDynamicObject();
        if (mixBySong == nullptr)
            return;

        const juce::Identifier songId(songKey);
        if (!mixBySong->hasProperty(songId))
            return;

        const auto entriesVar = mixBySong->getProperty(songId);
        auto* entries = entriesVar.getArray();
        if (entries == nullptr)
            return;

        const int limit = juce::jmin(trackMixState.getTrackCount(), entries->size());
        for (int i = 0; i < limit; ++i)
        {
            auto* entryObj = entries->getReference(i).getDynamicObject();
            if (entryObj == nullptr)
                continue;

            if (entryObj->hasProperty("volume"))
                trackMixState.setVolume(i, static_cast<int>(entryObj->getProperty("volume")));
            if (entryObj->hasProperty("expression"))
                trackMixState.setExpression(i, static_cast<int>(entryObj->getProperty("expression")));
            if (entryObj->hasProperty("reverb"))
                trackMixState.setReverb(i, static_cast<int>(entryObj->getProperty("reverb")));
            if (entryObj->hasProperty("channel"))
                trackMixState.setChannel(i, static_cast<int>(entryObj->getProperty("channel")));
            if (entryObj->hasProperty("mute"))
                trackMixState.setMuted(i, static_cast<bool>(entryObj->getProperty("mute")));
            if (entryObj->hasProperty("solo"))
                trackMixState.setSolo(i, static_cast<bool>(entryObj->getProperty("solo")));
        }
    }

    juce::var buildTrackMixPresetEntries() const
    {
        juce::Array<juce::var> entries;
        for (int i = 0; i < trackMixState.getTrackCount(); ++i)
        {
            auto entry = std::make_unique<juce::DynamicObject>();
            entry->setProperty("volume", trackMixState.getVolume(i));
            entry->setProperty("expression", trackMixState.getExpression(i));
            entry->setProperty("reverb", trackMixState.getReverb(i));
            entry->setProperty("channel", trackMixState.getChannel(i));
            entry->setProperty("mute", trackMixState.isMuted(i));
            entry->setProperty("solo", trackMixState.isSolo(i));
            entries.add(juce::var(entry.release()));
        }
        return juce::var(entries);
    }

    void initializeTrackMixFromMidi()
    {
        if (project.tracks.empty())
        {
            trackMixState.resizeForTrackCount(0);
            return;
        }

        std::vector<juce::MidiMessageSequence> sequences;
        sequences.reserve(project.tracks.size());
        for (const auto& track : project.tracks)
            sequences.push_back(track.sequence);
        TrackMixMidiSeed::applyFromTrackSequences(sequences, trackMixState);
    }

    void saveUiPreset(bool showStatusMessage = true)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("staff1Track", staff1TrackSelector.getSelectedItemIndex());
        obj->setProperty("staff2Track", staff2TrackSelector.getSelectedItemIndex());
        obj->setProperty("staff3Track", staff3TrackSelector.getSelectedItemIndex());
        obj->setProperty("staffTrackSelectionVersion", 2);
        obj->setProperty("staff1Clef", staff1ClefSelector.getSelectedId());
        obj->setProperty("staff2Clef", staff2ClefSelector.getSelectedId());
        obj->setProperty("staff3Clef", staff3ClefSelector.getSelectedId());
        obj->setProperty("staff1DisplayOctave", normalizeStaffDisplayOctaveSelectorId(staff1OctaveSelector.getSelectedId()));
        obj->setProperty("staff2DisplayOctave", normalizeStaffDisplayOctaveSelectorId(staff2OctaveSelector.getSelectedId()));
        obj->setProperty("staff3DisplayOctave", normalizeStaffDisplayOctaveSelectorId(staff3OctaveSelector.getSelectedId()));
        obj->setProperty("chordTrackSelection", buildChordTrackSelectionCsv());
        obj->setProperty("accidental", accidentalSelector.getSelectedId());
        obj->setProperty("alias", aliasSelector.getSelectedId());
        obj->setProperty("chordComplexity", normalizeChordComplexitySelectorId(chordComplexitySelector.getSelectedId()));
        obj->setProperty("chordResolution", normalizeChordResolutionSelectorId(chordResolutionSelector.getSelectedId()));
        obj->setProperty("scoreLightMode", scoreLightMode);
        obj->setProperty("pdfExportMode", exportPdfModeSelector.getSelectedId());
        obj->setProperty("tempoOverrideEnabled", tempoOverrideBpm.has_value());
        obj->setProperty("tempoOverrideText", tempoOverrideInput.getText().trim());
        obj->setProperty("loopEnabled", loopEnabled);
        obj->setProperty("startupResumeEnabled", startupResumeEnabled);
        if (lastLoadedMidiPath.isNotEmpty())
            obj->setProperty("lastLoadedMidiPath", lastLoadedMidiPath);
        if (lastMidiDirectory.isDirectory())
            obj->setProperty("lastMidiDirectory", lastMidiDirectory.getFullPathName());
        if (workingDirectory.isDirectory())
            obj->setProperty("workingDirectory", workingDirectory.getFullPathName());
        {
            juce::Array<juce::var> recent;
            for (const auto& path : recentMidiFiles)
                recent.add(path);
            obj->setProperty("recentMidiFiles", juce::var(recent));
        }

        auto staffTrackSelectionsBySong = std::make_unique<juce::DynamicObject>();
        auto chordTrackSelectionBySong = std::make_unique<juce::DynamicObject>();
        auto accidentalBySong = std::make_unique<juce::DynamicObject>();
        auto aliasBySong = std::make_unique<juce::DynamicObject>();
        auto chordComplexityBySong = std::make_unique<juce::DynamicObject>();
        auto chordResolutionBySong = std::make_unique<juce::DynamicObject>();
        auto transposeOverridesBySong = std::make_unique<juce::DynamicObject>();
        auto keyOverridesBySong = std::make_unique<juce::DynamicObject>();
        auto tempoOverridesBySong = std::make_unique<juce::DynamicObject>();
        auto trackMixBySong = std::make_unique<juce::DynamicObject>();
        auto loopBySong = std::make_unique<juce::DynamicObject>();
        const auto file = PresetFileStore::getPresetFilePath();
        if (file.existsAsFile())
        {
            const auto existingJsonText = file.loadFileAsString();
            juce::var existingParsed;
            const auto existingParseResult = juce::JSON::parse(existingJsonText, existingParsed);
            if (!existingParseResult.failed() && existingParsed.isObject())
            {
                if (auto* existingObj = existingParsed.getDynamicObject())
                {
                    const auto existingStaffSelectionsVar = existingObj->getProperty("staffTrackSelectionsBySong");
                    if (auto* existingStaffSelectionsObj = existingStaffSelectionsVar.getDynamicObject())
                    {
                        for (const auto& prop : existingStaffSelectionsObj->getProperties())
                            staffTrackSelectionsBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingChordSelectionVar = existingObj->getProperty("chordTrackSelectionBySong");
                    if (auto* existingChordSelectionObj = existingChordSelectionVar.getDynamicObject())
                    {
                        for (const auto& prop : existingChordSelectionObj->getProperties())
                            chordTrackSelectionBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingAccidentalVar = existingObj->getProperty("accidentalBySong");
                    if (auto* existingAccidentalObj = existingAccidentalVar.getDynamicObject())
                    {
                        for (const auto& prop : existingAccidentalObj->getProperties())
                            accidentalBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingAliasVar = existingObj->getProperty("aliasBySong");
                    if (auto* existingAliasObj = existingAliasVar.getDynamicObject())
                    {
                        for (const auto& prop : existingAliasObj->getProperties())
                            aliasBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingChordComplexityVar = existingObj->getProperty("chordComplexityBySong");
                    if (auto* existingChordComplexityObj = existingChordComplexityVar.getDynamicObject())
                    {
                        for (const auto& prop : existingChordComplexityObj->getProperties())
                            chordComplexityBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingChordResolutionVar = existingObj->getProperty("chordResolutionBySong");
                    if (auto* existingChordResolutionObj = existingChordResolutionVar.getDynamicObject())
                    {
                        for (const auto& prop : existingChordResolutionObj->getProperties())
                            chordResolutionBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingTransposeVar = existingObj->getProperty("transposeOverridesBySong");
                    if (auto* existingTransposeObj = existingTransposeVar.getDynamicObject())
                    {
                        for (const auto& prop : existingTransposeObj->getProperties())
                            transposeOverridesBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingKeyVar = existingObj->getProperty("keyOverridesBySong");
                    if (auto* existingKeyObj = existingKeyVar.getDynamicObject())
                    {
                        for (const auto& prop : existingKeyObj->getProperties())
                            keyOverridesBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingOverridesVar = existingObj->getProperty("tempoOverridesBySong");
                    if (auto* existingOverridesObj = existingOverridesVar.getDynamicObject())
                    {
                        for (const auto& prop : existingOverridesObj->getProperties())
                            tempoOverridesBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingTrackMixVar = existingObj->getProperty("trackMixBySong");
                    if (auto* existingTrackMixObj = existingTrackMixVar.getDynamicObject())
                    {
                        for (const auto& prop : existingTrackMixObj->getProperties())
                            trackMixBySong->setProperty(prop.name, prop.value);
                    }

                    const auto existingLoopVar = existingObj->getProperty("loopBySong");
                    if (auto* existingLoopObj = existingLoopVar.getDynamicObject())
                    {
                        for (const auto& prop : existingLoopObj->getProperties())
                            loopBySong->setProperty(prop.name, prop.value);
                    }
                }
            }
        }

        const auto songKey = getSongPresetKey();
        if (songKey.isNotEmpty())
        {
            const juce::Identifier songId(songKey);
            auto songStaffSelections = std::make_unique<juce::DynamicObject>();
            songStaffSelections->setProperty("staff1Track", staff1TrackSelector.getSelectedItemIndex());
            songStaffSelections->setProperty("staff2Track", staff2TrackSelector.getSelectedItemIndex());
            songStaffSelections->setProperty("staff3Track", staff3TrackSelector.getSelectedItemIndex());
            staffTrackSelectionsBySong->setProperty(songId, juce::var(songStaffSelections.release()));
            chordTrackSelectionBySong->setProperty(songId, buildChordTrackSelectionCsv());
            accidentalBySong->setProperty(songId, accidentalSelector.getSelectedId());
            aliasBySong->setProperty(songId, aliasSelector.getSelectedId());
            chordComplexityBySong->setProperty(songId, normalizeChordComplexitySelectorId(chordComplexitySelector.getSelectedId()));
            chordResolutionBySong->setProperty(songId, normalizeChordResolutionSelectorId(chordResolutionSelector.getSelectedId()));

            const int songTranspose = getGlobalTransposeSemitones(true);
            if (songTranspose != 0)
                transposeOverridesBySong->setProperty(songId, songTranspose);
            else
                transposeOverridesBySong->removeProperty(songId);

            if (keyOverride.has_value())
                keyOverridesBySong->setProperty(songId, keyOverride->displayText);
            else
                keyOverridesBySong->removeProperty(songId);

            if (tempoOverrideBpm.has_value())
                tempoOverridesBySong->setProperty(songId, juce::String(tempoOverrideBpm.value(), 1));
            else
                tempoOverridesBySong->removeProperty(songId);

            trackMixBySong->setProperty(songId, buildTrackMixPresetEntries());
            auto songLoop = std::make_unique<juce::DynamicObject>();
            songLoop->setProperty("enabled", loopEnabled);
            songLoop->setProperty("startBar", loopStartBar);
            songLoop->setProperty("endBar", loopEndBar);
            loopBySong->setProperty(songId, juce::var(songLoop.release()));
        }
        obj->setProperty("staffTrackSelectionsBySong", juce::var(staffTrackSelectionsBySong.release()));
        obj->setProperty("chordTrackSelectionBySong", juce::var(chordTrackSelectionBySong.release()));
        obj->setProperty("accidentalBySong", juce::var(accidentalBySong.release()));
        obj->setProperty("aliasBySong", juce::var(aliasBySong.release()));
        obj->setProperty("chordComplexityBySong", juce::var(chordComplexityBySong.release()));
        obj->setProperty("chordResolutionBySong", juce::var(chordResolutionBySong.release()));
        obj->setProperty("transposeOverridesBySong", juce::var(transposeOverridesBySong.release()));
        obj->setProperty("keyOverridesBySong", juce::var(keyOverridesBySong.release()));
        obj->setProperty("tempoOverridesBySong", juce::var(tempoOverridesBySong.release()));
        obj->setProperty("trackMixBySong", juce::var(trackMixBySong.release()));
        obj->setProperty("loopBySong", juce::var(loopBySong.release()));

        const juce::var payload(obj.release());
        juce::String writeError;
        const bool saveOk = PresetFileStore::writeTextFileAtomically(file, juce::JSON::toString(payload), writeError);
        if (saveOk)
        {
            markCurrentScoreSongSettingsAsSaved();
            if (songKey.isNotEmpty())
                markTrackMixAsSaved();
        }
        else
        {
            trackMixSaveStatus = TrackMixSaveStatus::failed;
        }
        refreshSavePresetButtonDirtyStyle();
        if (showStatusMessage)
        {
            if (saveOk)
                setStatusMessage("Preset saved: " + file.getFullPathName());
            else
                setStatusMessage("Failed to save preset.");
        }
        else if (!saveOk)
        {
            setStatusMessage("Preset autosave failed. Use Save Preset to retry.");
            juce::Logger::writeToLog("MidiScorer: preset autosave failed: " + writeError);
        }
    }

    bool loadUiPreset(bool silent = false)
    {
        invalidateDebouncedTrackMixPresetSave();
        const auto file = PresetFileStore::getPresetFilePath();
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

        const int staffTrackSelectionVersion = getIntProperty("staffTrackSelectionVersion", 1);
        auto normalizeStoredStaffSelection = [staffTrackSelectionVersion](int storedSelection) -> int
        {
            if (staffTrackSelectionVersion < 2 && storedSelection >= 0)
                return storedSelection + 1;
            return storedSelection;
        };

        auto applyTrackSelection = [this, &normalizeStoredStaffSelection](juce::ComboBox& box, int idx)
        {
            const int normalized = normalizeStoredStaffSelection(idx);
            const int clamped = juce::jlimit(0, juce::jmax(0, box.getNumItems() - 1), normalized);
            if (box.getNumItems() > 0)
                box.setSelectedItemIndex(clamped, juce::dontSendNotification);
        };

        const auto songStaffSelections = getSongStaffTrackSelectionsFromPreset(*obj);
        if (songStaffSelections.has_value())
        {
            applyTrackSelection(staff1TrackSelector, (*songStaffSelections)[0]);
            applyTrackSelection(staff2TrackSelector, (*songStaffSelections)[1]);
            applyTrackSelection(staff3TrackSelector, (*songStaffSelections)[2]);
        }
        else
        {
            const bool hasSongStaffSelectionMap = obj->hasProperty("staffTrackSelectionsBySong")
                && obj->getProperty("staffTrackSelectionsBySong").getDynamicObject() != nullptr;
            if (!hasSongStaffSelectionMap)
            {
                applyTrackSelection(staff1TrackSelector, getIntProperty("staff1Track", 0));
                applyTrackSelection(staff2TrackSelector, getIntProperty("staff2Track", 0));
                applyTrackSelection(staff3TrackSelector, getIntProperty("staff3Track", 0));
            }
        }

        staff1ClefSelector.setSelectedId(getIntProperty("staff1Clef", 1), juce::dontSendNotification);
        staff2ClefSelector.setSelectedId(getIntProperty("staff2Clef", 1), juce::dontSendNotification);
        staff3ClefSelector.setSelectedId(getIntProperty("staff3Clef", 1), juce::dontSendNotification);
        staff1OctaveSelector.setSelectedId(normalizeStaffDisplayOctaveSelectorId(getIntProperty("staff1DisplayOctave", 1)),
                                           juce::dontSendNotification);
        staff2OctaveSelector.setSelectedId(normalizeStaffDisplayOctaveSelectorId(getIntProperty("staff2DisplayOctave", 1)),
                                           juce::dontSendNotification);
        staff3OctaveSelector.setSelectedId(normalizeStaffDisplayOctaveSelectorId(getIntProperty("staff3DisplayOctave", 1)),
                                           juce::dontSendNotification);
        applyStaffDisplayOctaveFromSelectors(true);
        const auto songTransposeOverride = getSongTransposeOverrideFromPreset(*obj);
        if (songTransposeOverride.has_value())
        {
            globalTransposeInput.setText(juce::String(songTransposeOverride.value()), juce::dontSendNotification);
        }
        else
        {
            const bool hasSongTransposeMap = obj->hasProperty("transposeOverridesBySong")
                && obj->getProperty("transposeOverridesBySong").getDynamicObject() != nullptr;
            globalTransposeInput.setText(
                juce::String(hasSongTransposeMap ? 0 : juce::jlimit(-24, 24, getIntProperty("globalTranspose", 0))),
                juce::dontSendNotification);
        }
        const auto songChordTrackSelection = getSongChordTrackSelectionFromPreset(*obj);
        if (songChordTrackSelection.has_value())
        {
            applyChordTrackSelectionCsv(songChordTrackSelection.value());
        }
        else
        {
            const bool hasSongChordSelectionMap = obj->hasProperty("chordTrackSelectionBySong")
                && obj->getProperty("chordTrackSelectionBySong").getDynamicObject() != nullptr;
            if (!hasSongChordSelectionMap)
            {
                if (obj->hasProperty("chordTrackSelection"))
                    applyChordTrackSelectionCsv(obj->getProperty("chordTrackSelection").toString());
                else
                    applyChordTrackSelectionCsv("0,1,2,3,4");
            }
        }
        const auto songAccidentalSelection = getSongAccidentalSelectionFromPreset(*obj);
        if (songAccidentalSelection.has_value())
        {
            accidentalSelector.setSelectedId(songAccidentalSelection.value(), juce::dontSendNotification);
        }
        else
        {
            const bool hasSongAccidentalMap = obj->hasProperty("accidentalBySong")
                && obj->getProperty("accidentalBySong").getDynamicObject() != nullptr;
            accidentalSelector.setSelectedId(
                hasSongAccidentalMap ? 1 : juce::jlimit(1, 2, getIntProperty("accidental", 1)),
                juce::dontSendNotification);
        }

        const auto songAliasSelection = getSongAliasSelectionFromPreset(*obj);
        if (songAliasSelection.has_value())
        {
            aliasSelector.setSelectedId(songAliasSelection.value(), juce::dontSendNotification);
        }
        else
        {
            const bool hasSongAliasMap = obj->hasProperty("aliasBySong")
                && obj->getProperty("aliasBySong").getDynamicObject() != nullptr;
            aliasSelector.setSelectedId(
                hasSongAliasMap ? 1 : juce::jlimit(1, 2, getIntProperty("alias", 1)),
                juce::dontSendNotification);
        }

        const auto songChordComplexitySelection = getSongChordComplexityFromPreset(*obj);
        if (songChordComplexitySelection.has_value())
        {
            chordComplexitySelector.setSelectedId(songChordComplexitySelection.value(), juce::dontSendNotification);
        }
        else
        {
            const bool hasSongChordComplexityMap = obj->hasProperty("chordComplexityBySong")
                && obj->getProperty("chordComplexityBySong").getDynamicObject() != nullptr;
            chordComplexitySelector.setSelectedId(
                hasSongChordComplexityMap
                    ? static_cast<int>(ChordDetector::ChordComplexity::rich)
                    : normalizeChordComplexitySelectorId(
                          getIntProperty("chordComplexity", static_cast<int>(ChordDetector::ChordComplexity::rich))),
                juce::dontSendNotification);
        }

        const auto songChordResolutionSelection = getSongChordResolutionFromPreset(*obj);
        if (songChordResolutionSelection.has_value())
        {
            chordResolutionSelector.setSelectedId(songChordResolutionSelection.value(), juce::dontSendNotification);
        }
        else
        {
            const bool hasSongChordResolutionMap = obj->hasProperty("chordResolutionBySong")
                && obj->getProperty("chordResolutionBySong").getDynamicObject() != nullptr;
            chordResolutionSelector.setSelectedId(
                hasSongChordResolutionMap
                    ? static_cast<int>(ChordDetector::DetectionResolution::quarter)
                    : normalizeChordResolutionSelectorId(
                          getIntProperty("chordResolution", static_cast<int>(ChordDetector::DetectionResolution::quarter))),
                juce::dontSendNotification);
        }
        assignToggle.setToggleState(false, juce::dontSendNotification);
        keyOverrideProfileOnly = false;
        suppressProfileOnlyExitUntilKeyChange = false;
        keyTransposeReferenceTonicPc = getMidiDetectedTonicPc();
        exportPdfModeSelector.setSelectedId(
            juce::jlimit(static_cast<int>(PdfExportMode::allActiveStaffs),
                         static_cast<int>(PdfExportMode::staff1Only),
                         getIntProperty("pdfExportMode", static_cast<int>(PdfExportMode::allActiveStaffs))),
            juce::dontSendNotification);
        scoreLightMode = getIntProperty("scoreLightMode", 0) != 0;
        if (obj->hasProperty("lastMidiDirectory"))
        {
            const juce::File candidate(obj->getProperty("lastMidiDirectory").toString());
            if (candidate.isDirectory())
                lastMidiDirectory = candidate;
        }
        if (obj->hasProperty("workingDirectory"))
        {
            const juce::File candidate(obj->getProperty("workingDirectory").toString());
            if (candidate.isDirectory())
                workingDirectory = candidate;
        }
        const auto songKeyOverrideText = getSongKeyOverrideTextFromPreset(*obj);
        if (songKeyOverrideText.has_value() && songKeyOverrideText->isNotEmpty())
            keyOverrideInput.setText(songKeyOverrideText.value(), juce::dontSendNotification);
        else
        {
            const bool hasSongKeyMap = obj->hasProperty("keyOverridesBySong")
                && obj->getProperty("keyOverridesBySong").getDynamicObject() != nullptr;
            if (!hasSongKeyMap)
            {
                const bool keyOverrideEnabled = getIntProperty("keyOverrideEnabled", 0) != 0;
                if (keyOverrideEnabled && obj->hasProperty("keyOverrideText"))
                    keyOverrideInput.setText(obj->getProperty("keyOverrideText").toString(), juce::dontSendNotification);
                else
                    keyOverrideInput.setText({}, juce::dontSendNotification);
            }
            else
            {
                keyOverrideInput.setText({}, juce::dontSendNotification);
            }
        }
        applyKeyOverrideFromInput(false);
        syncKeyTransposeReferenceToOverrideOrMidi();
        if (keyOverride.has_value() && !assignToggle.getToggleState() && !keyOverrideProfileOnly)
        {
            keyTransposeAppliedSemitones = KeyOverrideTranspose::normalizeSemitoneDelta(
                keyOverride->tonicPc - getMidiDetectedTonicPc());
        }
        else
        {
            keyTransposeAppliedSemitones = 0;
        }
        pendingKeyTransposeReferenceTonicPc = std::nullopt;
        const auto songTempoOverride = getSongTempoOverrideFromPreset(*obj);
        const bool applySongTempoOverride = !silent && songTempoOverride.isNotEmpty();
        if (applySongTempoOverride)
            tempoOverrideInput.setText(songTempoOverride, juce::dontSendNotification);
        else
            tempoOverrideInput.setText({}, juce::dontSendNotification);
        applyTempoOverrideFromInput(false);

        loopEnabled = getIntProperty("loopEnabled", 0) != 0;
        loopStartBar = 1;
        loopEndBar = juce::jmax(1, project.maxBar);
        if (obj->hasProperty("loopBySong"))
        {
            if (auto* loopBySong = obj->getProperty("loopBySong").getDynamicObject())
            {
                const auto songKey = getSongPresetKey();
                if (songKey.isNotEmpty())
                {
                    const juce::Identifier songId(songKey);
                    if (loopBySong->hasProperty(songId))
                    {
                        if (auto* songLoop = loopBySong->getProperty(songId).getDynamicObject())
                        {
                            loopEnabled = static_cast<bool>(songLoop->getProperty("enabled"));
                            loopStartBar = juce::jmax(1, static_cast<int>(songLoop->getProperty("startBar")));
                            loopEndBar = juce::jmax(loopStartBar, static_cast<int>(songLoop->getProperty("endBar")));
                        }
                    }
                }
            }
        }
        loopEnabledToggle.setToggleState(loopEnabled, juce::dontSendNotification);
        loopStartInput.setText(juce::String(loopStartBar), juce::dontSendNotification);
        loopEndInput.setText(juce::String(loopEndBar), juce::dontSendNotification);
        applyLoopBoundsFromInputs(false);

        applyTrackMixFromPreset(*obj);
        markTrackMixAsSaved();
        applyScoreColorScheme();

        rebuildAllStaffs();
        markCurrentScoreSongSettingsAsSaved();
        refreshSavePresetButtonDirtyStyle();
        if (!silent)
            setStatusMessage("Preset loaded.");
        return true;
    }

    std::vector<MidiNoteEvent> collectChordAnalysisNotes(int selectedTrackIndex)
    {
        return collectChordAnalysisNotesInternal(true, selectedTrackIndex);
    }

    std::vector<MidiNoteEvent> collectSharedChordAnalysisNotes() const
    {
        return collectChordAnalysisNotesInternal(false, -1);
    }

    bool hasChordTrackSelection() const
    {
        for (auto* button : chordTrackButtons)
        {
            if (button->getToggleState())
                return true;
        }
        return false;
    }

    std::vector<MidiNoteEvent> collectChordAnalysisNotesInternal(bool allowStaffFallback, int selectedTrackIndex) const
    {
        std::vector<MidiNoteEvent> mergedNotes;
        juce::Array<int> selectedIndices;
        for (int i = 0; i < chordTrackButtons.size(); ++i)
        {
            if (chordTrackButtons[i]->getToggleState())
                selectedIndices.add(chordTrackSourceIndices[i]);
        }

        if (selectedIndices.isEmpty())
        {
            if (!allowStaffFallback || selectedTrackIndex < 0 || selectedTrackIndex >= static_cast<int>(project.tracks.size()))
                return mergedNotes;
            selectedIndices.add(selectedTrackIndex);
        }

        size_t reserveCount = 0;
        for (int idx : selectedIndices)
            reserveCount += project.tracks[(size_t) idx].notes.size();
        mergedNotes.reserve(reserveCount);

        for (int idx : selectedIndices)
        {
            const auto& notes = project.tracks[(size_t) idx].notes;
            mergedNotes.insert(mergedNotes.end(), notes.begin(), notes.end());
        }

        const int transposeSemitones = getClampedTranspose(globalTransposeInput) + getKeyOverrideTransposeSemitones();
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
        if (playbackPositionSource == nullptr || !playbackPositionSource->isPlaying())
            return;

        if (loopEnabled && !project.tracks.empty())
        {
            const int currentBar = playbackPositionSource->getCurrentBar();
            TransportCoordinator::handleLoopWrap(buildTransportContext(), currentBar);
        }

        if (playbackPositionSource->hasReachedEnd())
        {
            stopPlayback(false);
            setStatusMessage("Playback finished.");
            return;
        }

        const double elapsedSec = playbackPositionSource->getElapsedSeconds();
        auto midiDispatch = midiPlaybackEngine.processUntilPlaybackTime(
            elapsedSec,
            [this](const MidiFilePlaybackEngineAdapter::ScheduledEvent& event)
        {
                if (!TrackMixProcessor::shouldSendTrack(event.sourceTrackIndex, trackMixState))
                    return;

                const auto mixedMessage = TrackMixProcessor::applyVolumeToMessage(event.message, event.sourceTrackIndex, trackMixState);
                midiOutputDevice.sendMessageNow(mixedMessage);
            });
        if (midiDispatch.reachedEndOfStream && !midiPlaybackEngine.hasPendingEvents())
        {
            stopPlayback(false);
            setStatusMessage("Playback finished.");
            return;
        }

        const int bar = playbackPositionSource->getCurrentBar();
        scoreRenderer.setCurrentBar(bar);
        scoreRenderer2.setCurrentBar(bar);
        scoreRenderer3.setCurrentBar(bar);
        transportLabel.setText("Bar " + juce::String(bar), juce::dontSendNotification);
        displayedBar = bar;

        if (!project.tracks.empty())
        {
            const auto namingOptions = getChordNamingOptions();
            const auto resolution = getChordDetectionResolution();
            const double windowQuarterLength = ChordDetector::windowQuarterLength(resolution);
            const double quarter = project.tempoMap.secondsToQuarter(elapsedSec);
            const int windowIndex = static_cast<int>(std::floor((quarter / windowQuarterLength) + 1.0e-6));
            const double windowQuarterStart = static_cast<double>(windowIndex) * windowQuarterLength;
            const double windowQuarterEnd = windowQuarterStart + windowQuarterLength;
            const double windowSecStart = project.tempoMap.tickToSeconds(project.tempoMap.quarterToTick(windowQuarterStart));
            const double windowSecEnd = project.tempoMap.tickToSeconds(project.tempoMap.quarterToTick(windowQuarterEnd));

            auto updateLiveChordForStaff = [&](int staffIndex, ScoreRenderer& renderer)
            {
                auto& state = liveChordStates[(size_t) staffIndex];
                if (state.lastWindowIndex == windowIndex)
                    return;
                state.lastWindowIndex = windowIndex;

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
        const auto defaultDir = workingDirectory.isDirectory()
            ? workingDirectory
            : (lastMidiDirectory.isDirectory() ? lastMidiDirectory : juce::File());
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select a MIDI file",
            defaultDir,
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
            loadMidiFileFromPath(file);
        });
    }

    std::vector<ScorePdfExporter::StaffExportLane> buildScoreExportLanes(PdfExportMode mode) const
    {
        const bool showStaff1 = isStaffDisplayed(staff1TrackSelector);
        const bool showStaff2 = isStaffDisplayed(staff2TrackSelector);
        const bool showStaff3 = isStaffDisplayed(staff3TrackSelector);
        std::vector<ScorePdfExporter::StaffExportLane> lanes;
        if (mode == PdfExportMode::staff1Only)
        {
            lanes.reserve(1);
            if (showStaff1 && !scoreModel1.empty())
                lanes.push_back({ &scoreModel1, &scoreRenderer });
            return lanes;
        }

        lanes.reserve(3);
        if (showStaff1 && !scoreModel1.empty())
            lanes.push_back({ &scoreModel1, &scoreRenderer });
        if (showStaff2 && !scoreModel2.empty())
            lanes.push_back({ &scoreModel2, &scoreRenderer2 });
        if (showStaff3 && !scoreModel3.empty())
            lanes.push_back({ &scoreModel3, &scoreRenderer3 });

        return lanes;
    }

    void exportScorePdf()
    {
        if (!hasLoadedProject())
        {
            showWarningModal(this, "Export PDF", "Load a MIDI file before exporting a score PDF.");
            return;
        }

        const auto exportMode = pdfExportModeFromSelectorId(exportPdfModeSelector.getSelectedId());
        const auto lanes = buildScoreExportLanes(exportMode);
        if (lanes.empty())
        {
            const auto message = exportMode == PdfExportMode::staff1Only
                ? juce::String("Staff 1 has no rendered notation to export.")
                : juce::String("No rendered staff notation is available to export.");
            showWarningModal(this, "Export PDF", message);
            return;
        }

        const auto defaultDir = lastMidiDirectory.isDirectory()
            ? lastMidiDirectory
            : project.file.getParentDirectory();
        const auto baseName = project.file.existsAsFile()
            ? project.file.getFileNameWithoutExtension()
            : juce::String("score");
        const auto fileSuffix = exportMode == PdfExportMode::staff1Only
            ? juce::String("_staff1_score.pdf")
            : juce::String("_score.pdf");
        const auto defaultFile = defaultDir.getChildFile(baseName + fileSuffix);

        exportPdfFileChooser = std::make_unique<juce::FileChooser>(
            "Export score PDF",
            defaultFile,
            "*.pdf",
            false,
            false,
            this);

        const auto chooserFlags = juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting;
        exportPdfFileChooser->launchAsync(chooserFlags, [this, exportMode](const juce::FileChooser& chooser)
        {
            auto outputFile = chooser.getResult();
            if (outputFile == juce::File())
                return;
            if (!outputFile.hasFileExtension("pdf"))
                outputFile = outputFile.withFileExtension(".pdf");

            juce::String error;
            int pageCount = 0;
            ScorePdfExporter::ExportOptions options;
            options.title = project.file.existsAsFile()
                ? project.file.getFileNameWithoutExtension()
                : juce::String("MidiScorer Score");
            options.subtitle = "Bars 1-" + juce::String(juce::jmax(1, project.maxBar));
            if (!ScorePdfExporter::exportToFile(outputFile, buildScoreExportLanes(exportMode), options, error, &pageCount))
            {
                const auto message = error.isNotEmpty() ? error : juce::String("Failed to export PDF.");
                showWarningModal(this, "Export PDF", message);
                return;
            }

            lastMidiDirectory = outputFile.getParentDirectory();
            setStatusMessage("Exported PDF: "
                             + outputFile.getFileName()
                             + " ("
                             + juce::String(pageCount)
                             + (pageCount == 1 ? " page)" : " pages)"));
        });
    }

    void refreshTrackSelectors()
    {
        staff1TrackSelector.clear(juce::dontSendNotification);
        staff2TrackSelector.clear(juce::dontSendNotification);
        staff3TrackSelector.clear(juce::dontSendNotification);
        staff1TrackSelector.addItem("No Display", 1);
        staff2TrackSelector.addItem("No Display", 1);
        staff3TrackSelector.addItem("No Display", 1);
        int itemId = 2;
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

        staff1TrackSelector.setSelectedItemIndex(1, juce::dontSendNotification);
        staff2TrackSelector.setSelectedItemIndex(juce::jmin(2, trackCount), juce::dontSendNotification);
        staff3TrackSelector.setSelectedItemIndex(juce::jmin(3, trackCount), juce::dontSendNotification);
    }

    void clearStaff(ScoreModel& model, ScoreRenderer& renderer)
    {
        model.clear();
        renderer.setKeySignature(false, 0);
        renderer.setStaffLabel({});
        renderer.repaint();
    }

    TransportCoordinator::Context buildTransportContext()
    {
        TransportCoordinator::Context ctx;
        ctx.project = &project;
        ctx.playbackController = &playbackController;
        ctx.midiPlaybackEngine = &midiPlaybackEngine;
        ctx.midiOutputDevice = &midiOutputDevice;
        ctx.renderers[0] = &scoreRenderer;
        ctx.renderers[1] = &scoreRenderer2;
        ctx.renderers[2] = &scoreRenderer3;
        ctx.transportToggleButton = &transportToggleButton;
        ctx.continueButton = &continueButton;
        ctx.continueBarInput = &continueBarInput;
        ctx.loopEnabledToggle = &loopEnabledToggle;
        ctx.loopStartInput = &loopStartInput;
        ctx.loopEndInput = &loopEndInput;
        ctx.transportLabel = &transportLabel;
        ctx.continueArmed = &continueArmed;
        ctx.loopEnabled = &loopEnabled;
        ctx.loopStartBar = &loopStartBar;
        ctx.loopEndBar = &loopEndBar;
        ctx.displayedBar = &displayedBar;
        ctx.setStatusMessage = [this](const juce::String& message) { setStatusMessage(message); };
        ctx.resetLiveChordState = [this]() { resetLiveChordState(); };
        ctx.updateTransportControls = [this]() { updateTransportControls(); };
        return ctx;
    }

    ScoreRebuildService::Context buildScoreRebuildContext()
    {
        ScoreRebuildService::Context ctx;
        ctx.project = &project;
        ctx.playbackController = &playbackController;
        ctx.liveChordNotesByStaff = &liveChordNotesByStaff;
        ctx.clearStaff = [this](int, ScoreModel& model, ScoreRenderer& renderer) { clearStaff(model, renderer); };
        ctx.effectiveTransposeForTrack = [this](int)
        {
            return getEffectiveTransposeSemitones(true);
        };
        ctx.collectChordAnalysisNotes = [this](int trackIndex) { return collectChordAnalysisNotes(trackIndex); };
        ctx.collectSharedChordAnalysisNotes = [this]() { return collectSharedChordAnalysisNotes(); };
        ctx.hasChordTrackSelection = [this]() { return hasChordTrackSelection(); };
        ctx.namingOptions = [this]() { return getChordNamingOptions(); };
        ctx.chordResolution = [this]() { return getChordDetectionResolution(); };
        ctx.resetLiveChordState = [this]() { resetLiveChordState(); };
        ctx.setStatusMessage = [this](const juce::String& message) { setStatusMessage(message); };
        ctx.transportLabel = &transportLabel;
        ctx.keyLabel = &keyLabel;
        ctx.midiMetaLabel = &midiMetaLabel;
        ctx.chordTrackButtons = &chordTrackButtons;
        ctx.displayedBar = &displayedBar;
        ctx.hasKeyOverride = keyOverride.has_value();
        ctx.keySharpsOrFlats = keyOverride.has_value() ? keyOverride->sharpsOrFlats : 0;
        ctx.keyDisplayText = keyOverride.has_value() ? keyOverride->displayText : project.getKeyDisplayText();
        ctx.midiMetaText = buildMidiMetaText();
        ctx.refreshSavePresetButtonDirtyStyle = [this]() { refreshSavePresetButtonDirtyStyle(); };
        ctx.hasLoadedProject = [this]() { return hasLoadedProject(); };
        return ctx;
    }

    std::array<ScoreRebuildService::StaffLane, 3> buildScoreRebuildLanes()
    {
        return { {
            { &staff1TrackSelector, &staff1ClefSelector, &scoreModel1, &scoreRenderer, 0 },
            { &staff2TrackSelector, &staff2ClefSelector, &scoreModel2, &scoreRenderer2, 1 },
            { &staff3TrackSelector, &staff3ClefSelector, &scoreModel3, &scoreRenderer3, 2 },
        } };
    }

    void rebuildAllStaffs()
    {
        auto ctx = buildScoreRebuildContext();
        ScoreRebuildService::rebuildAllStaffs(ctx, buildScoreRebuildLanes());
        if (pendingKeyTransposeReferenceTonicPc.has_value())
        {
            keyTransposeReferenceTonicPc = pendingKeyTransposeReferenceTonicPc.value();
            pendingKeyTransposeReferenceTonicPc = std::nullopt;
        }
        resized();
    }

    void applyScoreColorScheme()
    {
        const auto scheme = scoreLightMode
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

    static void showWarningModal(juce::Component* parent, const juce::String& title, const juce::String& message)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
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
                      "This value stacks with Key override when Assign is unchecked.");
    }

    void showKeyHelpModal()
    {
        showInfoModal(this,
                      "Key Override Help",
                      "Override the song key for notation and transposition.\n\n"
                      "Examples: C, Eb, F#, Cm, Ebm\n"
                      "Use b for flats (Eb, Bb) and # for sharps (F#).\n"
                      "Add m for minor keys.\n\n"
                      "Leave blank to use the key from the MIDI file (Assign auto-checks).\n"
                      "Assign checked: key text is saved for profile/reference only.\n"
                      "Assign unchecked: app transposes by the difference between MIDI key and your override.");
    }

    void showTempoHelpModal()
    {
        showInfoModal(this,
                      "Tempo Override Help",
                      "Enter a BPM value from 10 to 400 to override playback tempo.\n\n"
                      "Leave blank to use the tempo map from the MIDI file.\n"
                      "Per-song tempo overrides are saved in your preset profile.\n\n"
                      "Override BPM is measured against the file's opening tempo and scales playback "
                      "uniformly. Songs with later tempo changes keep their internal tempo ratios; "
                      "notation and bar positions stay tied to the source map.");
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
        TransportCoordinator::startPlayback(buildTransportContext());
        if (playbackController.isPlaying())
            setScoreControlRowsHidden(true);
    }

    void continuePlaybackFromBar()
    {
        TransportCoordinator::continuePlaybackFromBar(buildTransportContext());
        if (playbackController.isPlaying())
            setScoreControlRowsHidden(true);
    }

    void stopPlayback(bool userInitiated)
    {
        TransportCoordinator::stopPlayback(buildTransportContext(), userInitiated);
    }

    void onTransportToggleClicked()
    {
        TransportCoordinator::onTransportToggleClicked(buildTransportContext());
    }

    void refreshTransportToggleButtonText()
    {
        TransportCoordinator::refreshTransportToggleButtonText(buildTransportContext());
    }

    void refreshTransportToggleButtonStyle(bool hasProject, bool isPlaying)
    {
        TransportCoordinator::refreshTransportToggleButtonStyle(buildTransportContext(), hasProject, isPlaying);
    }

    void updateTransportControls()
    {
        TransportCoordinator::updateTransportControls(buildTransportContext());
    }

    juce::String buildSignatureText() const
    {
        if (project.tracks.empty())
            return "Sig: n/a";

        const auto& sigs = project.tempoMap.getTimeSignatureEvents();
        if (sigs.empty())
            return "Sig: n/a";

        return "Sig: " + juce::String(sigs.front().numerator) + "/" + juce::String(sigs.front().denominator);
    }

    juce::String buildTempoMetaText() const
    {
        if (project.tracks.empty())
            return "Tempo: n/a";

        const auto& tempos = project.tempoMap.getTempoEvents();
        if (tempos.empty())
            return "Tempo: n/a";

        const double detectedBpm = tempos.front().bpm;
        juce::String detectedText = formatTempoBpm(detectedBpm);
        if (project.tempoMap.hasMultipleTempoEvents())
        {
            detectedText = formatTempoBpm(detectedBpm) + " (multi-tempo)";
        }

        juce::String tempoText = detectedText;
        if (tempoOverrideBpm.has_value())
        {
            tempoText = formatTempoBpm(tempoOverrideBpm.value()) + " (opening " + formatTempoBpm(detectedBpm);
            if (project.tempoMap.hasMultipleTempoEvents())
                tempoText += ", multi-tempo map";
            tempoText += ")";
        }
        return "Tempo: " + tempoText;
    }

    juce::String buildMidiMetaText() const
    {
        return buildSignatureText() + "  " + buildTempoMetaText();
    }

    juce::String buildStatusDetailsText() const
    {
        const auto keyText = keyOverride.has_value()
            ? ("KeySrc: override (" + keyOverride->displayText + ")")
            : ("KeySrc: detected (" + project.getKeyDisplayText() + ")");
        return buildTempoMetaText() + "  | " + keyText;
    }

    juce::String buildBarText() const
    {
        return project.tracks.empty()
            ? juce::String("Bar: n/a")
            : ("Bar: " + juce::String(juce::jmax(1, displayedBar)) + "/" + juce::String(juce::jmax(1, project.maxBar)));
    }

    void setStatusMessage(const juce::String& message)
    {
        statusMessageBase = message;
        refreshStatusMessage();
    }

    void refreshStatusMessage()
    {
        const auto prefix = statusMessageBase.isNotEmpty() ? statusMessageBase : juce::String("Ready");
        statusLabel.setText(buildSignatureText() + "  | " + buildBarText() + "  | " + prefix + "  | " + buildStatusDetailsText(),
                            juce::dontSendNotification);
    }

    juce::Label titleLabel;
    juce::TextButton loadButton;
    juce::Label staff1TrackLabel;
    juce::ComboBox staff1TrackSelector;
    StaffDisplayOctaveSelector staff1OctaveSelector;
    juce::ComboBox staff1ClefSelector;
    juce::Label staff2TrackLabel;
    juce::ComboBox staff2TrackSelector;
    StaffDisplayOctaveSelector staff2OctaveSelector;
    juce::ComboBox staff2ClefSelector;
    juce::Label staff3TrackLabel;
    juce::ComboBox staff3TrackSelector;
    StaffDisplayOctaveSelector staff3OctaveSelector;
    juce::ComboBox staff3ClefSelector;
    juce::TextButton transportToggleButton;
    HideControlArrowButton hideScoreControlsButton;
    juce::Label accidentalLabel;
    juce::ComboBox accidentalSelector;
    juce::TextButton accidentalHelpButton;
    juce::Label aliasLabel;
    juce::ComboBox aliasSelector;
    juce::Label autoChordLabel;
    juce::Label chordComplexityLabel;
    juce::ComboBox chordComplexitySelector;
    juce::Label chordResolutionLabel;
    juce::ComboBox chordResolutionSelector;
    juce::Label chordTracksLabel;
    juce::OwnedArray<juce::ToggleButton> chordTrackButtons;
    juce::Array<int> chordTrackSourceIndices;
    juce::Label globalTransposeLabel;
    juce::TextEditor globalTransposeInput;
    juce::TextButton transposeHelpButton;
    juce::TextButton continueButton;
    juce::Label continueBarLabel;
    juce::TextEditor continueBarInput;
    juce::ToggleButton loopEnabledToggle;
    juce::Label loopStartLabel;
    juce::TextEditor loopStartInput;
    juce::Label loopEndLabel;
    juce::TextEditor loopEndInput;
    juce::TextButton savePresetButton;
    juce::TextButton loadPresetButton;
    juce::ComboBox exportPdfModeSelector;
    juce::TextButton exportPdfButton;
    juce::TextButton openRecentButton;
    juce::Label statusLabel;
    juce::Label tempoOverrideLabel;
    juce::TextEditor tempoOverrideInput;
    juce::TextButton tempoHelpButton;
    juce::Label assignLabel;
    juce::ToggleButton assignToggle;
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
    IPlaybackPositionSource* playbackPositionSource = &playbackController;
    MidiFilePlaybackEngineAdapter midiPlaybackEngine;
    MidiOutputDevice midiOutputDevice;
    TrackMixState trackMixState;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::FileChooser> exportPdfFileChooser;
    juce::File lastMidiDirectory;
    juce::File workingDirectory;
    juce::String lastLoadedMidiPath;
    std::vector<juce::String> recentMidiFiles;
    bool startupResumeEnabled = false;
    bool scoreLightMode = true;
    bool scoreControlRowsHidden = false;
    juce::String midiOutputRestoreWarning;
    bool continueArmed = false;
    bool loopEnabled = false;
    int loopStartBar = 1;
    int loopEndBar = 1;
    int displayedBar = 1;
    juce::String statusMessageBase;
    juce::String savedScoreSongKey;
    std::optional<ScoreSongSettingsSnapshot> savedScoreSongSettingsSnapshot;
    std::vector<TrackMixSettings> savedTrackMixSnapshot;
    TrackMixSaveStatus trackMixSaveStatus = TrackMixSaveStatus::saved;
    std::optional<double> tempoOverrideBpm;
    std::optional<ParsedKey> keyOverride;
    bool keyOverrideProfileOnly = false;
    bool suppressProfileOnlyExitUntilKeyChange = false;
    int keyTransposeReferenceTonicPc = 0;
    int keyTransposeAppliedSemitones = 0;
    std::optional<int> pendingKeyTransposeReferenceTonicPc;
    std::array<std::vector<MidiNoteEvent>, 3> liveChordNotesByStaff;
    std::array<LiveChordState, 3> liveChordStates;
    static constexpr int trackMixPresetAutosaveDelayMs = 400;
    int trackMixPresetSaveGeneration = 0;
};
