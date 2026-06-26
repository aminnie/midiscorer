# MidiScorer Agent Guide

This file provides project-specific guidance for future coding agents working in this repository.

## Project intent

MidiScorer is a JUCE/C++ desktop app that:

- loads MIDI files
- renders up to three staff lanes as score-like notation
- detects/static-renders chords and tracks live chord changes
- syncs score display to playback state

Primary user goals are notation readability, deterministic chord behavior, and fast iteration on UI/workflow.

## Tech stack and platform intent

- Framework/runtime: JUCE desktop application.
- Language/toolchain baseline: C++17 project settings and existing JUCE/CMake patterns in repo.
- Target platforms: Windows and macOS parity for user-facing behavior unless explicitly scoped otherwise.
- Build system: CMake + JUCE helpers; prefer script/documented workflows in `build.md`.

## Core architecture

Use these modules as the source of truth for each concern:

- `src/midi/TempoMap.h` - timing/bar math conversions
- `src/midi/MidiProjectLoader.h` - MIDI ingest, SMF type/SMPTE validation, metadata extraction
- `src/midi/TrackNoteExtractor.h` - note-on/off pairing
- `src/notation/Quantizer.h` - note duration/start quantization
- `src/notation/ScoreModel.h` - score-domain symbols (notes, rests, ties, chords)
- `src/notation/ScoreRenderer.h` - rendering only (visual layer)
- `src/harmony/ChordDetector.h` - chord detection + naming policy
- `src/playback/PlaybackController.h` - playback time/bar state
- `src/playback/TrackMixState.h` - per-track mix state (channel, volume, reverb, mute, solo)
- `src/playback/TrackMixProcessor.h` - playback mix gating, channel remap, scaling
- `src/playback/TrackMixMidiSeed.h` - Chan/CC7/CC91 seeding on load
- `src/app/MainComponent.h` - orchestration/UI wiring (Score tab)
- `src/app/TracksTabComponent.h` - Effects tab mix UI
- `src/app/AppTabsHost.h` - tab host (`Start`, `Score`, `Effects`)

## Guardrails

- Keep `ScoreRenderer` visual-only. Do not move timing/chord decisions into it.
- Keep chord logic in `ChordDetector`.
- Keep bar/time conversions in `TempoMap`.
- Keep UI state and rebuild triggers in `MainComponent`.
- Preserve existing drum exceptions (drum clef / channel 10 should not be transposed).
- Keep SMF type **0** rejection and the type **1** conversion modal message in `MidiProjectLoader` / `MainComponent` unless requirements change explicitly.
- Keep workflow non-destructive: do not add features that rewrite source MIDI files in normal app flow.
- Persist user edits via profile state (`ui_preset.json`) and prefer Save/Load Preset based workflows.
- Keep transport semantics on Score tab as `Start/Stop` + `Continue` (no dedicated Pause button unless explicitly requested).
- Do not add hardcoded debug log file writes or agent instrumentation blocks in production UI code.

## Preferred change style

- Make focused changes in one subsystem at a time.
- Preserve existing behavior unless explicitly requested.
- Prefer small helper methods over deeply nested logic.
- Add short comments only for non-obvious logic.
- Keep local build docs/scripts/CI expectations aligned when changing build or workflow behavior.

## Common workflows

Configure, build, test, and launch: see **[build.md](build.md)** (Windows and macOS).

Typical agent loop after code changes:

1. Windows: `./scripts/build.ps1 -Configuration Debug -Target All`
2. macOS: `./scripts/mac-build.sh`
3. Launch the built app for smoke checks (`build/MidiScorer_artefacts/Debug/MidiScorer.exe` or `build-mac/MidiScorer_artefacts/Debug/MidiScorer.app`)

## Before finishing a coding task

1. Build successfully.
2. Run tests (`ctest`) for all code changes (docs-only changes are the exception).
3. Smoke-check app behavior for affected UI/notation/chord paths.
4. Update docs (`README.md`, `TECHNICAL_DESIGN.md`, `todo.md`) when user-visible behavior or architecture changes.

## Testing policy

- Prefer deterministic, checked-in fixture coverage for behavior changes when practical.
- Avoid relying only on manual smoke checks when a regression can be unit/integration tested.
- Keep tests aligned with documented behavior (especially MIDI ingest rules, transport UX, and preset persistence).

## Backlog and governance

- Treat `todo.md` as the working backlog for planned/implemented work.
- When a todo item is implemented, update its checklist state and acceptance notes accordingly.
- Do not introduce features that conflict with Guardrails unless the user explicitly requests an exception.
- When a change affects workflow policy or UX guardrails (for example: non-destructive file handling, transport semantics, preset behavior), use a commit message that explicitly names that policy-impacting change.

## Regression-sensitive areas

- Key override and transpose interaction
- Live chord marker state transitions across silence
- Cross-bar note splitting and tie continuity
- Rest insertion gap fill behavior
- Chord track checkbox mapping to source track indices
- Renderer first-visible-bar clef/key-signature spacing
- Track mix MIDI seeding vs saved `trackMixBySong` preset precedence (Chan, volume, reverb)
- SMF type **0** load rejection and warning-modal messaging
- Save Preset dirty-state styling and score song-settings snapshot comparison
- Loop A/B bounds clamping and loop wrap behavior during playback
- Recent-file UX (`Open Recent`) selection fallback and duplicate filename disambiguation

## Existing technical references

- `build.md` - configure, build, test, and launch for Windows and macOS
- `README.md` - user-facing capabilities and developer notes
- `TECHNICAL_DESIGN.md` - deep technical design for scoring and chord detection
- `CONTRIBUTING.md` - contributor expectations and smoke-test checklist
- `tests/fixtures/fixture-specs.md` - fixture intent and test mapping

## Decision history (latest)

- **Non-destructive workflow policy**
  - Source MIDI files are treated as read-only inputs in normal app usage.
  - User edits are persisted/restored through profile state (`ui_preset.json`) via Save/Load Preset.
  - Rationale: preserve original files and keep reversible user workflows.

- **Transport UX simplification**
  - Score tab transport uses `Start/Stop` and `Continue`; dedicated Pause button removed.
  - Rationale: reduce ambiguity and keep behavior aligned with user expectations.

- **Recent-file usability**
  - `Open Recent` should open a selected recent file, and safely fall back to most-recent when placeholder is selected.
  - Recent list entries should disambiguate duplicate filenames by including parent-folder context.
  - Rationale: prevent no-op clicks and confusion with same-named files from different directories.

- **Production logging hygiene**
  - Temporary/hardcoded debug instrumentation (especially file writes from UI callbacks) should not ship.
  - Rationale: avoid platform-specific failures, noisy I/O, and maintenance risk.
