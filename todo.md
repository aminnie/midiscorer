# MidiScorer Implementation Todo

This checklist tracks code-review findings and implementation work items.

## Priority 0 - Critical correctness

- [x] Reject unsupported SMPTE MIDI timing with a clear load error.
  - Files: `src/midi/MidiProjectLoader.h`
  - Acceptance:
    - Loader returns `false` for `getTimeFormat() <= 0`.
    - Error message states SMPTE/non-PPQ timing is unsupported.
- [x] Flush orphaned note-ons at end-of-track.
  - Files: `src/midi/TrackNoteExtractor.h`
  - Acceptance:
    - Remaining active notes are emitted with `endTick` at final track tick.
    - Output is time-sorted and non-negative in duration.

## Priority 1 - Chord/model correctness

- [x] Split notes across bar boundaries and preserve tie continuity.
  - Files: `src/notation/ScoreModel.h`
  - Acceptance:
    - Spanning notes create continuation note symbols in following bars.
    - Continuations do not get replaced by rests.
    - Tie flags are set on segments that continue into next bar.
- [x] Reset chord dedup behavior across silence/bar boundaries.
  - Files: `src/harmony/ChordDetector.h`
  - Acceptance:
    - Same chord after silence is re-announced.
    - Downbeat re-annotation can occur per bar policy.
- [x] Clear live chord markers on empty windows and allow re-attack.
  - Files: `src/app/MainComponent.h`
  - Acceptance:
    - Marker hides when no chord is detected.
    - Same symbol can reappear after silence.

## Priority 2 - Rendering reliability/fidelity

- [x] Render multiple static chord changes per bar.
  - Files: `src/notation/ScoreRenderer.h`
  - Acceptance:
    - Bar with multiple chord annotations displays all (or deterministic subset by beat position).
- [x] Prevent blank rendering when current bar is outside available model window.
  - Files: `src/notation/ScoreRenderer.h`
  - Acceptance:
    - Renderer clamps/falls back to valid bar range and always paints bars when model is non-empty.

## Priority 3 - Regression tests

- [x] Expand tests for loader/extractor/model/chord/live-marker behavior.
  - Files: `tests/test_main.cpp`
  - Acceptance:
    - Added tests for SMPTE rejection, orphan flush, cross-bar split/ties, chord dedup reset, and window clamp behavior.
    - Existing tests continue to pass.
- [x] Align fixture intent to tests.
  - Files: `tests/fixtures/fixture-specs.md` (mapping via test names/comments)
  - Acceptance:
    - New tests reference fixture-like scenarios (tempo changes, syncopation, ties, altered chords).

## Priority 4 - Player/tab integration

- [x] Add loosely-coupled MIDI playback adapter and transport position interface.
  - Files: `src/playback/IPlaybackPositionSource.h`, `src/playback/MidiFilePlaybackEngineAdapter.h`, `src/playback/PlaybackController.h`
  - Acceptance:
    - Playback position remains the authority for score rendering and live chord windows.
    - Scheduled MIDI events are emitted by adapter against caller-supplied playback time.
- [x] Add single-device MIDI output selection and Player tab transport UI.
  - Files: `src/playback/MidiOutputDevice.h`, `src/app/PlayerTabComponent.h`, `src/app/AppTabsHost.h`, `Main.cpp`
  - Acceptance:
    - User can select one detected MIDI output device.
    - Player tab exposes Play/Pause/Stop and bar seek/play-from-bar controls.
    - Tab host includes separate `Score` and `Player` pages.
- [x] Keep playback output path GM-only and avoid organ-style routing complexity.
  - Files: `src/app/MainComponent.h`
  - Acceptance:
    - Scheduled playback messages route only to the selected single output.
    - No per-track/per-module routing dependencies introduced.

## Priority 5 - Per-track mix controls

- [x] Add per-track mix domain + processor and source-track playback tagging.
  - Files: `src/playback/TrackMixState.h`, `src/playback/TrackMixProcessor.h`, `src/playback/MidiFilePlaybackEngineAdapter.h`
  - Acceptance:
    - Scheduled events carry `sourceTrackIndex`.
    - Playback mix gate supports mute/solo precedence.
    - Track volume scales note-on and CC7/CC11 messages.
- [x] Add Effects tab UI for grouped Volume/Mute/Solo controls.
  - Files: `src/app/TracksTabComponent.h`, `src/app/AppTabsHost.h`, `src/app/MainComponent.h`
  - Acceptance:
    - New `Effects` tab appears beside `Score` and `Player`.
    - Eligible tracks show bordered groups named by track with volume slider and mute/solo checkboxes.
- [x] Persist per-song track mix values in preset profile.
  - Files: `src/app/MainComponent.h`
  - Acceptance:
    - `ui_preset.json` stores/loads `trackMixBySong` keyed by song path.
    - Track mix changes auto-save while editing and reapply on reload.

## Priority 6 - Effects mix enhancements

- [x] Add per-track Reverb control with CC91 playback merge.
  - Files: `src/playback/TrackMixState.h`, `src/playback/TrackMixProcessor.h`, `src/app/TracksTabComponent.h`, `src/app/MainComponent.h`
  - Acceptance:
    - Effects tab shows Reverb slider per eligible track.
    - CC91 events merge with per-track reverb during playback.
- [x] Seed Volume/Reverb sliders from MIDI track CC values.
  - Files: `src/playback/TrackMixMidiSeed.h`, `src/app/MainComponent.h`, `tests/test_main.cpp`
  - Acceptance:
    - Last CC7/CC91 per track seeds slider values on load.
    - Missing CC defaults to volume 100 / reverb 10.
    - Saved `trackMixBySong` entries override seeded values.
- [x] Score tab status and preset UX refinements.
  - Files: `src/app/MainComponent.h`, `src/app/AppTabsHost.h`
  - Acceptance:
    - Tab order is Start → Score → Effects.
    - Status line shows Sig before Bar; KeySrc indicates detected vs override key source.
    - Save Preset highlights red when score song settings are dirty.
    - Playback status messages: running/stopped/continuing without tempo-override suffix noise.
- [x] Add per-track **Chan** control (1..16) with playback channel remap.
  - Files: `src/playback/TrackMixState.h`, `src/playback/TrackMixMidiSeed.h`, `src/playback/TrackMixProcessor.h`, `src/app/TracksTabComponent.h`, `src/app/MainComponent.h`, `tests/test_main.cpp`
  - Acceptance:
    - Effects tab shows **Chan** before Mute on each track row.
    - Chan seeds from the track's first MIDI channel on load (default 1); saved `trackMixBySong` overrides.
    - Chan edits auto-save to the profile and remap outgoing playback events.
    - Use Chan changes if you need to reorganize channels in order to play along with the score and MIDI file while playing an instrument that shares the selected MIDI module.
- [x] Reject Standard MIDI File type **0** on load with a conversion modal.
  - Files: `src/midi/MidiProjectLoader.h`, `src/app/MainComponent.h`, `tests/test_main.cpp`
  - Acceptance:
    - Loader detects SMF type from the file header and rejects type **0**.
    - UI shows a warning modal asking the user to save/export as type **1** in a MIDI editor.
    - SMPTE rejection tests remain valid with a type **1** fixture header.

## Priority 7 - Reliability and correctness (highest impact)

- [x] **P0 / S** Remove production debug instrumentation from Effects tab (`appendDebugLog`, hardcoded `debug-e2bc95.log`).
  - Files: `src/app/TracksTabComponent.h`
  - Acceptance:
    - No debug file writes occur during Effects-tab interaction.
    - Solo/mute updates still refresh rows safely and deterministically.
- [x] **P0 / M** Reconcile SMF type **0** behavior across loader/UI/tests/docs.
  - Files: `src/midi/MidiProjectLoader.h`, `src/app/MainComponent.h`, `tests/test_main.cpp`, `README.md`, `CONTRIBUTING.md`, `TECHNICAL_DESIGN.md`
  - Acceptance:
    - App load path and docs consistently describe type **0** rejection with conversion guidance.
    - Loader tests continue to verify type checks and user-facing error guidance.
- [x] **P0 / M** Make preset writes atomic and failure-safe (`ui_preset.json` temp-write + replace, user-visible failure path).
  - Files: `src/app/MainComponent.h`
  - Acceptance:
    - Preset writes use atomic replacement instead of direct overwrite.
    - Failed writes surface an actionable user-facing status message.
- [x] **P0 / S** Surface MIDI output restore failures in UI instead of logger-only handling.
  - Files: `src/app/MainComponent.h`, `src/app/PlayerTabComponent.h`
  - Acceptance:
    - Failed output restore shows a warning in Start tab status text.
    - Manual output selection clears stale restore warning state.

## Priority 8 - Workflow/policy alignment shipped

- [x] Enforce non-destructive workflow (no source MIDI rewrite/export path in normal app flow).
  - Files: `src/app/MainComponent.h`, `src/midi/MidiMixExporter.h`, `tests/test_main.cpp`, docs
  - Acceptance:
    - Export-MIDI action removed from Score tab and exporter path removed from active app/test flow.
    - Save/Load Preset remains the canonical way to keep/revert user edits.
- [x] Simplify transport UX to `Start/Stop` + `Continue` and remove dedicated Pause button.
  - Files: `src/app/MainComponent.h`, `README.md`, `AGENT.md`
  - Acceptance:
    - Main transport button toggles Start/Stop semantics only.
    - Dedicated Pause button is not shown in UI.
- [x] Fix `Open Recent` usability for no-op clicks and duplicate filename ambiguity.
  - Files: `src/app/MainComponent.h`, `AGENT.md`
  - Acceptance:
    - `Open Recent` falls back to most-recent when placeholder is selected.
    - Recent entries include folder context to disambiguate duplicate names.
- [x] Match Loop A/B input formatting with Continue Bar input.
  - Files: `src/app/MainComponent.h`
  - Acceptance:
    - Loop A and B text inputs use centered numeric text presentation.

## Priority 9 - Maintainability and performance

- [x] **P1 / L** Decompose `MainComponent` into smaller services (`PresetStore`, `ScoreRebuildService`, transport coordinator).
  - Files: `src/app/MainComponent.h` and extracted helpers under `src/app/`
  - Acceptance:
    - Orchestration remains behavior-compatible while reducing single-header coupling.
- [x] **P1 / M** Remove duplicate heavy chord detection during rebuild by computing static chord annotations once per rebuild and reusing across staff models.
  - Files: `src/app/MainComponent.h`, `src/harmony/ChordDetector.h`
  - Acceptance:
    - Full staff rebuild avoids repeated whole-song chord scans.
- [x] **P1 / S** Handle note-on velocity 0 as note-off in extractor with regression test.
  - Files: `src/midi/TrackNoteExtractor.h`, `tests/test_main.cpp`
  - Acceptance:
    - Velocity-0 note-on closes active notes as expected.
- [x] **P1 / S** Debounce preset autosave on mix slider edits to avoid UI-thread rewrite thrash.
  - Files: `src/app/MainComponent.h`
  - Acceptance:
    - Rapid slider changes batch into fewer preset writes.
- [x] **P1 / M** Define and implement tempo-override behavior for multi-tempo songs.
  - Files: `src/playback/PlaybackController.h`, `src/app/MainComponent.h`, tests as needed
  - Acceptance:
    - Behavior is deterministic and documented for files with tempo changes.

## Priority 10 - Test/CI hardening and product UX enhancements

- [x] **P2 / S** Pin JUCE revision in CI for stability.
  - Files: `.github/workflows/ci.yml`
  - Acceptance:
    - CI uses a fixed JUCE tag/commit instead of floating default branch.
- [x] **P2 / M** Convert fixture load-smoke checks into behavioral assertions for tempo/time-signature and ties fixtures.
  - Files: `tests/test_main.cpp`, `tests/fixtures/fixture-specs.md`
  - Acceptance:
    - Fixture tests validate expected musical/timing behavior, not only parse success.
- [x] **P2 / S** Add explicit CTest working directory and deterministic fixture path strategy.
  - Files: `CMakeLists.txt`, `tests/test_main.cpp`
  - Acceptance:
    - Fixture tests run reliably regardless of invocation directory.
- [x] **P2 / S** Fix dirty-state coverage gaps for clef and score color settings.
  - Files: `src/app/MainComponent.h`
  - Acceptance:
    - Clef and score color edits correctly trigger Save Preset dirty style.
- [x] **P3 / M** Add startup resume workflow (optional reopen last MIDI/recent path).
  - Files: `src/app/MainComponent.h`, `Main.cpp`
  - Acceptance:
    - User can opt in to reopening recent/last MIDI on startup.
- [x] **P3 / M** Add missing checked-in fixtures for syncopation and altered-chord scenarios plus assertions.
  - Files: `tests/fixtures/`, `tests/fixtures/fixture-specs.md`, `tests/test_main.cpp`
  - Acceptance:
    - Fixture specs for syncopation/altered chords are represented by checked-in files and tests.
- [ ] **P4 / L** Add score export options (PNG/PDF only) and/or rendering regression tests.
  - Files: `src/notation/ScoreRenderer.h`, `src/app/MainComponent.h`, tests/docs as needed
  - Acceptance:
    - A documented non-MIDI export path (PNG/PDF) or regression-testing path improves score-output reliability without rewriting source MIDI files.
