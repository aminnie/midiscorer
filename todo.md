# MidiScorer Implementation Todo

This checklist tracks code-review findings and implementation work items.

## Priority 0 - Critical correctness

- [ ] Reject unsupported SMPTE MIDI timing with a clear load error.
  - Files: `src/midi/MidiProjectLoader.h`
  - Acceptance:
    - Loader returns `false` for `getTimeFormat() <= 0`.
    - Error message states SMPTE/non-PPQ timing is unsupported.
- [ ] Flush orphaned note-ons at end-of-track.
  - Files: `src/midi/TrackNoteExtractor.h`
  - Acceptance:
    - Remaining active notes are emitted with `endTick` at final track tick.
    - Output is time-sorted and non-negative in duration.

## Priority 1 - Chord/model correctness

- [ ] Split notes across bar boundaries and preserve tie continuity.
  - Files: `src/notation/ScoreModel.h`
  - Acceptance:
    - Spanning notes create continuation note symbols in following bars.
    - Continuations do not get replaced by rests.
    - Tie flags are set on segments that continue into next bar.
- [ ] Reset chord dedup behavior across silence/bar boundaries.
  - Files: `src/harmony/ChordDetector.h`
  - Acceptance:
    - Same chord after silence is re-announced.
    - Downbeat re-annotation can occur per bar policy.
- [ ] Clear live chord markers on empty windows and allow re-attack.
  - Files: `src/app/MainComponent.h`
  - Acceptance:
    - Marker hides when no chord is detected.
    - Same symbol can reappear after silence.

## Priority 2 - Rendering reliability/fidelity

- [ ] Render multiple static chord changes per bar.
  - Files: `src/notation/ScoreRenderer.h`
  - Acceptance:
    - Bar with multiple chord annotations displays all (or deterministic subset by beat position).
- [ ] Prevent blank rendering when current bar is outside available model window.
  - Files: `src/notation/ScoreRenderer.h`
  - Acceptance:
    - Renderer clamps/falls back to valid bar range and always paints bars when model is non-empty.

## Priority 3 - Regression tests

- [ ] Expand tests for loader/extractor/model/chord/live-marker behavior.
  - Files: `tests/test_main.cpp`
  - Acceptance:
    - Added tests for SMPTE rejection, orphan flush, cross-bar split/ties, chord dedup reset, and window clamp behavior.
    - Existing tests continue to pass.
- [ ] Align fixture intent to tests.
  - Files: `tests/fixtures/fixture-specs.md` (mapping via test names/comments)
  - Acceptance:
    - New tests reference fixture-like scenarios (tempo changes, syncopation, ties, altered chords).
