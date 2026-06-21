# MidiScorer Agent Guide

This file provides project-specific guidance for future coding agents working in this repository.

## Project intent

MidiScorer is a JUCE/C++ desktop app that:

- loads MIDI files
- renders up to three staff lanes as score-like notation
- detects/static-renders chords and tracks live chord changes
- syncs score display to playback state

Primary user goals are notation readability, deterministic chord behavior, and fast iteration on UI/workflow.

## Core architecture

Use these modules as the source of truth for each concern:

- `src/midi/TempoMap.h` - timing/bar math conversions
- `src/midi/MidiProjectLoader.h` - MIDI ingest + metadata extraction
- `src/midi/TrackNoteExtractor.h` - note-on/off pairing
- `src/notation/Quantizer.h` - note duration/start quantization
- `src/notation/ScoreModel.h` - score-domain symbols (notes, rests, ties, chords)
- `src/notation/ScoreRenderer.h` - rendering only (visual layer)
- `src/harmony/ChordDetector.h` - chord detection + naming policy
- `src/playback/PlaybackController.h` - playback time/bar state
- `src/playback/TrackMixState.h` - per-track mix state
- `src/playback/TrackMixProcessor.h` - playback mix gating/scaling
- `src/playback/TrackMixMidiSeed.h` - CC7/CC91 seeding on load
- `src/app/MainComponent.h` - orchestration/UI wiring (Score tab)
- `src/app/TracksTabComponent.h` - Effects tab mix UI
- `src/app/AppTabsHost.h` - tab host (`Start`, `Score`, `Effects`)

## Guardrails

- Keep `ScoreRenderer` visual-only. Do not move timing/chord decisions into it.
- Keep chord logic in `ChordDetector`.
- Keep bar/time conversions in `TempoMap`.
- Keep UI state and rebuild triggers in `MainComponent`.
- Preserve existing drum exceptions (drum clef / channel 10 should not be transposed).

## Preferred change style

- Make focused changes in one subsystem at a time.
- Preserve existing behavior unless explicitly requested.
- Prefer small helper methods over deeply nested logic.
- Add short comments only for non-obvious logic.

## Common workflows

### Build

```powershell
cmake -S . -B build -DJUCE_ROOT="C:/JUCE"
cmake --build build --config Debug --target MidiScorer MidiScorerTests
```

### Test

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

### Launch app

- `build/MidiScorer_artefacts/Debug/MidiScorer.exe`

## Before finishing a coding task

1. Build successfully.
2. Run tests (`ctest`) when behavior changes touch core logic.
3. Smoke-check app behavior for affected UI/notation/chord paths.
4. Update docs (`README.md`, `TECHNICAL_DESIGN.md`, `todo.md`) when user-visible behavior or architecture changes.

## Regression-sensitive areas

- Key override and transpose interaction
- Live chord marker state transitions across silence
- Cross-bar note splitting and tie continuity
- Rest insertion gap fill behavior
- Chord track checkbox mapping to source track indices
- Renderer first-visible-bar clef/key-signature spacing
- Track mix MIDI seeding vs saved `trackMixBySong` preset precedence
- Save Preset dirty-state styling and score song-settings snapshot comparison

## Existing technical references

- `README.md` - user-facing capabilities and developer notes
- `TECHNICAL_DESIGN.md` - deep technical design for scoring and chord detection
- `CONTRIBUTING.md` - contributor expectations and smoke-test checklist
- `tests/fixtures/fixture-specs.md` - fixture intent and test mapping
