# MidiScorer

MidiScorer is a JUCE/C++ standalone desktop app that reads MIDI files, renders up to three selected tracks as score-like notation, detects chords, and follows playback with a rolling 5-bar view.

![MidiScorer UI](resources/midiscorer.png)

## Current capabilities

- Load `.mid` / `.midi` files.
- Auto-load last saved UI preset when a MIDI file is loaded (if present).
- Display up to three independent staffs:
  - per-staff track selection
  - per-staff clef selection (`Treble` / `Bass`)
- Build tempo, time-signature, and key-signature maps from MIDI meta events.
- Quantize note starts and durations to:
  - 1/16, 1/8, 1/4, 1/2, whole
- Render score-style notation with:
  - noteheads, stems, flags, ties
  - key-aware accidental display (`#`/`b` preference by key signature)
  - key-signature glyphs (up to 7 sharps/flats) rendered at the first visible bar on each staff
  - explicit rest symbols (gaps are modeled and rendered as rests)
- Detect/display chords:
  - static bar chord label (left-aligned)
  - live chord marker recomputed on 1/8-note boundaries, shown only when chord changes
- Chord source selection:
  - dynamic per-track checkbox list (`Chord Tracks`) used for harmonic analysis
- Playback controls:
  - `Play`, `Stop`, `Continue`, and bar start input
  - on `Stop`, continue bar auto-fills with current bar
- Display options:
  - `Light Score` / dark score toggle (Light Score is default)
  - status line includes Sig, Tempo, Key, and Bar
- UI layout refinements:
  - first-row controls place transpose and score color toggle directly after chord naming options
  - staff selector row is compacted for faster Staff 1/2/3 scanning
  - `Save Preset` / `Load Preset` are placed directly in the status row
- Window title includes loaded MIDI filename.

## Project structure

- `CMakeLists.txt` - JUCE/CMake project setup
- `Main.cpp` - JUCE application entry point
- `src/app/MainComponent.h` - UI controls, orchestration, playback updates
- `src/midi/TempoMap.h` - tempo/time-signature/bar conversion
- `src/midi/TrackNoteExtractor.h` - note-on/note-off pairing
- `src/midi/MidiProjectLoader.h` - MIDI ingestion and metadata extraction
- `src/notation/Quantizer.h` - rhythmic quantization
- `src/notation/ScoreModel.h` - score bars/notes/chords/rest insertion
- `src/notation/ScoreRenderer.h` - score drawing and live chord marker rendering
- `src/harmony/ChordDetector.h` - chord analysis and naming options
- `src/playback/PlaybackClock.h` - playback timing core
- `src/playback/PlaybackController.h` - playback state + current bar
- `tests/test_main.cpp` - core tests
- `tests/fixtures/` - fixture specs/documentation

## Requirements

- CMake 3.22+
- C++17 compiler
- JUCE source checkout available in one of:
  - `-DJUCE_ROOT=<path-to-JUCE>`
  - `.deps/JUCE` under this project
  - `C:/JUCE` (Windows auto-detected)

## Build (Windows example)

```powershell
cmake -S . -B build -DJUCE_ROOT="C:/JUCE"
cmake --build build --config Debug --target MidiScorer
```

Output executable:

- `build/MidiScorer_artefacts/Debug/MidiScorer.exe`

## Run tests

```powershell
cmake --build build --config Debug --target MidiScorerTests
ctest --test-dir build -C Debug --output-on-failure
```

## How to use

1. Launch `MidiScorer`.
2. Click **Load MIDI** and choose a MIDI file.
3. Optionally let auto-preset apply, or use **Load Preset** manually.
4. Configure Staff 1/2/3 track and clef.
5. Choose harmonic source tracks using **Chord Tracks** checkboxes.
6. Optionally adjust:
   - global transpose
   - chord naming options
   - score color mode
7. Click **Play**.
8. Click **Stop** (Continue bar auto-populates current bar).
9. Click **Continue** to resume from selected bar.

## Notes and known limitations

- Rendering is intentionally simplified (single-voice approximation per staff).
- Quantization is limited to 1/16 through whole-note values.
- Rests, beaming, and accidental handling are practical approximations, not full engraving rules.
- Chord detection uses deterministic template scoring and may be ambiguous for dense voicings.
- Playback currently drives visual sync; MIDI output transport/routing is not DAW-grade.

## Developer notes

- `MIDI ingest pipeline`
  - Entry point is `MidiProjectLoader::load()` in `src/midi/MidiProjectLoader.h`.
- `Tempo/bar math`
  - `src/midi/TempoMap.h` is the authoritative timing layer.
- `Chord detection`
  - `src/harmony/ChordDetector.h` contains template matching and naming rules.
  - `detectInWindow(...)` supports live playback window detection.
- `Notation model`
  - `src/notation/ScoreModel.h` inserts explicit rest symbols per bar by gap-filling occupied note spans.
- `Notation rendering`
  - `src/notation/ScoreRenderer.h` handles static chord labels, live chord marker, notes, rests, accidental display, and first-visible-bar key signatures per staff.
- `UI orchestration`
  - `src/app/MainComponent.h` coordinates all preferences, preset load/save, multi-staff rebuilds, and timer-based updates.

## First contribution checklist

1. Configure/build locally.
2. Run `ctest` and ensure zero failures.
3. Smoke test:
   - load MIDI
   - verify staff selectors and chord track checkboxes
   - verify Play/Stop/Continue behavior
   - verify live chord marker updates on playback
