# Fixture Specs

## 1) tempo-time-signature-changes
- Start in 4/4 at 120 BPM
- Change to 3/4 at bar 3
- Tempo change to 90 BPM at bar 4
- Purpose: verify bar mapping and playback sync over meta changes
- Related tests:
  - `testTempoMap()`
  - `testMidiLoaderRejectsSmpte()`
  - `testWindowClampBehaviorForScoreWindow()`
  - `testTempoTimeSigFixtureBehavior()` via `tempo_time_sig.mid`

## 2) syncopated-note-durations
- Notes starting off-beat (16th offsets)
- Durations quantized to 1/16, 1/8, 1/4, 1/2, 1
- Purpose: verify quantizer rounding and symbol mapping
- Related tests:
  - `testQuantizer()`
  - `testScoreModelNormalizesChordQuarterInBar()`
  - `testSyncopatedDurationsFixtureBehavior()` via `syncopated_durations.mid`

## 3) altered-chords
- Harmonic windows including: Cmaj9, G7b9, D7#9, F13
- Purpose: verify extended jazz chord naming and slash-bass handling
- Related tests:
  - `testChordDetector()`
  - `testChordDetectorResetsAcrossSilence()`
  - `testScoreModelNormalizesChordQuarterInBar()`
  - `testAlteredChordsFixtureBehavior()` via `altered_chords.mid`

## 4) ties-across-bars
- Sustained notes crossing bar boundaries
- Purpose: verify tie indicator model behavior
- Related tests:
  - `testTrackNoteExtractorFlushesOrphans()`
  - `testScoreModelSplitsCrossBarNotes()`
  - `testTiesSyncopationFixtureBehavior()` via `ties_syncopation.mid`
