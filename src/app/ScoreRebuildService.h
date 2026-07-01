#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <optional>
#include <vector>
#include "../harmony/ChordDetector.h"
#include "../midi/MidiProjectLoader.h"
#include "../notation/Quantizer.h"
#include "../notation/ScoreModel.h"
#include "../notation/ScoreRenderer.h"
#include "../playback/PlaybackController.h"

class ScoreRebuildService
{
public:
    struct StaffLane
    {
        juce::ComboBox* trackSelector = nullptr;
        juce::ComboBox* clefSelector = nullptr;
        ScoreModel* model = nullptr;
        ScoreRenderer* renderer = nullptr;
        size_t staffIndex = 0;
    };

    struct Context
    {
        const MidiProjectData* project = nullptr;
        PlaybackController* playbackController = nullptr;
        std::array<std::vector<MidiNoteEvent>, 3>* liveChordNotesByStaff = nullptr;
        std::function<void(int staffIndex, ScoreModel&, ScoreRenderer&)> clearStaff;
        std::function<int(int trackIndex)> effectiveTransposeForTrack;
        std::function<std::vector<MidiNoteEvent>(int trackIndex)> collectChordAnalysisNotes;
        std::function<std::vector<MidiNoteEvent>()> collectSharedChordAnalysisNotes;
        std::function<bool()> hasChordTrackSelection;
        std::function<ChordDetector::NamingOptions()> namingOptions;
        std::function<ChordDetector::DetectionResolution()> chordResolution;
        std::function<void()> resetLiveChordState;
        std::function<void(const juce::String&)> setStatusMessage;
        juce::Label* transportLabel = nullptr;
        juce::Label* keyLabel = nullptr;
        juce::Label* midiMetaLabel = nullptr;
        juce::OwnedArray<juce::ToggleButton>* chordTrackButtons = nullptr;
        int* displayedBar = nullptr;
        bool hasKeyOverride = false;
        int keySharpsOrFlats = 0;
        juce::String keyDisplayText;
        juce::String midiMetaText;
        std::function<void()> refreshSavePresetButtonDirtyStyle;
        std::function<bool()> hasLoadedProject;
    };

    static ScoreRenderer::ClefType clefTypeFromCombo(const juce::ComboBox& combo)
    {
        if (combo.getSelectedId() == 2)
            return ScoreRenderer::ClefType::bass;
        if (combo.getSelectedId() == 3)
            return ScoreRenderer::ClefType::drum;
        return ScoreRenderer::ClefType::treble;
    }

    // Staff track selectors use index 0 for "No Display", then tracks at 1..N.
    static int sourceTrackIndexFromSelector(const juce::ComboBox& combo)
    {
        return combo.getSelectedItemIndex() - 1;
    }

    static void rebuildStaff(const StaffLane& lane,
                             const Context& ctx,
                             const std::optional<std::vector<ChordAnnotation>>& sharedChordAnnotations = std::nullopt)
    {
        if (ctx.project == nullptr || lane.trackSelector == nullptr || lane.clefSelector == nullptr
            || lane.model == nullptr || lane.renderer == nullptr || ctx.liveChordNotesByStaff == nullptr)
            return;

        const int index = sourceTrackIndexFromSelector(*lane.trackSelector);
        if (index < 0 || index >= static_cast<int>(ctx.project->tracks.size()))
        {
            ctx.clearStaff(static_cast<int>(lane.staffIndex), *lane.model, *lane.renderer);
            (*ctx.liveChordNotesByStaff)[lane.staffIndex].clear();
            return;
        }

        const auto& track = ctx.project->tracks[(size_t) index];
        auto transposedNotes = track.notes;
        const auto clefType = clefTypeFromCombo(*lane.clefSelector);
        const int transposeSemitones = ctx.effectiveTransposeForTrack ? ctx.effectiveTransposeForTrack(index) : 0;
        if (transposeSemitones != 0 && clefType != ScoreRenderer::ClefType::drum)
        {
            for (auto& note : transposedNotes)
                note.noteNumber = juce::jlimit(0, 127, note.noteNumber + transposeSemitones);
        }

        auto quantized = Quantizer::quantizeTrack(transposedNotes, ctx.project->tempoMap);
        const auto chordAnalysisNotes = ctx.collectChordAnalysisNotes
            ? ctx.collectChordAnalysisNotes(index)
            : std::vector<MidiNoteEvent>();
        (*ctx.liveChordNotesByStaff)[lane.staffIndex] = chordAnalysisNotes;
        const auto namingOptions = ctx.namingOptions ? ctx.namingOptions() : ChordDetector::NamingOptions();
        const auto chordResolution = ctx.chordResolution ? ctx.chordResolution() : ChordDetector::DetectionResolution::quarter;

        std::vector<ChordAnnotation> chords;
        if (sharedChordAnnotations.has_value())
            chords = sharedChordAnnotations.value();
        else
            chords = ChordDetector::detect(chordAnalysisNotes, ctx.project->tempoMap, ctx.project->maxBar, namingOptions, chordResolution);
        lane.model->build(ctx.project->tempoMap, quantized, chords, ctx.project->maxBar);

        lane.renderer->setStaffLabel(track.name);
        lane.renderer->setClefType(clefType);
        if (ctx.playbackController != nullptr)
            lane.renderer->setCurrentBar(ctx.playbackController->getCurrentBar());
    }

    static void rebuildAllStaffs(const Context& ctx, const std::array<StaffLane, 3>& lanes)
    {
        if (ctx.project == nullptr || ctx.project->tracks.empty())
        {
            for (const auto& lane : lanes)
            {
                if (lane.model != nullptr && lane.renderer != nullptr)
                    ctx.clearStaff(static_cast<int>(lane.staffIndex), *lane.model, *lane.renderer);
            }
            if (ctx.resetLiveChordState)
                ctx.resetLiveChordState();
            if (ctx.refreshSavePresetButtonDirtyStyle)
                ctx.refreshSavePresetButtonDirtyStyle();
            if (ctx.setStatusMessage)
                ctx.setStatusMessage("Load a MIDI file to begin.");
            return;
        }

        const bool hasKeySignature = ctx.hasKeyOverride ? true : ctx.project->hasKeySignature;
        const int keySharpsOrFlats = ctx.hasKeyOverride ? ctx.keySharpsOrFlats : ctx.project->keySharpsOrFlats;
        for (const auto& lane : lanes)
        {
            if (lane.renderer != nullptr)
                lane.renderer->setKeySignature(hasKeySignature, keySharpsOrFlats);
        }

        std::optional<std::vector<ChordAnnotation>> sharedChordAnnotations;
        const bool useSharedChords = ctx.hasChordTrackSelection && ctx.hasChordTrackSelection()
            && ctx.collectSharedChordAnalysisNotes;
        if (useSharedChords)
        {
            const auto sharedNotes = ctx.collectSharedChordAnalysisNotes();
            const auto namingOptions = ctx.namingOptions ? ctx.namingOptions() : ChordDetector::NamingOptions();
            const auto chordResolution = ctx.chordResolution ? ctx.chordResolution() : ChordDetector::DetectionResolution::quarter;
            sharedChordAnnotations = ChordDetector::detect(sharedNotes,
                                                           ctx.project->tempoMap,
                                                           ctx.project->maxBar,
                                                           namingOptions,
                                                           chordResolution);
        }

        for (const auto& lane : lanes)
            rebuildStaff(lane, ctx, sharedChordAnnotations);

        if (ctx.resetLiveChordState)
            ctx.resetLiveChordState();

        const int bar = ctx.hasLoadedProject && ctx.hasLoadedProject()
            ? juce::jlimit(1, juce::jmax(1, ctx.project->maxBar), ctx.playbackController->getCurrentBar())
            : 1;
        auto selectedBar = bar;

        auto countNonRestInWindow = [](const ScoreModel& model, int centerBar) -> int
        {
            int count = 0;
            const auto bars = model.getWindowBars(centerBar, 2, 2);
            for (const auto& windowBar : bars)
                for (const auto& symbol : windowBar.notes)
                    if (!symbol.isRest)
                        ++count;
            return count;
        };

        auto firstNonRestBar = [](const ScoreModel& model) -> int
        {
            if (model.empty())
                return 0;

            const auto bars = model.getBarsInRange(model.getFirstBar(), model.getLastBar());
            for (const auto& modelBar : bars)
            {
                for (const auto& symbol : modelBar.notes)
                {
                    if (!symbol.isRest)
                        return modelBar.barNumber;
                }
            }
            return 0;
        };

        if (ctx.playbackController != nullptr
            && !ctx.playbackController->isPlaying()
            && selectedBar == 1)
        {
            int staff1WindowNonRest = 0;
            int staff1FirstNonRestBar = 0;
            if (lanes[0].model != nullptr)
            {
                staff1WindowNonRest = countNonRestInWindow(*lanes[0].model, selectedBar);
                staff1FirstNonRestBar = firstNonRestBar(*lanes[0].model);
            }

            if (staff1WindowNonRest == 0 && staff1FirstNonRestBar > 0)
            {
                selectedBar = staff1FirstNonRestBar;
            }
            else
            {
                int visibleWindowNonRest = 0;
                for (const auto& lane : lanes)
                {
                    if (lane.model == nullptr || lane.model->empty())
                        continue;
                    visibleWindowNonRest += countNonRestInWindow(*lane.model, selectedBar);
                }

                if (visibleWindowNonRest == 0)
                {
                    int earliestNonRestBar = std::numeric_limits<int>::max();
                    for (const auto& lane : lanes)
                    {
                        if (lane.model == nullptr || lane.model->empty())
                            continue;

                        const int laneFirstNonRest = firstNonRestBar(*lane.model);
                        if (laneFirstNonRest > 0)
                            earliestNonRestBar = juce::jmin(earliestNonRestBar, laneFirstNonRest);
                    }

                    if (earliestNonRestBar != std::numeric_limits<int>::max())
                        selectedBar = earliestNonRestBar;
                }
            }
        }

        for (const auto& lane : lanes)
        {
            if (lane.renderer != nullptr)
                lane.renderer->setCurrentBar(selectedBar);
        }
        if (ctx.transportLabel != nullptr)
            ctx.transportLabel->setText("Bar " + juce::String(selectedBar), juce::dontSendNotification);
        if (ctx.displayedBar != nullptr)
            *ctx.displayedBar = selectedBar;

        int selectedTrackCount = 0;
        if (ctx.chordTrackButtons != nullptr)
        {
            for (auto* button : *ctx.chordTrackButtons)
                selectedTrackCount += button->getToggleState() ? 1 : 0;
        }
        if (ctx.keyLabel != nullptr)
            ctx.keyLabel->setText("Key: " + ctx.keyDisplayText, juce::dontSendNotification);
        if (ctx.midiMetaLabel != nullptr)
            ctx.midiMetaLabel->setText(ctx.midiMetaText, juce::dontSendNotification);
        if (ctx.setStatusMessage)
            ctx.setStatusMessage("Staffs ready  | Chords from " + juce::String(selectedTrackCount) + " selected tracks");
    }
};
