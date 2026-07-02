# MidiScorer

MidiScorer is a JUCE/C++ standalone desktop app that reads MIDI files, renders up to three selected tracks as score-like notation, detects chords, follows playback with a rolling 5-bar view, and can emit MIDI playback to a selected external MIDI output device.

![MidiScorer UI](src/resources/midiscorer.png)

![MidiScorer Score tab](src/resources/scorereffects.png)

## Current capabilities

- Load `.mid` / `.midi` files (Standard MIDI File type **1** only).
  - type **0** files (single-track) are rejected with a conversion guidance modal
  - SMPTE/non-PPQ timing is also rejected
- Auto-load last saved UI preset when a MIDI file is loaded (if present).
- Display up to three independent staffs:
  - per-staff track selection
  - per-staff `No Display` option to hide a lane
  - per-staff clef selection (`Treble` / `Bass` / `Drum`)
- Build tempo, time-signature, and key-signature maps from MIDI meta events.
- Quantize note starts and durations to:
  - 1/16, 1/8, 1/4, 1/2, whole, plus dotted variants (e.g. dotted quarter = 1.5 beats, dotted half = 3 beats)
- Render score-style notation with:
  - noteheads, stems, flags, ties, augmentation dots
  - open (outline) noteheads for half and whole notes; filled noteheads for quarter and shorter values
  - key-aware accidental display (`#`/`b` preference by key signature)
  - first-visible-bar start symbols:
    - enlarged clef glyphs (treble/bass)
    - enlarged key-signature glyphs (up to 7 sharps/flats)
  - per-measure top-right staff header text shows selected track/instrument name
  - drum clef mode uses percussion-oriented note placement and x-noteheads for hat/cymbal hits
  - explicit rest symbols (gaps are modeled and rendered as rests)
- Detect/display chords:
  - static bar chord label (left-aligned)
  - configurable chord detection grid (`Quarter` or `Eighth`) used for both static labels and live marker windows
  - configurable chord vocabulary level (`Simple`, `Standard`, `Rich`) used to constrain allowed suffix templates
  - live chord marker shown only when chord text or marker position changes
- Chord source selection:
  - dynamic per-track checkbox list (`Chord Tracks`) used for harmonic analysis
- Playback controls (Score tab):
  - **Start/Stop**, **Continue**, and bar start input
  - optional A-B loop controls for repeating a series of bars during practice
- Tabbed workspace (tab order: **Start**, **Score**, **Sounds**, **Effects**):
  - `Start` tab for MIDI output device selection and optional **Reopen last MIDI on startup** (transport controls are on the Score tab)
  - `Score` tab for notation/chord controls and renderers
  - `Sounds` tab for module/category voice browsing and side-panel sound details
  - `Effects` tab for per-track mix controls and sound selection launch buttons
- MIDI player and output:
  - MIDI file playback events are scheduled from file time and dispatched on the same transport timeline that drives live score/chord updates
  - single selected MIDI output device (GM-oriented output path)
  - persisted selected output device identifier under `Documents/MidiScorer/midi_output.json`
- Per-track playback mix:
  - per eligible MIDI track controls for **Chan** (1..16), **Sound**, **Mute**, **Solo**, **Volume**, **Expression**, and **Reverb** (0..127)
  - on load, **Chan** seeds from the track's first non-meta MIDI channel (default **1**); volume/expression/reverb seed from the track's last **CC7** / **CC11** / **CC91** when present; otherwise defaults are **100** / **100** / **10**
  - on load, sound defaults seed from the track's last **CC0**/**CC32**/**Program Change** values when present
  - saved per-song mix in `ui_preset.json` (`trackMixBySong`) overrides MIDI-seeded values after edit/save
  - grouped controls by track name in the `Effects` tab (AMidiOrgan-style slider colors)
  - **Chan** remaps outgoing playback events (notes, program change, CC, etc.) to the chosen output channel
  - selecting a sound stores per-track **MSB/LSB/Program + sound name** in the same per-song preset payload
  - selected sound overrides are proactively sent on **Start**, **Continue**, **Seek**, and loop wrap
  - incoming MIDI **CC0**/**CC32**/**Program Change** events are replaced with the selected per-track override values during playback
  - note-on velocity is compounded by volume/expression (`volume * expression`), volume scales CC7, expression scales CC11, and reverb merges CC91 during playback
  - use **Chan** changes if you need to reorganize channels in order to play along with the score and MIDI file while playing an instrument that shares the selected MIDI module
- Per-song score overrides (stored in `Documents/MidiScorer/ui_preset.json`):
  - transpose per song under `transposeOverridesBySong`
  - key override per song under `keyOverridesBySong`
  - tempo override per song under `tempoOverridesBySong` (scales playback uniformly against the file's opening tempo; later tempo changes keep their relative ratios)
  - chord complexity per song under `chordComplexityBySong` (`Rich` default)
  - chord detection resolution per song under `chordResolutionBySong` (`Quarter` default)
  - staff/chord-track/accidental/alias/chord-level selections (including `No Display`) and other score UI state per song
- Non-destructive workflow:
  - loaded MIDI files are treated as source material and are not rewritten by the app
  - when loading a MIDI outside the configured working directory, MidiScorer prompts for a working-copy filename before copying; cancel aborts the load
  - startup auto-resume loads still skip the rename prompt and use the existing automatic working-directory copy behavior
  - user edits are persisted in profile data and can be reloaded with **Load Preset**
- Full-song score PDF export:
  - `PDF mode` selector supports:
    - `All active staffs` (current behavior)
    - `Staff 1 only`
  - **Export PDF...** writes a multi-page rendered score for bars `1..maxBar` using the selected mode
  - export includes static chord labels and excludes live playback chord markers
  - output is a rendered document (non-MIDI), preserving the app's non-destructive workflow
  - selected PDF mode is remembered in `ui_preset.json`
- Display options:
  - `Light Score` / dark score toggle (Light Score is default)
  - each staff has a display-octave selector: `-`, up arrow, down arrow
  - display-octave shift affects notation rendering and PDF output only (not MIDI playback/chord analysis)
  - Score tab includes a disclosure-style `Hide/Unhide` arrow button that collapses/expands the staff/chord/tempo control rows
  - control rows auto-hide when playback starts (`Start` or `Continue`) and can be re-shown at any time with the toggle
  - status line order: **Sig**, **Bar**, playback message, **Tempo**, **KeySrc**
  - `Save Preset` turns red when score song settings are dirty; returns to default after save/load
- Score tab UI layout (row summary):
  - row 1: Load MIDI, Start/Stop, Continue, Bar, Save/Load Preset, Export PDF + mode, Load Recent, Chord Tracks label, Hide/Unhide
  - row 2: staff track/display-octave/clef selectors
  - row 3: Chord Grid controls + Chord Tracks checkboxes
  - row 4: Tempo, Assign, Key, Transpose, status line
- Window title includes loaded MIDI filename.

## Project structure

- `CMakeLists.txt` - JUCE/CMake project setup
- `Main.cpp` - JUCE application entry point
- `src/app/AppTabsHost.h` - top-level tab container (`Start` + `Score` + `Sounds` + `Effects`)
- `src/app/MainComponent.h` - score page UI controls, notation orchestration, playback sync
- `src/app/ScorePdfExporter.h` - full-song score pagination/export assembly
- `src/app/PlayerTabComponent.h` - player page MIDI output selection
- `src/app/TracksTabComponent.h` - Effects tab (per-track Chan/Sound/Mute/Solo/Volume/Expression/Reverb)
- `src/app/SoundsTabComponent.h` - Sounds tab (module/category selection and commit back to Effects)
- `src/sounds/InstrumentCatalog.h` - local module/category/voice catalog loader
- `src/resources/icons/app-icon-master.png` - application icon source (1024×1024 ARGB; embedded on Windows via CMake)
- `src/resources/icons/MidiScorer.icns` - macOS bundle icon (generate with `scripts/build-macos-icon.sh`)
- `src/midi/TempoMap.h` - tempo/time-signature/bar conversion
- `src/midi/TrackNoteExtractor.h` - note-on/note-off pairing
- `src/midi/MidiProjectLoader.h` - MIDI ingestion, SMF type validation, and metadata extraction
- `src/notation/Quantizer.h` - rhythmic quantization
- `src/notation/ScoreModel.h` - score bars/notes/chords/rest insertion
- `src/notation/ScoreRenderer.h` - score drawing and live chord marker rendering
- `src/notation/SimplePdfWriter.h` - minimal PDF writer for rasterized score pages
- `src/harmony/ChordDetector.h` - chord analysis and naming options
- `src/playback/PlaybackClock.h` - playback timing core
- `src/playback/PlaybackController.h` - playback state/current bar implementing position-source interface
- `src/playback/IPlaybackPositionSource.h` - transport position abstraction boundary
- `src/playback/MidiFilePlaybackEngineAdapter.h` - scheduled MIDI-event playback adapter
- `src/playback/MidiOutputDevice.h` - single-output MIDI device abstraction
- `src/playback/TrackMixState.h` - per-track channel/sound/volume/expression/reverb/mute/solo state
- `src/playback/TrackMixProcessor.h` - playback gating, channel remap, and per-track message scaling/merge
- `src/playback/TrackMixMidiSeed.h` - seed Chan/sound and mix sliders from track sequences on load
- `tests/test_main.cpp` - core tests
- `tests/fixtures/` - fixture specs/documentation

## Build and test

See **[build.md](build.md)** for configure, build, test, and launch instructions for Windows and macOS.

Quick start (Windows):

```powershell
.\scripts\build.ps1 -Configuration Debug -Target All
```

Requirements: CMake 3.22+, C++17, and a JUCE source checkout (details in `build.md`).

Quick start (macOS):

```bash
./scripts/mac-build.sh
open "build-mac/MidiScorer_artefacts/Debug/MidiScorer.app"
```

`mac-build.sh` configures, builds `MidiScorer` + `MidiScorerTests`, and runs `ctest`.

## How to use

1. Launch `MidiScorer`.
2. Click **Load MIDI** and choose a type **1** MIDI file.
   - if the file is type **0**, a warning modal explains how to convert/export it as type **1**
3. Optionally let auto-preset apply, or use **Load Preset** manually.
   - this restores your saved profile changes for the current song without changing the MIDI file
4. Configure Staff 1/2/3 track and clef.
   - use `Drum` clef for percussion tracks
5. Choose harmonic source tracks using **Chord Tracks** checkboxes.
6. Optionally adjust:
   - transpose
   - key override text
   - key `Assign` checkbox (session-only; checked: profile-only key text, no transpose)
   - chord naming options (`Sharp/Flat`, `Plain/Jazz`, `Simple/Standard/Rich`)
   - score color mode
7. Use **Score** tab to view/edit notation options and track assignments.
8. Choose the Score tab `PDF mode` (`All active staffs` or `Staff 1 only`).
9. Use **Export PDF...** on the Score tab to write a full-song rendered score PDF.
   - export uses current staff selections/clefs/chord labels according to selected PDF mode
   - live chord marker overlays are excluded from static export
10. Use **Start** tab to select a MIDI output device.
11. Use **Score** tab **Start/Stop**, **Continue**, and **Bar** for playback transport.
12. Use **Effects** tab to adjust per-track **Chan**, **Sound**, Mute, Solo, Volume, Expression, and Reverb.
    - Use **Chan** changes if you need to reorganize channels in order to play along with the score and MIDI file while playing an instrument that shares the selected MIDI module.
13. Click a track **Sound** button to open **Sounds**, select a module/category/voice, then click **Back to Effects** to save that track override.

## Notes and known limitations

- Type **0** files are rejected with conversion guidance; SMPTE/non-PPQ files remain unsupported.
- SMPTE/non-PPQ MIDI timing is not supported.
- Quantization is limited to 1/16 through whole-note values, including single augmentation dots (not triplets or double dots).
- Rests, beaming, and accidental handling are practical approximations, not full engraving rules.
- Chord detection uses deterministic template scoring and may be ambiguous for dense voicings.
- Chord detection grid is selectable (`Quarter` default, `Eighth` optional) and can still be ambiguous for dense voicings.
- Chord complexity level is selectable (`Simple`, `Standard`, `Rich`) and limits which suffix templates can be emitted.
- Playback drives visual sync and optional MIDI output from one shared timeline.
- Output is intentionally limited to a single GM-oriented MIDI destination.
- Profile save/load is the supported way to keep or revert edits; the app does not rewrite source MIDI files.
- PDF export is rendered page output (JPEG-backed pages in PDF), not vector engraving.
- Track mix controls apply per source MIDI track:
  - **Chan** remaps outgoing playback to the selected MIDI channel (1..16)
  - note-on velocity is compounded by volume/expression (`volume * expression`)
  - volume scales CC7 and expression scales CC11
  - reverb merges CC91 when present in the file
  - **Chan** seeds from the track's first channel on load (default 1); volume/expression/reverb seed from last CC7/CC11/CC91 per track (defaults 100/100/10) unless overridden by saved preset

## Developer notes

- `MIDI ingest pipeline`
  - Entry point is `MidiProjectLoader::load()` in `src/midi/MidiProjectLoader.h`.
  - Rejects SMF type **0** files in app load flow and rejects SMPTE/non-PPQ files.
- `Tempo/bar math`
  - `src/midi/TempoMap.h` is the authoritative timing layer.
- `Chord detection`
  - `src/harmony/ChordDetector.h` contains template matching and naming rules.
  - `detect(...)` and `detectInWindow(...)` support quarter/eighth detection windows and simple/standard/rich complexity filtering.
- `Notation model`
  - `src/notation/ScoreModel.h` inserts explicit rest symbols per bar by gap-filling occupied note spans.
- `Notation rendering`
  - `src/notation/ScoreRenderer.h` handles static chord labels, live chord marker, notes/rests (open vs filled noteheads, augmentation dots), per-staff instrument header labels, first-visible-bar clef/key symbols, and drum-mode note rendering.
- `Staff lane visibility`
  - `No Display` hides a selected staff lane in Score view and excludes it from PDF lane export selection.
- `UI orchestration`
  - `src/app/MainComponent.h` coordinates all preferences, preset load/save, multi-staff rebuilds, and timer-based updates.

## First contribution checklist

1. Configure and build locally (`build.md`).
2. Run `ctest` and ensure zero failures.
3. Smoke test:
   - load MIDI
   - verify staff selectors and chord track checkboxes
   - verify Start/Stop/Continue behavior
  - verify type **0** MIDI shows conversion guidance modal and is rejected
   - verify live chord marker updates on playback

## Chord Complexity Whitelist

The detector uses suffix-template whitelists per chord complexity level:

- `Simple` (beginner): triads and sus only
- `Standard` (intermediate): simple + common 6/7/9/11/13 extensions
- `Rich` (advanced): full set including altered dominant forms

Ready-to-paste JSON form:

```json
{
  "simple": ["", "m", "dim", "aug", "sus2", "sus4"],
  "standard": ["", "m", "dim", "aug", "sus2", "sus4", "6", "m6", "7", "maj7", "m7", "mMaj7", "add9", "9", "m9", "11", "13"],
  "rich": ["", "m", "dim", "aug", "sus2", "sus4", "6", "m6", "7", "maj7", "m7", "mMaj7", "add9", "9", "m9", "11", "13", "7b5", "7#5", "7b9", "7#9"]
}
```

Ready-to-paste C++ form:

```cpp
constexpr std::array<const char*, 6> kSimpleSuffixes = {
    "", "m", "dim", "aug", "sus2", "sus4"
};

constexpr std::array<const char*, 17> kStandardSuffixes = {
    "", "m", "dim", "aug", "sus2", "sus4", "6", "m6", "7", "maj7", "m7", "mMaj7", "add9", "9", "m9", "11", "13"
};

constexpr std::array<const char*, 21> kRichSuffixes = {
    "", "m", "dim", "aug", "sus2", "sus4", "6", "m6", "7", "maj7", "m7", "mMaj7", "add9", "9", "m9", "11", "13", "7b5", "7#5", "7b9", "7#9"
};
```
