# Contributing to MidiScorer

Thanks for contributing. This project is a JUCE/C++ app focused on MIDI parsing, score-style rendering, chord analysis, and playback-synced visualization.

## Quick start

1. Configure:
   - `cmake -S . -B build -DJUCE_ROOT="C:/JUCE"`
2. Build:
   - `cmake --build build --config Debug --target MidiScorer MidiScorerTests`
3. Test:
   - `ctest --test-dir build -C Debug --output-on-failure`

## Expected change scope

Keep pull requests small and focused. Preferred examples:

- one chord-detection rule change + tests
- one quantization improvement + tests
- one rendering refinement + manual smoke notes

Avoid mixing unrelated subsystems in one PR (for example, timing model + UI drawing + chord templates all at once).

Recent feature areas to keep isolated in PRs:

- multi-staff selection/clef UI
- chord-track checkbox source selection
- live eighth-note chord marker behavior
- rest insertion and rendering
- first-visible-bar key-signature drawing

## Core architecture guardrails

- `TempoMap` is the timing source of truth.
- `PlaybackController` is the playback/bar source of truth.
- `ScoreRenderer` should remain visual-only (no timing or harmonic logic decisions).
- `ChordDetector` should own chord matching and naming policy.
- `MainComponent` should orchestrate UI state and trigger score rebuilds.

## Testing expectations

For changes in:

- `src/midi/TempoMap.h`: add/adjust deterministic timing tests.
- `src/notation/Quantizer.h`: add quantization behavior checks.
- `src/notation/ScoreModel.h`: add or adjust gap/rest insertion behavior checks.
- `src/harmony/ChordDetector.h`: add chord match and naming-option checks.
- `src/app/MainComponent.h`: include manual playback smoke coverage for timer/live marker behavior.
- `src/notation/ScoreRenderer.h`: include manual rendering checks for clef-specific key signature positioning.

At minimum before submitting:

1. Build app and tests successfully.
2. Run `ctest` with zero failures.
3. Smoke test in app:
   - load MIDI
   - verify auto-preset load behavior
   - switch staff tracks and clefs
   - toggle chord-track checkboxes
   - verify chord naming preference toggles
   - verify light/dark score toggle
   - verify key signature glyphs appear at the left edge of the first visible bar on each staff
   - verify rolling bar playback updates
   - verify live chord marker changes only when chord changes
   - verify Stop pre-fills Continue bar and Continue resumes correctly
   - verify Save/Load preset buttons remain usable from the status row

## PR checklist

- [ ] Change has clear scope and rationale.
- [ ] Relevant tests updated or added.
- [ ] `ctest` passes locally.
- [ ] No new warnings/errors introduced in edited files.
- [ ] README/Developer Notes updated if behavior changed.

## Style notes

- Use C++17 compatible code.
- Prefer small, composable helper functions over deeply nested blocks.
- Keep new comments short and only for non-obvious logic.
