# MidiScorer Technical Design

This document describes the internal design of MidiScorer with emphasis on:

- note scoring (notation model + rendering pipeline)
- chord detection (analysis windows, template scoring, naming)

It is intended for maintainers and contributors who need implementation-level context.

## 1) System overview

MidiScorer is a JUCE desktop app that loads a MIDI file, builds timing/key metadata, extracts notes per track, then derives two visual products:

- a score-like staff view (up to 3 staffs)
- chord annotations (static + live)

The default user workflow is non-destructive: loaded MIDI files are treated as source input, while user edits (track selection, mix controls, transpose/key/tempo overrides, loop settings, etc.) are stored and restored from profile state in `Documents/MidiScorer/ui_preset.json`.

Core modules:

- `src/midi/MidiProjectLoader.h` - MIDI ingestion, SMF type/SMPTE validation, metadata extraction
- `src/midi/TempoMap.h` - tick/quarter/second/bar conversion
- `src/midi/TrackNoteExtractor.h` - note-on/off pairing into note events
- `src/notation/Quantizer.h` - rhythmic quantization
- `src/notation/ScoreModel.h` - score-domain symbols (notes, rests, ties, chords)
- `src/notation/ScoreRenderer.h` - drawing engine for staff notation
- `src/notation/SimplePdfWriter.h` - minimal JPEG-backed PDF writing utility
- `src/harmony/ChordDetector.h` - chord-template detection and naming
- `src/app/ScorePdfExporter.h` - full-song score export pagination/orchestration
- `src/app/AppTabsHost.h` - top-level tab host (`Start`, `Score`, `Effects`)
- `src/app/MainComponent.h` - score-page orchestration, UI state, playback sync
- `src/app/PlayerTabComponent.h` - player-page MIDI output UI
- `src/app/TracksTabComponent.h` - per-track mix UI (Chan/mute/solo/volume/reverb)
- `src/playback/IPlaybackPositionSource.h` - transport position abstraction
- `src/playback/MidiFilePlaybackEngineAdapter.h` - scheduled MIDI event engine adapter
- `src/playback/MidiOutputDevice.h` - single MIDI output device abstraction
- `src/playback/TrackMixState.h` - per-track mix state container
- `src/playback/TrackMixProcessor.h` - per-track playback filtering/scaling/merge
- `src/playback/TrackMixMidiSeed.h` - initialize mix from per-track channel/CC7/CC91 on load

## 2) End-to-end data flow

1. `MainComponent::loadMidiFile()` opens a JUCE file chooser and loads a MIDI file.
2. `MidiProjectLoader::load()` validates the Standard MIDI File header, then reads tracks, tempo/time signatures, key signature, and note events.
3. `TempoMap` is built and becomes the canonical timing conversion layer.
4. For each visible staff, `MainComponent::rebuildStaff()`:
   - selects source track
   - applies effective transpose (global + key override; drum exceptions)
   - quantizes notes via `Quantizer`
   - computes chord events via `ChordDetector::detect()`
   - builds score bars via `ScoreModel::build()`
5. `ScoreRenderer` paints rolling 5-bar windows centered on current playback bar.
6. `MainComponent::exportScorePdf()` can render full-song pages through `ScorePdfExporter` and persist them via `SimplePdfWriter`.
7. During playback, `timerCallback()` updates current bar/live chord markers and dispatches scheduled MIDI events to the selected output device.
8. Before dispatch, each event is filtered/scaled/remapped by track mix state (mute/solo gate, channel remap, volume/reverb scaling).

## 2.0) MIDI file requirements

`MidiProjectLoader` enforces ingest rules immediately after `juce::MidiFile::readFrom(..., &midiFileType)`:

- **Type 0 rejected** — SMF type **0** (single merged track) returns `false`, sets `rejectedType0`, and uses `getType0NotSupportedMessage()` so the UI can show a conversion modal. MidiScorer expects type **1** files with separate tracks per part.
- **SMPTE/non-PPQ rejected** — negative or zero PPQ time format returns an error before tempo-map construction.
- **Type 1 accepted** — normal multi-track ingest continues into tempo-map and per-track note extraction.

`MainComponent::loadMidiFile()` branches on `rejectedType0` to call `showWarningModal(...)` instead of only updating the status line.

## 2.1) Playback and output architecture

The player path is intentionally loosely coupled:

- `PlaybackController` implements `IPlaybackPositionSource` and remains the single timing authority used by score rendering and live chord windows.
- `MidiFilePlaybackEngineAdapter` loads MIDI message events from file and emits events up to a caller-provided playback time.
- `MainComponent::timerCallback()` bridges the two by passing `getElapsedSeconds()` into the adapter and routing emitted messages through `MidiOutputDevice`.
- `PlayerTabComponent` exposes MIDI output selection; playback transport lives on the Score tab.

**Tempo override policy (multi-tempo files):** override BPM is compared to the file's opening tempo and applies a uniform playback-rate scale via `PlaybackController::setTempoOverrideBpm()`. Internal tempo-change ratios in the source map are preserved; bar positions, score timing, and MIDI event scheduling remain tied to the source tempo map's song-time axis.

This keeps the scorer and player separable for future reuse in a tabbed host such as AMidiOrganOrg.

## 2.2) Per-track playback mix

Track mix is source-track based (not MIDI-channel strip based):

- `MidiFilePlaybackEngineAdapter::ScheduledEvent` carries `sourceTrackIndex`.
- `TrackMixState` holds per-track `channel`, `volume`, `reverb`, `muted`, and `solo`.
- `TrackMixMidiSeed` initializes mix on load:
  - scans each project track sequence for the first non-meta MIDI channel (default **1**),
  - scans for the last CC7 (volume) and CC91 (reverb),
  - uses defaults **100** / **10** when a controller is absent on that track,
  - runs before preset overlay so saved values can override seeded values.
- `TrackMixProcessor` applies:
  - solo precedence (if any solo active, only solo tracks pass),
  - mute blocking when no solo is active,
  - output channel remap to the per-track **Chan** value,
  - volume scaling for note-on velocity and CC7/CC11 controller values,
  - reverb merge for CC91: `(fileValue * trackReverb) / 127`.
- `TracksTabComponent` exposes grouped controls by track name (Chan, Mute, Solo, Volume, Reverb).
- Use **Chan** changes if you need to reorganize channels in order to play along with the score and MIDI file while playing an instrument that shares the selected MIDI module.
- `MainComponent` persists per-song mix entries in `ui_preset.json` under `trackMixBySong` keyed by normalized song path; mix control edits auto-save via `onTrackMixStateChanged()`.

## 3) Note scoring design

### 3.1 Score domain model

`ScoreModel` stores notation in score-space, not raw MIDI-space:

- `ScoreBar`
  - bar number + active time signature
  - `notes` (`ScoreNoteSymbol`)
  - `chords` (`ChordAnnotation`, bar-relative)
- `ScoreNoteSymbol`
  - absolute quarter position + `quarterInBar`
  - quantized duration (`durationQuarter`, `value`)
  - MIDI pitch
  - `isRest` flag
  - `tieIntoNextBar` flag

This separation allows rendering and playback sync to operate on bar-relative notation primitives.

### 3.2 Quantization boundary

Quantization occurs before score construction (`Quantizer` output enters `ScoreModel::build()`).

Current rhythmic target set:

- sixteenth (0.25q)
- eighth (0.5q)
- quarter (1.0q)
- half (2.0q)
- whole (4.0q)

This intentionally trades engraving completeness for stable readability in mixed MIDI sources.

### 3.3 Cross-bar note segmentation and ties

`ScoreModel::build()` segments a note whenever it crosses a bar boundary:

- determine active bar from `TempoMap::quarterToBar()`
- clamp segment end to next bar downbeat
- emit one symbol per segment
- set `tieIntoNextBar = true` for non-final segments

Result: sustained MIDI notes preserve continuity across bars while still fitting measure-local rendering.

### 3.4 Rest insertion (gap filling)

Rests are explicit symbols, not an implicit absence of notes.

Process per bar:

1. Collect occupied time spans from non-rest notes.
2. Merge overlapping spans.
3. Fill uncovered regions with rests using greedy longest-fit durations:
   - 4.0, 2.0, 1.0, 0.5, 0.25 quarter notes

This is handled by `insertRestsIntoBar()` + `addRestGap()`.

Outcome:

- every bar has complete rhythmic coverage
- visual rhythm remains legible even with sparse or irregular MIDI input

### 3.5 Clef, key spelling, and pitch placement

`ScoreRenderer` converts MIDI pitch into display spelling (`SpelledPitch`) and vertical position:

- spelling mode from key signature:
  - sharps
  - flats
  - mixed-in-C fallback
- accidental text derived from spelling, not raw pitch-class constants
- vertical placement uses diatonic steps for treble/bass
- drum mode uses practical percussion mapping and x-noteheads for cymbal/hat classes

This avoids semitone-linear staff placement artifacts and improves key-aware readability.

### 3.6 Rendering primitives

`ScoreRenderer::drawBar()` draws:

- bar frame, staff lines, beat guides
- first-visible-bar clef and key signature glyphs
- noteheads/stems/flags/ledger lines
- ties
- rest symbols by note value
- top header labels (`Bar N` + track/instrument label)

The component supports light/dark color schemes with the same geometry.

### 3.7 Playback-coupled score window

The rendered window is a fixed rolling context:

- 2 bars before current
- current bar
- 2 bars after current

`setCurrentBar()` clamps to model range, preventing invalid view states near edges.

### 3.8 Full-song PDF export path

Score export reuses the existing notation stack rather than building a second renderer:

1. `MainComponent` gathers non-empty staff lanes (`ScoreModel` + `ScoreRenderer` pairs).
   - lane collection respects selected export mode:
     - `All active staffs` collects all non-empty lanes
     - `Staff 1 only` collects lane 1 only when non-empty
2. `ScorePdfExporter` paginates bars `1..maxBar` in fixed-size systems (`barsPerRow`).
3. For each system/lane, `ScoreRenderer::paintBarStrip(...)` draws bars with:
   - static chords enabled,
   - live chord marker disabled,
   - no playback highlight.
4. Page images are passed to `SimplePdfWriter`, which embeds JPEG page images into a valid PDF document.

This keeps export non-destructive and consistent with on-screen notation behavior.

The selected export mode is persisted in the preset payload (`pdfExportMode`) and restored on load.

## 4) Chord detection design

### 4.1 Input selection

Chord analysis does not rely on a single displayed staff track.

`MainComponent` builds a merged note set from selected "Chord Tracks" checkboxes:

- track list is filtered to tracks that contain note events or non-meta MIDI events
- checkbox selections map back to original track indices
- merged note list is optionally transposed using effective transpose
- GM percussion channel (10) is excluded from transpose

### 4.2 Static chord annotation pass

`ChordDetector::detect()` scans by bars and quarter-note windows:

- for each bar in `[1..maxBar]`
- for each quarter-note window in bar
- call `detectWindow(notes, secA, secB, options)`

Dedup behavior:

- annotations emitted only when symbol changes
- `previousSymbol` resets per bar and on silence windows

This produces stable, non-redundant chord markers in score space.

### 4.3 Live chord detection pass

During playback (`timerCallback()`):

- recompute at 1/8-note boundaries
- detect in half-quarter windows (`0.5q`)
- update marker only when symbol or marker position changes
- clear marker on empty windows

Live markers are tracked per staff (`liveChordStates`) and rendered separately from static bar labels.

### 4.4 Window extraction

`detectWindow()`:

- includes notes whose time intervals overlap the query window
- extracts pitch-class set (`pcs`)
- tracks bass (minimum MIDI note) and highest note

Minimum threshold:

- fewer than 3 pitch classes -> no chord result

### 4.5 Template matching and scoring

Chord templates are defined as:

- `suffix`
- required intervals
- optional intervals

Detection loops over all 12 roots and all templates:

- skip if required intervals missing
- score each observed pitch class:
  - +5 required
  - +2 optional
  - -1 other
- add context bonuses:
  - +2 if bass pitch-class equals root
  - +1 if highest note equals perfect fifth of root

Best score wins.

This deterministic scorer is simple, explainable, and fast enough for realtime updates.

### 4.6 Naming and display style

Root/bass spelling controlled by `NamingOptions`:

- `preferSharps` or `preferFlats`
- `plain` or `jazzSymbols` suffix style

Slash chord output is emitted when bass pitch-class differs from root.

Examples:

- `Cmaj7`
- `Ebm7`
- `G7/B`

## 5) Key and transpose interactions

Effective transpose used by notation/chord analysis:

- `effective = globalTranspose + keyOverrideDelta`

where key override delta is:

- override tonic pitch-class minus detected MIDI tonic pitch-class
- normalized to nearest interval in `[-6, +6]`

Important:

- detected MIDI key uses major/minor conversion compatible with the tonic lookup helper
- drum clef and channel-10 percussion remain untransposed

## 5.1) Score status line

`MainComponent::refreshStatusMessage()` composes the Score tab status label as:

`Sig | Bar | {playback/status message} | Tempo | KeySrc`

- **Sig** — time signature from tempo map (first signature event)
- **Bar** — current bar / max bar
- **Tempo** — detected BPM, with override text when tempo override is active
- **KeySrc** — whether key used for notation/transposition is **detected** (MIDI key signature) or **override** (Key field)

Playback messages include `Playback running`, `Playback stopped`, `Playback continuing from bar N`, and `Playback finished.`

## 6) Design constraints and trade-offs

- Single-voice approximation per staff (not full polyphonic engraving)
- Quantization limited to five note values
- Chord detection is template-based (not probabilistic/hierarchical)
- Realtime behavior prioritized over exhaustive harmonic interpretation

These constraints keep behavior predictable and maintainable for mixed-quality MIDI sources.

## 7) Extension points

Likely future improvements:

- richer rhythmic values (triplets, dotted durations)
- voice separation for dense piano tracks
- stronger chord context model (inversion weighting, temporal smoothing)
- configurable chord detection window sizes
- richer tie/rest engraving rules
- per-track output routing (currently out of scope by design)

## 8) Validation strategy

Current verification layers:

- unit tests in `tests/test_main.cpp` for timing/chord/model regressions
- rebuild + playback smoke tests for UI/render sync
- fixture-based MIDI scenarios for edge cases (cross-bar notes, silence resets, SMPTE rejection, orphan note-ons)

For changes touching scoring or harmony logic, re-run both tests and manual playback checks.
