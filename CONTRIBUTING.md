# Contributing to MidiScorer

Thanks for contributing. This project is a JUCE/C++ app focused on MIDI parsing, score-style rendering, chord analysis, and playback-synced visualization.

## Quick start

Configure, build, and test: see **[build.md](build.md)**.

After a successful build, run `ctest` for your active build directory and complete the smoke-test checklist below before submitting.
- Windows example: `ctest --test-dir build -C Debug --output-on-failure`
- macOS example: `ctest --test-dir build-mac --output-on-failure`
- Helper scripts:
  - Windows: `./scripts/build.ps1 -Configuration Debug -Target All`
  - macOS: `./scripts/mac-build.sh`

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
- rest insertion and rendering (including dotted rests)
- open vs filled notehead rendering by duration
- first-visible-bar key-signature drawing
- drum clef notation and percussion notehead behavior
- per-staff header labels from selected track names

## Core architecture guardrails

- `TempoMap` is the timing source of truth.
- `PlaybackController` is the playback/bar source of truth.
- `ScoreRenderer` should remain visual-only (no timing or harmonic logic decisions).
- `ChordDetector` should own chord matching and naming policy.
- `MainComponent` should orchestrate UI state and trigger score rebuilds.
- Keep MIDI source files non-destructive in normal app workflow; persist user edits in profile data (`ui_preset.json`).

## Testing expectations

For changes in:

- `src/midi/MidiProjectLoader.h`: add/adjust type **0**, SMPTE, and ingest validation tests.
- `src/midi/TempoMap.h`: add/adjust deterministic timing tests.
- `src/notation/Quantizer.h`: add quantization behavior checks.
- `src/notation/ScoreModel.h`: add or adjust gap/rest insertion behavior checks.
- `src/harmony/ChordDetector.h`: add chord match and naming-option checks.
- `src/app/MainComponent.h`: include manual playback smoke coverage for timer/live marker behavior.
  - include both `Quarter` and `Eighth` chord-grid checks for static labels/live marker windows.
  - include `No Display` lane-hiding checks for Staff 1/2/3.
- `src/notation/ScoreRenderer.h`: include manual rendering checks for clef-specific key signature positioning.
- `src/notation/ScoreRenderer.h`: include manual checks for drum note placement/noteheads, open vs filled noteheads, augmentation dots, and per-staff header text.

At minimum before submitting:

1. Build app and tests successfully.
2. Run `ctest` with zero failures.
3. Smoke test in app:
   - load a type **1** MIDI file
   - verify a type **0** MIDI file shows the conversion warning modal and does not load
   - verify edits are restored via **Load Preset** (without rewriting source MIDI)
   - verify auto-preset load behavior
   - switch staff tracks and clefs
   - toggle chord-track checkboxes
   - verify chord naming preference toggles
   - verify light/dark score toggle
   - verify key signature glyphs appear at the left edge of the first visible bar on each staff
   - verify enlarged clef/key-signature symbols do not overlap notes in first visible bar
   - verify checked `Assign` keeps key text profile-only (no transpose), unchecked `Assign` applies key override transpose
   - verify key override transpose accumulates correctly across key changes (e.g. C -> F -> C returns to original pitch)
   - verify top-right staff header text follows selected track/instrument names
   - verify Drum clef mode renders percussion track notes with expected vertical placement
   - verify `No Display` hides selected staff lanes and remaining lanes expand to fill space
   - verify rolling bar playback updates
   - verify live chord marker changes only when chord text or marker position changes
   - verify `Chord grid` quarter/eighth selection updates both static labels and live marker behavior
   - verify `Chord grid` selection is restored after Save Preset + Load Preset
   - verify Stop pre-fills Continue bar and Continue resumes correctly
   - verify Save/Load Preset on Score row 1 and Save Preset red dirty indicator
   - verify `No Display` selections round-trip via Save Preset / Load Preset
   - verify `No Display` lanes are excluded from PDF export lane output
  - verify Effects tab Chan/Mute/Solo/Volume/Expression/Reverb controls; Chan seeds from track channel (default 1); volume/expression/reverb CC-seeded defaults (100/100/10)

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
