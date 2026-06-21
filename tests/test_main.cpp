#include <JuceHeader.h>
#include "../src/midi/MidiProjectLoader.h"
#include "../src/midi/TempoMap.h"
#include "../src/midi/TrackNoteExtractor.h"
#include "../src/notation/Quantizer.h"
#include "../src/notation/ScoreModel.h"
#include "../src/harmony/ChordDetector.h"
#include "../src/playback/MidiFilePlaybackEngineAdapter.h"
#include "../src/playback/TrackMixProcessor.h"
#include "../src/playback/TrackMixMidiSeed.h"

namespace
{
int failures = 0;

void expectTrue(bool condition, const juce::String& label)
{
    if (!condition)
    {
        ++failures;
        juce::Logger::writeToLog("FAIL: " + label);
    }
    else
    {
        juce::Logger::writeToLog("PASS: " + label);
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

void testMidiLoaderRejectsSmpte()
{
    // Fixture intent: tempo-time-signature-changes (timing-format validation path)
    juce::TemporaryFile tempFile("smpte.mid");
    const auto file = tempFile.getFile();
    static const uint8_t smpteMidiData[] =
    {
        0x4d, 0x54, 0x68, 0x64,  // MThd
        0x00, 0x00, 0x00, 0x06,  // header size
        0x00, 0x00,              // format 0
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
    for (const auto& note : bars[1].notes)
    {
        if (!note.isRest && note.midiNote == 60 && std::abs(note.quarterInBar) < 1.0e-6)
            bar2ContinuationFound = true;
    }
    expectTrue(bar2ContinuationFound, "Continuation note is present at next bar downbeat");
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

    state.setReverb(1, 64);
    auto cc91 = juce::MidiMessage::controllerEvent(1, TrackMixProcessor::kReverbController, 100);
    auto scaledCc91 = TrackMixProcessor::applyVolumeToMessage(cc91, 1, state);
    expectTrue(scaledCc91.isController() && scaledCc91.getControllerNumber() == TrackMixProcessor::kReverbController,
               "Track mix preserves CC91 type");
    expectTrue(scaledCc91.getControllerValue() < cc91.getControllerValue(), "Track mix scales CC91 value");
}

void testTrackMixMidiSeed()
{
    juce::MidiMessageSequence sequence;
    sequence.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0.0);
    TrackMixState state;

    TrackMixMidiSeed::applyFromTrackSequences({ sequence }, state);
    expectTrue(state.getVolume(0) == TrackMixMidiSeed::kDefaultVolume, "Missing CC7 defaults volume to 100");
    expectTrue(state.getReverb(0) == TrackMixMidiSeed::kDefaultReverb, "Missing CC91 defaults reverb to 10");

    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 7, 80), 1.0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, TrackMixProcessor::kReverbController, 40), 2.0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 7, 95), 3.0);
    TrackMixMidiSeed::applyFromTrackSequences({ sequence }, state);
    expectTrue(state.getVolume(0) == 95, "Track mix seed uses last CC7 value");
    expectTrue(state.getReverb(0) == 40, "Track mix seed uses last CC91 value");

    state.setVolume(0, 64);
    state.setReverb(0, 22);
    expectTrue(state.getVolume(0) == 64, "Saved preset volume remains after manual set");
    expectTrue(state.getReverb(0) == 22, "Saved preset reverb remains after manual set");
}
}

int main()
{
    testTempoMap();
    testQuantizer();
    testChordDetector();
    testMidiLoaderRejectsSmpte();
    testTrackNoteExtractorFlushesOrphans();
    testScoreModelSplitsCrossBarNotes();
    testChordDetectorResetsAcrossSilence();
    testScoreModelNormalizesChordQuarterInBar();
    testWindowClampBehaviorForScoreWindow();
    testMidiPlaybackAdapterSeekAndDispatch();
    testTrackMixProcessor();
    testTrackMixMidiSeed();

    if (failures == 0)
    {
        juce::Logger::writeToLog("All MidiScorer tests passed.");
        return 0;
    }

    juce::Logger::writeToLog("MidiScorer tests failed: " + juce::String(failures));
    return 1;
}
