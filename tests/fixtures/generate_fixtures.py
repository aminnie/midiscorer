#!/usr/bin/env python3
"""Regenerate checked-in MIDI fixtures from tests/fixtures/fixture-specs.md."""

from pathlib import Path

import mido
from mido import Message, MetaMessage, MidiFile, MidiTrack

FIXTURES_DIR = Path(__file__).resolve().parent


def append_chord_block(track: MidiTrack, start_tick: int, notes: list[int], hold_ticks: int, time_cursor: int) -> int:
    delta = start_tick - time_cursor
    for index, note in enumerate(notes):
        track.append(Message("note_on", channel=0, note=note, velocity=90, time=delta if index == 0 else 0))

    for index, note in enumerate(notes):
        track.append(Message("note_off", channel=0, note=note, velocity=0, time=hold_ticks if index == 0 else 0))

    return start_tick + hold_ticks


def write_tempo_time_sig(path: Path) -> None:
    mid = MidiFile(type=1, ticks_per_beat=480)
    track = MidiTrack()
    mid.tracks.append(track)

    track.append(MetaMessage("set_tempo", tempo=500000, time=0))
    track.append(MetaMessage("time_signature", numerator=4, denominator=4, time=0))
    track.append(Message("note_on", channel=0, note=60, velocity=80, time=0))
    track.append(MetaMessage("time_signature", numerator=3, denominator=4, time=3840))
    track.append(MetaMessage("set_tempo", tempo=666666, time=1440))
    track.append(Message("note_off", channel=0, note=60, velocity=0, time=1920))
    track.append(Message("note_on", channel=0, note=62, velocity=80, time=0))
    track.append(Message("note_off", channel=0, note=62, velocity=0, time=2880))
    track.append(MetaMessage("end_of_track", time=0))

    mid.save(path)


def write_ties_syncopation(path: Path) -> None:
    mid = MidiFile(type=1, ticks_per_beat=480)
    track = MidiTrack()
    mid.tracks.append(track)

    track.append(MetaMessage("set_tempo", tempo=500000, time=0))
    track.append(MetaMessage("time_signature", numerator=4, denominator=4, time=0))
    track.append(Message("note_on", channel=0, note=60, velocity=90, time=1440))
    track.append(Message("note_off", channel=0, note=60, velocity=0, time=2400))
    track.append(Message("note_on", channel=0, note=64, velocity=80, time=240))
    track.append(Message("note_off", channel=0, note=64, velocity=0, time=480))
    track.append(MetaMessage("end_of_track", time=0))

    mid.save(path)


def write_syncopated_durations(path: Path) -> None:
    mid = MidiFile(type=1, ticks_per_beat=480)
    track = MidiTrack()
    mid.tracks.append(track)

    track.append(MetaMessage("set_tempo", tempo=500000, time=0))
    track.append(MetaMessage("time_signature", numerator=4, denominator=4, time=0))

    # Off-beat sixteenth at quarter 0.25, duration sixteenth.
    track.append(Message("note_on", channel=0, note=60, velocity=80, time=120))
    track.append(Message("note_off", channel=0, note=60, velocity=0, time=120))

    # Off-beat eighth at quarter 0.75, duration eighth.
    track.append(Message("note_on", channel=0, note=62, velocity=80, time=240))
    track.append(Message("note_off", channel=0, note=62, velocity=0, time=240))

    # Quarter at beat 2.0, duration quarter.
    track.append(Message("note_on", channel=0, note=64, velocity=80, time=720))
    track.append(Message("note_off", channel=0, note=64, velocity=0, time=480))

    # Half at beat 4.0, duration half.
    track.append(Message("note_on", channel=0, note=65, velocity=80, time=960))
    track.append(Message("note_off", channel=0, note=65, velocity=0, time=960))

    # Whole at beat 8.0, duration whole.
    track.append(Message("note_on", channel=0, note=67, velocity=80, time=1920))
    track.append(Message("note_off", channel=0, note=67, velocity=0, time=1920))
    track.append(MetaMessage("end_of_track", time=0))

    mid.save(path)


def write_altered_chords(path: Path) -> None:
    mid = MidiFile(type=1, ticks_per_beat=480)
    track = MidiTrack()
    mid.tracks.append(track)

    track.append(MetaMessage("set_tempo", tempo=500000, time=0))
    track.append(MetaMessage("time_signature", numerator=4, denominator=4, time=0))

    chord_blocks = [
        (0, [60, 64, 67, 71, 74]),
        (1920, [55, 59, 62, 65, 68]),
        (3840, [62, 66, 69, 72, 65]),
        (5760, [53, 57, 60, 63, 65]),
    ]

    time_cursor = 0
    for start_tick, notes in chord_blocks:
        time_cursor = append_chord_block(track, start_tick, notes, 960, time_cursor)

    track.append(MetaMessage("end_of_track", time=0))
    mid.save(path)


def main() -> None:
    write_tempo_time_sig(FIXTURES_DIR / "tempo_time_sig.mid")
    write_ties_syncopation(FIXTURES_DIR / "ties_syncopation.mid")
    write_syncopated_durations(FIXTURES_DIR / "syncopated_durations.mid")
    write_altered_chords(FIXTURES_DIR / "altered_chords.mid")
    print(f"Wrote fixtures to {FIXTURES_DIR}")


if __name__ == "__main__":
    main()
