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
