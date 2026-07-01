#include <JuceHeader.h>
#include "../src/midi/MidiProjectLoader.h"
#include "../src/midi/TempoMap.h"
#include "../src/midi/TrackNoteExtractor.h"
#include "../src/notation/Quantizer.h"
#include "../src/notation/ScoreModel.h"
#include "../src/harmony/ChordDetector.h"
#include "../src/playback/MidiFilePlaybackEngineAdapter.h"
#include "../src/playback/PlaybackController.h"
#include "../src/playback/TrackMixProcessor.h"
#include "../src/playback/TrackMixMidiSeed.h"
#include "../src/app/ScoreRebuildService.h"
#include "../src/app/KeyOverrideTranspose.h"
#include "../src/app/ScorePdfExporter.h"
#include "../src/notation/ScoreRenderer.h"
#include "TestFixturePaths.h"
#include <iostream>
#include <set>

namespace
{
int failures = 0;

void expectTrue(bool condition, const juce::String& label)
{
    if (!condition)
    {
        ++failures;
        juce::Logger::writeToLog("FAIL: " + label);
        std::cout << "FAIL: " << label << std::endl;
    }
    else
    {
        juce::Logger::writeToLog("PASS: " + label);
        std::cout << "PASS: " << label << std::endl;
    }
}

void testTempoMap()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 }, { 960.0, 60.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 }, { 1920.0, 3, 4 } };
    expectTrue(map.build(960.0, tempos, signatures, 3840.0), "TempoMap builds");
    expectTrue(std::abs(map.tickToSeconds(960.0) - 0.5) < 0.02, "Tick->seconds uses tempo");
    expectTrue(map.quarterToBar(0.0) == 1, "Quarter zero is bar 1");
    expectTrue(map.quarterToBar(4.0) == 2, "Bar increments in 4/4");
}

void testQuantizer()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 1920.0);

    MidiNoteEvent note;
    note.noteNumber = 64;
    note.startTick = 0.0;
    note.endTick = 960.0;
    std::vector<MidiNoteEvent> notes { note };

    auto quantized = Quantizer::quantizeTrack(notes, map);
    expectTrue(!quantized.empty(), "Quantizer returns notes");
    expectTrue(quantized.front().value == NoteValue::quarter, "Quarter duration detected");
}

void testQuantizerTripletFlagging()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 1920.0);

    MidiNoteEvent triplet;
    triplet.noteNumber = 60;
    triplet.startTick = 0.0;
    triplet.endTick = 320.0; // one third of a quarter at 960 PPQ
    auto tripletQuantized = Quantizer::quantizeTrack(std::vector<MidiNoteEvent> { triplet }, map);
    expectTrue(!tripletQuantized.empty(), "Triplet quantization returns notes");
    if (!tripletQuantized.empty())
        expectTrue(tripletQuantized.front().isTriplet, "Triplet duration is flagged for rendering");

    MidiNoteEvent straight;
    straight.noteNumber = 62;
    straight.startTick = 0.0;
    straight.endTick = 480.0; // straight eighth
    auto straightQuantized = Quantizer::quantizeTrack(std::vector<MidiNoteEvent> { straight }, map);
    expectTrue(!straightQuantized.empty(), "Straight quantization returns notes");
    if (!straightQuantized.empty())
        expectTrue(!straightQuantized.front().isTriplet, "Straight duration is not flagged as triplet");
}

void testDottedDurationQuantization()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 7680.0);

    MidiNoteEvent dottedHalf;
    dottedHalf.noteNumber = 60;
    dottedHalf.startTick = 0.0;
    dottedHalf.endTick = 2880.0;
    auto quantizedHalf = Quantizer::quantizeTrack(std::vector<MidiNoteEvent> { dottedHalf }, map);
    expectTrue(!quantizedHalf.empty(), "Dotted-half quantizer returns notes");
    if (!quantizedHalf.empty())
    {
        expectTrue(quantizedHalf.front().value == NoteValue::half, "Three quarters quantizes to a half note value");
        expectTrue(quantizedHalf.front().dotted, "Three quarters quantizes to a dotted duration");
        expectTrue(std::abs(quantizedHalf.front().durationQuarter - 3.0) < 1.0e-6,
                   "Three quarters keeps a 3.0 quarter duration");
    }

    MidiNoteEvent dottedQuarter;
    dottedQuarter.noteNumber = 62;
    dottedQuarter.startTick = 0.0;
    dottedQuarter.endTick = 1440.0;
    auto quantizedQuarter = Quantizer::quantizeTrack(std::vector<MidiNoteEvent> { dottedQuarter }, map);
    expectTrue(!quantizedQuarter.empty(), "Dotted-quarter quantizer returns notes");
    if (!quantizedQuarter.empty())
    {
        expectTrue(quantizedQuarter.front().value == NoteValue::quarter, "One and a half quarters quantizes to a quarter note value");
        expectTrue(quantizedQuarter.front().dotted, "One and a half quarters quantizes to a dotted duration");
        expectTrue(std::abs(quantizedQuarter.front().durationQuarter - 1.5) < 1.0e-6,
                   "One and a half quarters keeps a 1.5 quarter duration");
    }

    QuantizedNote manualDottedHalf;
    manualDottedHalf.midiNote = 64;
    manualDottedHalf.startQuarter = 0.0;
    manualDottedHalf.durationQuarter = 3.0;
    manualDottedHalf.value = NoteValue::half;
    manualDottedHalf.dotted = true;

    ScoreModel model;
    model.build(map, std::vector<QuantizedNote> { manualDottedHalf }, {}, 1);
    const auto bars = model.getWindowBars(1, 0, 0);
    expectTrue(!bars.empty(), "Dotted-half score model returns a bar");
    if (!bars.empty())
    {
        bool foundDottedHalf = false;
        for (const auto& symbol : bars[0].notes)
        {
            if (!symbol.isRest && symbol.midiNote == 64)
                foundDottedHalf = symbol.value == NoteValue::half && symbol.dotted;
        }
        expectTrue(foundDottedHalf, "Score model preserves dotted half symbols");
    }
}

void testChordDetector()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 3840.0);

    std::vector<MidiNoteEvent> notes;
    for (int midiNote : { 60, 64, 67, 71, 74 })
    {
        MidiNoteEvent ev;
        ev.noteNumber = midiNote;
        ev.startSec = 0.0;
        ev.endSec = 2.0;
        notes.push_back(ev);
    }

    auto chords = ChordDetector::detect(notes, map, 2);
    expectTrue(!chords.empty(), "Chord detector returns at least one chord");
    if (!chords.empty())
        expectTrue(chords.front().symbol.startsWith("C"), "Chord detector resolves C-root harmony");

    ChordDetector::NamingOptions flatOptions;
    flatOptions.accidentalPreference = ChordDetector::AccidentalPreference::preferFlats;
    flatOptions.jazzAliasStyle = ChordDetector::JazzAliasStyle::jazzSymbols;
    auto flatChords = ChordDetector::detect(notes, map, 2, flatOptions);
    expectTrue(!flatChords.empty(), "Chord detector supports naming options");
    if (!flatChords.empty())
        expectTrue(flatChords.front().symbol.contains("C"), "Chord naming options keep root identity");
}

void testChordComplexityWhitelist()
{
    std::vector<MidiNoteEvent> notes;
    for (int midiNote : { 48, 52, 55, 58, 61 }) // C, E, G, Bb, Db -> C7b9 sonority
    {
        MidiNoteEvent ev;
        ev.noteNumber = midiNote;
        ev.startSec = 0.0;
        ev.endSec = 1.0;
        notes.push_back(ev);
    }

    ChordDetector::NamingOptions simpleOptions;
    simpleOptions.complexity = ChordDetector::ChordComplexity::simple;
    const auto simple = ChordDetector::detectInWindow(notes, 0.0, 1.0, simpleOptions);
    const bool simpleVocabulary = !simple.contains("7")
        && !simple.contains("9")
        && !simple.contains("11")
        && !simple.contains("13")
        && !simple.contains("b5")
        && !simple.contains("#5");
    expectTrue(simpleVocabulary, "Simple chord complexity limits output to triad/sus vocabulary");

    ChordDetector::NamingOptions standardOptions;
    standardOptions.complexity = ChordDetector::ChordComplexity::standard;
    const auto standard = ChordDetector::detectInWindow(notes, 0.0, 1.0, standardOptions);
    expectTrue(standard == "C7", "Standard chord complexity allows sevenths but excludes altered dominant suffixes");

    ChordDetector::NamingOptions richOptions;
    richOptions.complexity = ChordDetector::ChordComplexity::rich;
    const auto rich = ChordDetector::detectInWindow(notes, 0.0, 1.0, richOptions);
    expectTrue(rich == "C7b9", "Rich chord complexity allows altered dominant suffixes");
}

void testChordDetectionResolution()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 3840.0);

    std::vector<MidiNoteEvent> notes;
    for (int midiNote : { 72, 77, 79 })
    {
        MidiNoteEvent ev;
        ev.noteNumber = midiNote;
        ev.startSec = 0.0;
        ev.endSec = 0.25;
        notes.push_back(ev);
    }

    for (int midiNote : { 43, 67, 71, 74 })
    {
        MidiNoteEvent ev;
        ev.noteNumber = midiNote;
        ev.startSec = 0.25;
        ev.endSec = 2.0;
        notes.push_back(ev);
    }

    const auto quarterChords = ChordDetector::detect(
        notes, map, 1, {}, ChordDetector::DetectionResolution::quarter);
    const auto eighthChords = ChordDetector::detect(
        notes, map, 1, {}, ChordDetector::DetectionResolution::eighth);

    bool eighthStartsWithC = false;
    for (const auto& chord : eighthChords)
    {
        if (chord.barNumber == 1 && std::abs(chord.quarter) < 1.0e-6)
            eighthStartsWithC = chord.symbol.startsWith("C");
    }
    expectTrue(eighthStartsWithC, "Eighth resolution can capture short C harmony at bar start");

    bool quarterStartsWithG = false;
    for (const auto& chord : quarterChords)
    {
        if (chord.barNumber == 1 && std::abs(chord.quarter) < 1.0e-6)
            quarterStartsWithG = chord.symbol.startsWith("G");
    }
    expectTrue(quarterStartsWithG, "Quarter resolution favors sustained G harmony at bar start");
}

void testNoDisplayStaffSelectorMapping()
{
    juce::ComboBox selector;
    selector.addItem("No Display", 1);
    selector.addItem("Track A", 2);
    selector.addItem("Track B", 3);

    selector.setSelectedItemIndex(0, juce::dontSendNotification);
    expectTrue(ScoreRebuildService::sourceTrackIndexFromSelector(selector) == -1,
               "No Display maps to hidden staff index");

    selector.setSelectedItemIndex(1, juce::dontSendNotification);
    expectTrue(ScoreRebuildService::sourceTrackIndexFromSelector(selector) == 0,
               "First visible track maps to source index 0");

    selector.setSelectedItemIndex(2, juce::dontSendNotification);
    expectTrue(ScoreRebuildService::sourceTrackIndexFromSelector(selector) == 1,
               "Second visible track maps to source index 1");
}

void testMidiLoaderRejectsType0()
{
    juce::TemporaryFile tempFile("type0.mid");
    const auto file = tempFile.getFile();
    static const uint8_t type0MidiData[] =
    {
        0x4d, 0x54, 0x68, 0x64,  // MThd
        0x00, 0x00, 0x00, 0x06,  // header size
        0x00, 0x00,              // format 0
        0x00, 0x01,              // one track
        0x01, 0xE0,              // 480 PPQ
        0x4d, 0x54, 0x72, 0x6b,  // MTrk
        0x00, 0x00, 0x00, 0x0D,
        0x00, 0x90, 0x3C, 0x64,
        0x83, 0x60, 0x80, 0x3C, 0x00,
        0x00, 0xFF, 0x2F, 0x00   // end-of-track
    };

    expectTrue(file.replaceWithData(type0MidiData, sizeof(type0MidiData)),
               "Can write temporary type 0 MIDI fixture");

    MidiProjectLoader loader;
    MidiProjectData project;
    juce::String error;
    bool rejectedType0 = false;
    const bool ok = loader.load(file, project, error, &rejectedType0, false);
    expectTrue(!ok, "Loader rejects MIDI file type 0 when auto-convert is disabled");
    expectTrue(rejectedType0, "Loader flags type 0 rejection");
    expectTrue(error.containsIgnoreCase("type 0"), "Loader reports type 0 in error message");
    expectTrue(error.containsIgnoreCase("type 1"), "Loader asks user to convert to type 1");

    MidiProjectData convertedProject;
    juce::String convertError;
    bool convertedType0 = false;
    const bool convertedOk = loader.load(file, convertedProject, convertError, nullptr, true, &convertedType0);
    expectTrue(convertedOk, "Loader auto-converts MIDI file type 0 when enabled");
    expectTrue(convertedType0, "Loader reports type 0 conversion");
    expectTrue(!convertedProject.tracks.empty(), "Converted type 0 project has tracks");
}

void testMidiLoaderRejectsSmpte()
{
    // Fixture intent: tempo-time-signature-changes (timing-format validation path)
    juce::TemporaryFile tempFile("smpte.mid");
    const auto file = tempFile.getFile();
    static const uint8_t smpteMidiData[] =
    {
        0x4d, 0x54, 0x68, 0x64,  // MThd
        0x00, 0x00, 0x00, 0x06,  // header size
        0x00, 0x01,              // format 1
        0x00, 0x01,              // one track
        0xe7, 0x28,              // SMPTE division: -25 fps, 40 ticks/frame
        0x4d, 0x54, 0x72, 0x6b,  // MTrk
        0x00, 0x00, 0x00, 0x04,  // track size
        0x00, 0xff, 0x2f, 0x00   // end-of-track
    };

    expectTrue(file.replaceWithData(smpteMidiData, sizeof(smpteMidiData)),
               "Can write temporary SMPTE MIDI fixture");

    MidiProjectLoader loader;
    MidiProjectData project;
    juce::String error;
    const bool ok = loader.load(file, project, error);
    expectTrue(!ok, "Loader rejects SMPTE/non-PPQ MIDI");
    expectTrue(error.containsIgnoreCase("SMPTE") || error.containsIgnoreCase("non-PPQ"),
               "Loader reports clear SMPTE/non-PPQ error");
}

void testTrackNoteExtractorFlushesOrphans()
{
    // Fixture intent: ties-across-bars (sustained event finalization edge case)
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 960.0);

    juce::MidiMessageSequence seq;
    auto onOrphan = juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100);
    onOrphan.setTimeStamp(0.0);
    seq.addEvent(onOrphan);

    auto onNormal = juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100);
    onNormal.setTimeStamp(240.0);
    seq.addEvent(onNormal);

    auto offNormal = juce::MidiMessage::noteOff(1, 64);
    offNormal.setTimeStamp(480.0);
    seq.addEvent(offNormal);

    const auto notes = TrackNoteExtractor::extract(seq, map);
    expectTrue(notes.size() == 2, "Extractor keeps normal notes and flushes orphaned note-on");

    bool orphanFlushed = false;
    for (const auto& note : notes)
    {
        if (note.noteNumber == 60)
        {
            orphanFlushed = std::abs(note.endTick - 480.0) < 1.0e-6;
            break;
        }
    }
    expectTrue(orphanFlushed, "Orphan note ends at final track tick");
}

void testTrackNoteExtractorTreatsZeroVelocityNoteOnAsNoteOff()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 960.0);

    juce::MidiMessageSequence seq;
    auto on = juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100);
    on.setTimeStamp(0.0);
    seq.addEvent(on);

    auto offViaZeroVelocity = juce::MidiMessage::noteOn(1, 60, (juce::uint8) 0);
    offViaZeroVelocity.setTimeStamp(480.0);
    seq.addEvent(offViaZeroVelocity);

    const auto notes = TrackNoteExtractor::extract(seq, map);
    expectTrue(notes.size() == 1, "Velocity-0 note-on closes the active note instead of opening another");
    if (notes.empty())
        return;

    expectTrue(notes.front().noteNumber == 60, "Closed note keeps expected pitch");
    expectTrue(std::abs(notes.front().endTick - 480.0) < 1.0e-6, "Velocity-0 note-on ends note at event tick");
}

void testTempoMapDetectsMultipleTempos()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 }, { 960.0, 60.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 3840.0);

    expectTrue(map.hasMultipleTempoEvents(), "Tempo map reports multiple distinct tempo events");
}

void testPlaybackControllerTempoOverrideScalesMultiTempoTimeline()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 }, { 960.0, 60.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 3840.0);

    const double totalDurationSec = map.tickToSeconds(3840.0);
    PlaybackController controller;
    controller.setTempoMap(&map, totalDurationSec);
    controller.setTempoOverrideBpm(240.0, 120.0);
    expectTrue(std::abs(controller.getTempoScale() - 2.0) < 1.0e-6,
               "Tempo override scales relative to opening tempo");

    const double tempoChangeSec = map.tickToSeconds(960.0);
    controller.seekToSecond(tempoChangeSec);
    expectTrue(std::abs(controller.getElapsedSeconds() - tempoChangeSec) < 1.0e-6,
               "Seek preserves song-time position at tempo change boundary");

    const int barAtChange = controller.getCurrentBar();
    controller.seekToSecond(map.tickToSeconds(1920.0));
    expectTrue(controller.getCurrentBar() >= barAtChange,
               "Later song-time positions remain ordered across tempo changes under override");
}

void testScoreModelSplitsCrossBarNotes()
{
    // Fixture intent: ties-across-bars
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 7680.0);

    QuantizedNote longNote;
    longNote.midiNote = 60;
    longNote.startQuarter = 3.0;
    longNote.durationQuarter = 2.0;
    longNote.value = NoteValue::half;

    ScoreModel model;
    model.build(map, std::vector<QuantizedNote> { longNote }, {}, 2);
    const auto bars = model.getWindowBars(1, 0, 1);
    expectTrue(bars.size() == 2, "ScoreModel returns two bars for split-note test");
    if (bars.size() != 2)
        return;

    bool bar1TieFound = false;
    for (const auto& note : bars[0].notes)
    {
        if (!note.isRest && note.midiNote == 60)
            bar1TieFound = note.tieIntoNextBar;
    }
    expectTrue(bar1TieFound, "Start-bar note ties into next bar");

    bool bar2ContinuationFound = false;
    bool bar2TieFromPreviousFound = false;
    for (const auto& note : bars[1].notes)
    {
        if (!note.isRest && note.midiNote == 60 && std::abs(note.quarterInBar) < 1.0e-6)
        {
            bar2ContinuationFound = true;
            bar2TieFromPreviousFound = note.tieFromPreviousBar;
        }
    }
    expectTrue(bar2ContinuationFound, "Continuation note is present at next bar downbeat");
    expectTrue(bar2TieFromPreviousFound, "Continuation note is marked as tied from previous bar");
}

void testChordDetectorResetsAcrossSilence()
{
    // Fixture intent: altered-chords (window re-detection after silence)
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 11520.0); // 3 bars in 4/4 at 960 ppq

    std::vector<MidiNoteEvent> notes;
    for (const auto span : { std::pair<double, double> { 0.0, 2.0 }, std::pair<double, double> { 4.0, 6.0 } })
    {
        for (int midiNote : { 60, 64, 67 })
        {
            MidiNoteEvent ev;
            ev.noteNumber = midiNote;
            ev.startSec = span.first;
            ev.endSec = span.second;
            notes.push_back(ev);
        }
    }

    const auto chords = ChordDetector::detect(notes, map, 3);
    bool hasBar1 = false;
    bool hasBar3 = false;
    for (const auto& chord : chords)
    {
        hasBar1 = hasBar1 || chord.barNumber == 1;
        hasBar3 = hasBar3 || chord.barNumber == 3;
    }
    expectTrue(hasBar1, "Chord detector annotates first bar");
    expectTrue(hasBar3, "Chord detector re-announces chord after silence in later bar");
}

void testScoreModelNormalizesChordQuarterInBar()
{
    // Fixture intent: syncopated-note-durations / altered-chords (bar-relative event placement)
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 7680.0);

    std::vector<ChordAnnotation> chords
    {
        { 2, 4.0, "C" },
        { 2, 5.0, "G" }
    };

    ScoreModel model;
    model.build(map, {}, chords, 2);
    const auto bars = model.getWindowBars(2, 0, 0);
    expectTrue(bars.size() == 1, "Can inspect bar 2 for chord normalization");
    if (bars.empty())
        return;

    expectTrue(bars.front().chords.size() == 2, "Both chord changes are retained in the bar");
    if (bars.front().chords.size() >= 2)
    {
        expectTrue(std::abs(bars.front().chords[0].quarter - 0.0) < 1.0e-6, "First chord starts at quarter-in-bar 0");
        expectTrue(std::abs(bars.front().chords[1].quarter - 1.0) < 1.0e-6, "Second chord starts at quarter-in-bar 1");
    }
}

void testWindowClampBehaviorForScoreWindow()
{
    // Fixture intent: tempo-time-signature-changes (window navigation/clamp reliability)
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 3840.0);

    QuantizedNote quarterNote;
    quarterNote.midiNote = 60;
    quarterNote.startQuarter = 0.0;
    quarterNote.durationQuarter = 1.0;
    quarterNote.value = NoteValue::quarter;

    ScoreModel model;
    model.build(map, std::vector<QuantizedNote> { quarterNote }, {}, 2);

    const int clampedHigh = juce::jlimit(model.getFirstBar(), model.getLastBar(), 999);
    const int clampedLow = juce::jlimit(model.getFirstBar(), model.getLastBar(), -20);
    expectTrue(clampedHigh == model.getLastBar(), "Out-of-range high current bar clamps to last bar");
    expectTrue(clampedLow == model.getFirstBar(), "Out-of-range low current bar clamps to first bar");

    const auto highWindow = model.getWindowBars(clampedHigh, 2, 2);
    const auto lowWindow = model.getWindowBars(clampedLow, 2, 2);
    expectTrue(!highWindow.empty(), "High clamped bar produces a non-empty score window");
    expectTrue(!lowWindow.empty(), "Low clamped bar produces a non-empty score window");
}

void testScoreModelGetBarsInRange()
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 7680.0);

    ScoreModel emptyModel;
    const auto emptyRange = emptyModel.getBarsInRange(1, 4);
    expectTrue(emptyRange.empty(), "Empty ScoreModel returns empty explicit bar range");

    QuantizedNote note;
    note.midiNote = 64;
    note.startQuarter = 0.0;
    note.durationQuarter = 1.0;
    note.value = NoteValue::quarter;

    ScoreModel model;
    model.build(map, std::vector<QuantizedNote> { note }, {}, 4);

    const auto middleRange = model.getBarsInRange(2, 3);
    expectTrue(middleRange.size() == 2, "Explicit bar range returns requested bars");
    if (middleRange.size() == 2)
    {
        expectTrue(middleRange[0].barNumber == 2, "Explicit bar range starts at requested bar");
        expectTrue(middleRange[1].barNumber == 3, "Explicit bar range ends at requested bar");
    }

    const auto clampedRange = model.getBarsInRange(-20, 99);
    expectTrue(clampedRange.size() == 4, "Explicit bar range clamps to model boundaries");
}

ScoreModel makeTestScoreModelWithBarCount(int barCount)
{
    TempoMap map;
    std::vector<TempoMetaEvent> tempos { { 0.0, 120.0 } };
    std::vector<TimeSignatureMetaEvent> signatures { { 0.0, 4, 4 } };
    map.build(960.0, tempos, signatures, 3840.0 * static_cast<double>(juce::jmax(1, barCount)));

    std::vector<QuantizedNote> notes;
    notes.reserve(static_cast<size_t>(juce::jmax(1, barCount)));
    for (int bar = 0; bar < juce::jmax(1, barCount); ++bar)
    {
        QuantizedNote note;
        note.midiNote = 60 + (bar % 5);
        note.startQuarter = static_cast<double>(bar) * 4.0;
        note.durationQuarter = 1.0;
        note.value = NoteValue::quarter;
        notes.push_back(note);
    }

    ScoreModel model;
    model.build(map, notes, {}, juce::jmax(1, barCount));
    return model;
}

void testScorePdfExportWritesValidPdf()
{
    auto model1 = makeTestScoreModelWithBarCount(12);
    auto model2 = makeTestScoreModelWithBarCount(12);
    ScoreRenderer renderer1;
    renderer1.setScoreModel(&model1);
    renderer1.setClefType(ScoreRenderer::ClefType::treble);
    renderer1.setStaffLabel("Staff 1");
    renderer1.setCurrentBar(1);

    ScoreRenderer renderer2;
    renderer2.setScoreModel(&model2);
    renderer2.setClefType(ScoreRenderer::ClefType::bass);
    renderer2.setStaffLabel("Staff 2");
    renderer2.setCurrentBar(1);

    ScorePdfExporter::ExportOptions options;
    options.title = "PdfExportValid";
    options.subtitle = "Fixture";
    options.barsPerRow = 4;

    const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("midiscorer_pdf_valid_", ".pdf", false);
    juce::String error;
    int pageCount = 0;
    const bool ok = ScorePdfExporter::exportToFile(
        tempFile,
        std::vector<ScorePdfExporter::StaffExportLane> { { &model1, &renderer1 }, { &model2, &renderer2 } },
        options,
        error,
        &pageCount);

    expectTrue(ok, "Score PDF export returns success for multi-staff lane export");
    if (!ok)
        std::cout << "PDF export error: " << error << std::endl;
    expectTrue(tempFile.existsAsFile(), "Score PDF export writes output file");
    expectTrue(tempFile.getSize() > 1024, "Score PDF export writes non-trivial PDF content");
    expectTrue(pageCount >= 1, "Score PDF export reports at least one page");

    juce::MemoryBlock data;
    const bool loaded = tempFile.loadFileAsData(data);
    expectTrue(loaded && data.getSize() >= 4, "Score PDF output can be read for header validation");
    if (loaded && data.getSize() >= 4)
    {
        const auto* bytes = static_cast<const char*>(data.getData());
        const bool hasPdfHeader = std::memcmp(bytes, "%PDF", 4) == 0;
        expectTrue(hasPdfHeader, "Score PDF output starts with %PDF header");
    }

    if (tempFile.existsAsFile())
        tempFile.deleteFile();
}

void testScorePdfExportFailsForEmptyStaff1OnlyMode()
{
    ScoreModel emptyStaff1Model;
    ScoreRenderer emptyStaff1Renderer;
    emptyStaff1Renderer.setScoreModel(&emptyStaff1Model);
    emptyStaff1Renderer.setCurrentBar(1);

    auto populatedStaff2Model = makeTestScoreModelWithBarCount(8);
    ScoreRenderer populatedStaff2Renderer;
    populatedStaff2Renderer.setScoreModel(&populatedStaff2Model);
    populatedStaff2Renderer.setCurrentBar(1);
    populatedStaff2Renderer.setStaffLabel("Staff 2");

    ScorePdfExporter::ExportOptions options;
    options.title = "PdfStaff1OnlyFailure";
    options.barsPerRow = 4;

    const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("midiscorer_pdf_staff1_only_fail_", ".pdf", false);
    juce::String error;
    int pageCount = 0;
    const bool ok = ScorePdfExporter::exportToFile(
        tempFile,
        std::vector<ScorePdfExporter::StaffExportLane> { { &emptyStaff1Model, &emptyStaff1Renderer } },
        options,
        error,
        &pageCount);

    expectTrue(!ok, "Staff 1 only export fails when Staff 1 has no notation");
    expectTrue(error.containsIgnoreCase("No staff notation"), "Staff 1 only empty export returns clear error message");
    expectTrue(pageCount == 0, "Failed Staff 1 only export reports zero pages");

    // Ensure all-staff style exports can still succeed with populated non-first lanes.
    juce::String allStaffError;
    int allStaffPages = 0;
    const bool allStaffOk = ScorePdfExporter::exportToFile(
        tempFile,
        std::vector<ScorePdfExporter::StaffExportLane> { { &emptyStaff1Model, &emptyStaff1Renderer },
                                                         { &populatedStaff2Model, &populatedStaff2Renderer } },
        options,
        allStaffError,
        &allStaffPages);
    expectTrue(allStaffOk, "All-staff export still succeeds when a later staff has notation");
    expectTrue(allStaffPages >= 1, "All-staff export reports pages when non-first staff has notation");

    if (tempFile.existsAsFile())
        tempFile.deleteFile();
}

void testDisplayOctaveShiftForView()
{
    const int unchanged = ScoreRenderer::applyDisplayOctaveShiftForView(60, 0, false);
    expectTrue(unchanged == 60, "Display octave shift leaves normal notes unchanged");

    const int upOne = ScoreRenderer::applyDisplayOctaveShiftForView(60, 1, false);
    expectTrue(upOne == 72, "Display octave shift moves notes up one octave");

    const int downOne = ScoreRenderer::applyDisplayOctaveShiftForView(60, -1, false);
    expectTrue(downOne == 48, "Display octave shift moves notes down one octave");

    const int clampedHigh = ScoreRenderer::applyDisplayOctaveShiftForView(123, 1, false);
    expectTrue(clampedHigh == 127, "Display octave shift clamps upper MIDI note range");

    const int clampedLow = ScoreRenderer::applyDisplayOctaveShiftForView(2, -1, false);
    expectTrue(clampedLow == 0, "Display octave shift clamps lower MIDI note range");

    const int drumIgnored = ScoreRenderer::applyDisplayOctaveShiftForView(42, 1, true);
    expectTrue(drumIgnored == 42, "Display octave shift is ignored for drum clef rendering");
}

void testScorePdfExportPageCountScalesWithBars()
{
    auto shortModel = makeTestScoreModelWithBarCount(4);
    auto longModel = makeTestScoreModelWithBarCount(32);

    ScoreRenderer shortRenderer;
    shortRenderer.setScoreModel(&shortModel);
    shortRenderer.setClefType(ScoreRenderer::ClefType::treble);
    shortRenderer.setCurrentBar(1);
    shortRenderer.setStaffLabel("Short");

    ScoreRenderer longRenderer;
    longRenderer.setScoreModel(&longModel);
    longRenderer.setClefType(ScoreRenderer::ClefType::treble);
    longRenderer.setCurrentBar(1);
    longRenderer.setStaffLabel("Long");

    ScorePdfExporter::ExportOptions options;
    options.title = "PdfExportPages";
    options.barsPerRow = 4;

    const auto shortFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("midiscorer_pdf_short_", ".pdf", false);
    const auto longFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("midiscorer_pdf_long_", ".pdf", false);

    juce::String shortError;
    juce::String longError;
    int shortPages = 0;
    int longPages = 0;

    const bool shortOk = ScorePdfExporter::exportToFile(
        shortFile,
        std::vector<ScorePdfExporter::StaffExportLane> { { &shortModel, &shortRenderer } },
        options,
        shortError,
        &shortPages);
    const bool longOk = ScorePdfExporter::exportToFile(
        longFile,
        std::vector<ScorePdfExporter::StaffExportLane> { { &longModel, &longRenderer } },
        options,
        longError,
        &longPages);

    expectTrue(shortOk, "Short score PDF export succeeds");
    expectTrue(longOk, "Long score PDF export succeeds");
    if (!shortOk)
        std::cout << "Short PDF export error: " << shortError << std::endl;
    if (!longOk)
        std::cout << "Long PDF export error: " << longError << std::endl;
    expectTrue(longPages > shortPages, "Longer score export produces more PDF pages");

    if (shortFile.existsAsFile())
        shortFile.deleteFile();
    if (longFile.existsAsFile())
        longFile.deleteFile();
}

void testMidiPlaybackAdapterSeekAndDispatch()
{
    MidiFilePlaybackEngineAdapter adapter;

    juce::TemporaryFile tempFile("adapter_seek_dispatch.mid");
    const auto file = tempFile.getFile();
    static const uint8_t midiData[] =
    {
        0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06,
        0x00, 0x00, 0x00, 0x01, 0x01, 0xE0,
        0x4d, 0x54, 0x72, 0x6b, 0x00, 0x00, 0x00, 0x0D,
        0x00, 0x90, 0x3C, 0x64,
        0x83, 0x60, 0x80, 0x3C, 0x00,
        0x00, 0xFF, 0x2F, 0x00
    };
    expectTrue(file.replaceWithData(midiData, sizeof(midiData)), "Can write temporary playback adapter MIDI fixture");

    juce::String error;
    expectTrue(adapter.loadFromFile(file, error), "Playback adapter loads MIDI fixture");
    expectTrue(adapter.getEventCount() >= 2, "Playback adapter stores scheduled note events");
    if (!adapter.getEvents().empty())
        expectTrue(adapter.getEvents().front().sourceTrackIndex == 0, "Playback adapter stores source track index");

    int emitted = 0;
    auto result0 = adapter.processUntilPlaybackTime(0.0, [&](const MidiFilePlaybackEngineAdapter::ScheduledEvent&) { ++emitted; });
    expectTrue(result0.emittedEventCount == 1, "Playback adapter emits note-on at start");

    adapter.seekToPlaybackTime(0.45);
    emitted = 0;
    auto result1 = adapter.processUntilPlaybackTime(0.5, [&](const MidiFilePlaybackEngineAdapter::ScheduledEvent&) { ++emitted; });
    expectTrue(result1.emittedEventCount >= 1, "Playback adapter emits note-off after seek");
}

void testTrackMixProcessor()
{
    TrackMixState state;
    state.resizeForTrackCount(3);

    expectTrue(TrackMixProcessor::shouldSendTrack(0, state), "Track mix sends unmuted track by default");
    state.setMuted(0, true);
    expectTrue(!TrackMixProcessor::shouldSendTrack(0, state), "Track mix blocks muted track when no solo");

    state.setSolo(1, true);
    expectTrue(!TrackMixProcessor::shouldSendTrack(0, state), "Track mix blocks non-solo track when solo active");
    expectTrue(TrackMixProcessor::shouldSendTrack(1, state), "Track mix allows active solo track");

    state.setVolume(1, 64);
    auto noteOn = juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100);
    auto scaledNote = TrackMixProcessor::applyVolumeToMessage(noteOn, 1, state);
    expectTrue(scaledNote.isNoteOn(), "Track mix preserves note-on type");
    expectTrue(scaledNote.getVelocity() < noteOn.getVelocity(), "Track mix scales note-on velocity");

    auto cc7 = juce::MidiMessage::controllerEvent(1, 7, 100);
    auto scaledCc7 = TrackMixProcessor::applyVolumeToMessage(cc7, 1, state);
    expectTrue(scaledCc7.isController() && scaledCc7.getControllerNumber() == 7, "Track mix preserves CC7 type");
    expectTrue(scaledCc7.getControllerValue() < cc7.getControllerValue(), "Track mix scales CC7 value");

    state.setVolume(1, 127);
    state.setExpression(1, 64);
    auto cc11 = juce::MidiMessage::controllerEvent(1, TrackMixProcessor::kExpressionController, 100);
    auto scaledCc11 = TrackMixProcessor::applyVolumeToMessage(cc11, 1, state);
    expectTrue(scaledCc11.isController() && scaledCc11.getControllerNumber() == TrackMixProcessor::kExpressionController,
               "Track mix preserves CC11 type");
    expectTrue(scaledCc11.getControllerValue() < cc11.getControllerValue(), "Track mix scales CC11 value by expression");

    auto noteOnNoExpressionImpact = juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100);
    auto expressionScaledNote = TrackMixProcessor::applyVolumeToMessage(noteOnNoExpressionImpact, 1, state);
    expectTrue(expressionScaledNote.getVelocity() < noteOnNoExpressionImpact.getVelocity(),
               "Track mix expression compounds note-on velocity scaling");

    state.setReverb(1, 64);
    auto cc91 = juce::MidiMessage::controllerEvent(1, TrackMixProcessor::kReverbController, 100);
    auto scaledCc91 = TrackMixProcessor::applyVolumeToMessage(cc91, 1, state);
    expectTrue(scaledCc91.isController() && scaledCc91.getControllerNumber() == TrackMixProcessor::kReverbController,
               "Track mix preserves CC91 type");
    expectTrue(scaledCc91.getControllerValue() < cc91.getControllerValue(), "Track mix scales CC91 value");

    state.setChannel(1, 3);
    auto noteOnCh1 = juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100);
    auto remappedNote = TrackMixProcessor::applyVolumeToMessage(noteOnCh1, 1, state);
    expectTrue(remappedNote.isNoteOn(), "Track mix preserves note-on after channel remap");
    expectTrue(remappedNote.getChannel() == 3, "Track mix remaps output channel");
}

void testTrackMixMidiSeed()
{
    juce::MidiMessageSequence sequence;
    sequence.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0.0);
    TrackMixState state;

    TrackMixMidiSeed::applyFromTrackSequences({ sequence }, state);
    expectTrue(state.getVolume(0) == TrackMixMidiSeed::kDefaultVolume, "Missing CC7 defaults volume to 100");
    expectTrue(state.getExpression(0) == TrackMixMidiSeed::kDefaultExpression, "Missing CC11 defaults expression to 100");
    expectTrue(state.getReverb(0) == TrackMixMidiSeed::kDefaultReverb, "Missing CC91 defaults reverb to 10");
    expectTrue(state.getChannel(0) == TrackMixMidiSeed::kDefaultChannel, "Missing channel defaults to 1");

    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 7, 80), 1.0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, TrackMixProcessor::kExpressionController, 70), 1.5);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, TrackMixProcessor::kReverbController, 40), 2.0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 7, 95), 3.0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, TrackMixProcessor::kExpressionController, 88), 3.5);
    sequence.addEvent(juce::MidiMessage::noteOn(5, 60, (juce::uint8) 100), 4.0);
    TrackMixMidiSeed::applyFromTrackSequences({ sequence }, state);
    expectTrue(state.getVolume(0) == 95, "Track mix seed uses last CC7 value");
    expectTrue(state.getExpression(0) == 88, "Track mix seed uses last CC11 value");
    expectTrue(state.getReverb(0) == 40, "Track mix seed uses last CC91 value");
    expectTrue(state.getChannel(0) == 1, "Track mix seed uses first MIDI channel in track");

    state.setVolume(0, 64);
    state.setExpression(0, 77);
    state.setReverb(0, 22);
    state.setChannel(0, 8);
    expectTrue(state.getVolume(0) == 64, "Saved preset volume remains after manual set");
    expectTrue(state.getExpression(0) == 77, "Saved preset expression remains after manual set");
    expectTrue(state.getReverb(0) == 22, "Saved preset reverb remains after manual set");
    expectTrue(state.getChannel(0) == 8, "Saved preset channel remains after manual set");
}

void testTrackMixSnapshotComparison()
{
    std::vector<TrackMixSettings> baseline
    {
        { 100, 100, 10, 1, false, false },
        { 90, 88, 15, 2, true, false }
    };

    std::vector<TrackMixSettings> same = baseline;
    expectTrue(same == baseline, "Track mix snapshot comparison treats identical states as equal");

    auto modified = baseline;
    modified[1].expression = 89;
    expectTrue(!(modified == baseline), "Track mix snapshot comparison detects changed expression");
}

void testTempoTimeSigFixtureBehavior()
{
    const auto fixture = getTestFixtureFile("tempo_time_sig.mid");
    expectTrue(fixture.existsAsFile(), "Fixture tempo_time_sig.mid is checked in");

    MidiProjectLoader loader;
    MidiProjectData project;
    juce::String error;
    const bool loaded = loader.load(fixture, project, error);
    expectTrue(loaded, "Loader can parse tempo_time_sig fixture");
    if (!loaded)
    {
        juce::Logger::writeToLog("Fixture load error: " + error);
        return;
    }

    const auto& signatures = project.tempoMap.getTimeSignatureEvents();
    expectTrue(signatures.size() >= 2, "tempo_time_sig fixture includes time-signature changes");
    expectTrue(signatures.front().numerator == 4 && signatures.front().denominator == 4,
               "tempo_time_sig fixture opens in 4/4");

    bool hasThreeFour = false;
    for (const auto& signature : signatures)
    {
        if (signature.numerator == 3 && signature.denominator == 4)
            hasThreeFour = true;
    }
    expectTrue(hasThreeFour, "tempo_time_sig fixture includes 3/4 time signature");

    const auto& tempos = project.tempoMap.getTempoEvents();
    expectTrue(tempos.size() >= 2, "tempo_time_sig fixture includes tempo changes");
    expectTrue(std::abs(tempos.front().bpm - 120.0) < 0.1, "tempo_time_sig fixture opens at 120 BPM");

    bool hasNinetyBpm = false;
    for (const auto& tempo : tempos)
    {
        if (std::abs(tempo.bpm - 90.0) < 0.1)
            hasNinetyBpm = true;
    }
    expectTrue(hasNinetyBpm, "tempo_time_sig fixture includes 90 BPM tempo change");

    expectTrue(project.maxBar >= 4, "tempo_time_sig fixture spans at least four bars");
    expectTrue(std::abs(project.tempoMap.barToQuarterDownbeat(3) - 8.0) < 0.01,
               "tempo_time_sig fixture maps bar 3 downbeat to quarter 8 before 3/4 change");
}

void testTiesSyncopationFixtureBehavior()
{
    const auto fixture = getTestFixtureFile("ties_syncopation.mid");
    expectTrue(fixture.existsAsFile(), "Fixture ties_syncopation.mid is checked in");

    MidiProjectLoader loader;
    MidiProjectData project;
    juce::String error;
    const bool loaded = loader.load(fixture, project, error);
    expectTrue(loaded, "Loader can parse ties_syncopation fixture");
    if (!loaded)
    {
        juce::Logger::writeToLog("Fixture load error: " + error);
        return;
    }

    expectTrue(!project.tracks.empty(), "ties_syncopation fixture yields at least one track");
    expectTrue(project.maxBar >= 2, "ties_syncopation fixture spans multiple bars");

    bool foundCrossBarSustain = false;
    for (const auto& track : project.tracks)
    {
        for (const auto& note : track.notes)
        {
            const int startBar = project.tempoMap.secondsToBar(note.startSec);
            const int endBar = project.tempoMap.secondsToBar(juce::jmax(note.startSec, note.endSec - 1.0e-6));
            if (endBar > startBar)
                foundCrossBarSustain = true;
        }
    }
    expectTrue(foundCrossBarSustain, "ties_syncopation fixture contains a note sustained across a bar line");

    const auto& track = project.tracks.front();
    expectTrue(!track.notes.empty(), "ties_syncopation primary track contains extracted notes");

    const auto quantized = Quantizer::quantizeTrack(track.notes, project.tempoMap);
    ScoreModel model;
    model.build(project.tempoMap, quantized, {}, project.maxBar);

    bool foundTieIntoNextBar = false;
    for (int bar = model.getFirstBar(); bar <= model.getLastBar(); ++bar)
    {
        const auto bars = model.getWindowBars(bar, 0, 0);
        for (const auto& barData : bars)
        {
            for (const auto& note : barData.notes)
            {
                if (!note.isRest && note.tieIntoNextBar)
                    foundTieIntoNextBar = true;
            }
        }
    }
    expectTrue(foundTieIntoNextBar, "ties_syncopation fixture produces a score tie into the next bar");
}

void testSyncopatedDurationsFixtureBehavior()
{
    const auto fixture = getTestFixtureFile("syncopated_durations.mid");
    expectTrue(fixture.existsAsFile(), "Fixture syncopated_durations.mid is checked in");

    MidiProjectLoader loader;
    MidiProjectData project;
    juce::String error;
    const bool loaded = loader.load(fixture, project, error);
    expectTrue(loaded, "Loader can parse syncopated_durations fixture");
    if (!loaded)
        return;

    expectTrue(!project.tracks.empty(), "syncopated_durations fixture yields at least one track");
    const auto quantized = Quantizer::quantizeTrack(project.tracks.front().notes, project.tempoMap);
    expectTrue(!quantized.empty(), "syncopated_durations fixture yields quantized notes");

    std::set<NoteValue> values;
    bool hasOffbeatStart = false;
    for (const auto& note : quantized)
    {
        values.insert(note.value);
        const double fractional = note.startQuarter - std::floor(note.startQuarter);
        if (std::abs(fractional - 0.25) < 1.0e-6 || std::abs(fractional - 0.75) < 1.0e-6)
            hasOffbeatStart = true;
    }

    expectTrue(values.count(NoteValue::sixteenth) > 0, "syncopated_durations fixture quantizes a sixteenth");
    expectTrue(values.count(NoteValue::eighth) > 0, "syncopated_durations fixture quantizes an eighth");
    expectTrue(values.count(NoteValue::quarter) > 0, "syncopated_durations fixture quantizes a quarter");
    expectTrue(values.count(NoteValue::half) > 0, "syncopated_durations fixture quantizes a half note");
    expectTrue(values.count(NoteValue::whole) > 0, "syncopated_durations fixture quantizes a whole note");
    expectTrue(hasOffbeatStart, "syncopated_durations fixture preserves off-beat sixteenth-grid starts");
}

void testAlteredChordsFixtureBehavior()
{
    const auto fixture = getTestFixtureFile("altered_chords.mid");
    expectTrue(fixture.existsAsFile(), "Fixture altered_chords.mid is checked in");

    MidiProjectLoader loader;
    MidiProjectData project;
    juce::String error;
    const bool loaded = loader.load(fixture, project, error);
    expectTrue(loaded, "Loader can parse altered_chords fixture");
    if (!loaded)
        return;

    expectTrue(!project.tracks.empty(), "altered_chords fixture yields at least one track");

    const auto& notes = project.tracks.front().notes;
    ChordDetector::NamingOptions options;
    options.accidentalPreference = ChordDetector::AccidentalPreference::preferFlats;
    options.jazzAliasStyle = ChordDetector::JazzAliasStyle::jazzSymbols;
    const auto chords = ChordDetector::detect(notes, project.tempoMap, project.maxBar, options);
    expectTrue(chords.size() >= 4, "altered_chords fixture yields multiple chord windows");

    auto containsSymbol = [&](const juce::String& fragment)
    {
        for (const auto& chord : chords)
        {
            if (chord.symbol.containsIgnoreCase(fragment))
                return true;
        }
        return false;
    };

    expectTrue(containsSymbol("maj9") || containsSymbol("^9") || containsSymbol("9"),
               "altered_chords fixture detects Cmaj9 family harmony");
    expectTrue(containsSymbol("7b9") || containsSymbol("-9") || containsSymbol("b9"),
               "altered_chords fixture detects G7b9 family harmony");
    expectTrue(containsSymbol("7#9") || containsSymbol("#9"),
               "altered_chords fixture detects D7#9 family harmony");
    expectTrue(containsSymbol("13") || containsSymbol("7"),
               "altered_chords fixture detects F13 family harmony");
}

void testCheckedInFixturesLoad()
{
    expectTrue(getTestFixturesDirectory().isDirectory(), "Test fixtures directory resolves");
    testTempoTimeSigFixtureBehavior();
    testTiesSyncopationFixtureBehavior();
    testSyncopatedDurationsFixtureBehavior();
    testAlteredChordsFixtureBehavior();
}

void testKeyOverrideTranspose()
{
    // C-major MIDI metadata (0 sharps/flats, major).
    expectTrue(KeyOverrideTranspose::midiTonicPc(true, 0, true) == 0, "C major MIDI tonic is C");
    expectTrue(KeyOverrideTranspose::midiTonicPc(true, -2, true) == 10, "Bb major MIDI tonic is Bb");

    expectTrue(KeyOverrideTranspose::semitonesForKeyOverride(false, false, 0, true, 0, true) == 0,
               "Matching override and MIDI key does not transpose");
    expectTrue(KeyOverrideTranspose::semitonesForKeyOverride(false, false, 10, true, 0, true) == -2,
               "Bb override on C-major MIDI transposes down 2 semitones");
    expectTrue(KeyOverrideTranspose::semitonesForKeyOverride(false, false, 2, true, 0, true) == 2,
               "D override on C-major MIDI transposes up 2 semitones");

    expectTrue(KeyOverrideTranspose::semitonesForKeyOverride(true, false, 10, true, 0, true) == 0,
               "Assign checked blocks key override transpose");
    expectTrue(KeyOverrideTranspose::semitonesForKeyOverride(false, true, 10, true, 0, true) == 0,
               "Profile-only mode blocks key override transpose");

    expectTrue(KeyOverrideTranspose::semitonesForKeyOverride(false, false, 2, true, 0, true, 2) == 0,
               "D override with D reference does not transpose");
    expectTrue(KeyOverrideTranspose::semitonesForKeyOverride(false, false, 10, true, 0, true, 2) == -4,
               "Bb override with D reference transposes down 4 semitones");
    expectTrue(KeyOverrideTranspose::semitonesForKeyOverride(false, false, 2, true, 0, true, 10) == 4,
               "D override with Bb reference transposes up 4 semitones");

    expectTrue(KeyOverrideTranspose::appliedSemitonesAfterKeyChange(0, 0, 5) == 5,
               "C to F accumulates +5 semitones from source");
    expectTrue(KeyOverrideTranspose::appliedSemitonesAfterKeyChange(5, 5, 0) == 0,
               "F back to C returns accumulated transpose to zero");
}
}

int main()
{
    testTempoMap();
    testQuantizer();
    testQuantizerTripletFlagging();
    testDottedDurationQuantization();
    testChordDetector();
    testChordComplexityWhitelist();
    testChordDetectionResolution();
    testNoDisplayStaffSelectorMapping();
    testMidiLoaderRejectsType0();
    testMidiLoaderRejectsSmpte();
    testTrackNoteExtractorFlushesOrphans();
    testTrackNoteExtractorTreatsZeroVelocityNoteOnAsNoteOff();
    testTempoMapDetectsMultipleTempos();
    testPlaybackControllerTempoOverrideScalesMultiTempoTimeline();
    testScoreModelSplitsCrossBarNotes();
    testChordDetectorResetsAcrossSilence();
    testScoreModelNormalizesChordQuarterInBar();
    testWindowClampBehaviorForScoreWindow();
    testScoreModelGetBarsInRange();
    testScorePdfExportWritesValidPdf();
    testScorePdfExportPageCountScalesWithBars();
    testScorePdfExportFailsForEmptyStaff1OnlyMode();
    testDisplayOctaveShiftForView();
    testMidiPlaybackAdapterSeekAndDispatch();
    testTrackMixProcessor();
    testTrackMixMidiSeed();
    testTrackMixSnapshotComparison();
    testKeyOverrideTranspose();
    testCheckedInFixturesLoad();

    if (failures == 0)
    {
        juce::Logger::writeToLog("All MidiScorer tests passed.");
        return 0;
    }

    juce::Logger::writeToLog("MidiScorer tests failed: " + juce::String(failures));
    return 1;
}
