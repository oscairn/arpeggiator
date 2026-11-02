#!/usr/bin/env python3
"""Turn a Standard MIDI File into an Oscairn phrase (doc/phrase-format.md).

    midi_to_phrase.py clip.mid out.ophr [--name NAME] [--bars N] [--track N]
                      [--mode fixed|relative|chord] [--chord 60,64,67] [--root 60]

Modes, for every note of the clip:
  fixed     the note's own key, whatever is held (k)
  relative  the key moved by (lowest held key - root) (r)
  chord     a chord tone becomes a held-chord position with an octave (p); other notes become
            relative keys (r), so they follow the chord's lowest key. --chord names the chord
            the clip was played against (default: the notes sounding at its first tick).
Standard library only.
"""
import argparse
import struct
import sys


def read_vlq(data, i):
    v = 0
    while True:
        b = data[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i


def read_smf(data):
    """Return (ppq, tracks); each track is a list of (tick, status, data bytes)."""
    if data[:4] != b"MThd":
        raise ValueError("not a Standard MIDI File")
    hlen, fmt, ntrk, div = struct.unpack(">IHHH", data[4:14])
    if div & 0x8000:
        raise ValueError("SMPTE time division is not supported")
    i = 8 + hlen
    tracks = []
    while i + 8 <= len(data) and len(tracks) < ntrk:
        cid, clen = data[i:i + 4], struct.unpack(">I", data[i + 4:i + 8])[0]
        body, i = data[i + 8:i + 8 + clen], i + 8 + clen
        if cid != b"MTrk":
            continue
        events, j, tick, running = [], 0, 0, None
        while j < len(body):
            d, j = read_vlq(body, j)
            tick += d
            st = body[j]
            if st == 0xFF:
                kind, j = body[j + 1], j + 2
                n, j = read_vlq(body, j)
                if kind == 0x2F:
                    events.append((tick, 0xFF2F, b""))
                    break
                j += n
                continue
            if st in (0xF0, 0xF7):
                n, j = read_vlq(body, j + 1)
                j += n
                continue
            if st & 0x80:
                running, j = st, j + 1
            elif running is None:
                raise ValueError("running status without a status byte")
            size = 1 if running & 0xF0 in (0xC0, 0xD0) else 2
            events.append((tick, running, bytes(body[j:j + size])))
            j += size
        tracks.append(events)
    return ppq_of(div), tracks


def ppq_of(div):
    return div & 0x7FFF


def notes_of(events, ppq_in, ppq_out):
    """Pair note-ons with offs; returns (notes, other) in output ticks."""
    scale = ppq_out / ppq_in
    open_, notes, other = {}, [], []
    for tick, st, d in events:
        t = round(tick * scale)
        kind = st & 0xF0 if st < 0x100 else None
        if kind == 0x90 and d[1] > 0:
            open_.setdefault(d[0], []).append((t, d[1]))
        elif kind in (0x80, 0x90):
            if open_.get(d[0]):
                t0, v = open_[d[0]].pop(0)
                notes.append((t0, d[0], v, max(1, t - t0)))
        elif kind == 0xB0:
            other.append((t, "cc %d %d %d" % (t, d[0], d[1])))
        elif kind == 0xE0:
            other.append((t, "bend %d %d" % (t, (d[0] | d[1] << 7) - 8192)))
        elif kind == 0xD0:
            other.append((t, "pressure %d %d" % (t, d[0])))
    end = max([n[0] + n[3] for n in notes] + [t for t, _ in other] + [0])
    for k, lst in open_.items():  # notes never released end with the clip
        for t0, v in lst:
            notes.append((t0, k, v, max(1, end - t0)))
    notes.sort()
    return notes, other, end


def source_for(key, mode, chord, root):
    if mode == "fixed":
        return "k%d" % key
    if mode == "chord":
        classes = [c % 12 for c in chord]
        if key % 12 in classes:
            pos = classes.index(key % 12)
            octave = (key - chord[pos]) // 12
            return "p%d%+d" % (pos, octave) if octave else "p%d" % pos
    return "r%d" % key


def convert(data, name, bars=None, track=None, mode="chord", chord=None, root=None, ppq=480):
    ppq_in, tracks = read_smf(data)
    events = [e for i, t in enumerate(tracks) if track is None or i == track for e in t]
    events.sort(key=lambda e: e[0])
    notes, other, end = notes_of(events, ppq_in, ppq)
    if not notes and not other:
        raise ValueError("the clip has no notes or controllers")
    if mode == "chord" and not chord:
        first = notes[0][0] if notes else 0
        chord = sorted({k for t, k, v, g in notes if t == first})
    chord = sorted(chord or [])
    if root is None:
        root = chord[0] if chord else (notes[0][1] if notes else 60)
    bar = ppq * 4
    length = bars * bar if bars else max(bar, -(-end // bar) * bar)
    lines = ["oscairn-phrase 1", 'name "%s"' % name.replace('"', "'"), "ppq %d" % ppq,
             "length %d" % length, "meter 4 4", "track", "  select index", "  root %d" % root]
    body = [(t, "  note %d %s v%d g%d" % (t, source_for(k, mode, chord, root), v, g))
            for t, k, v, g in notes if t < length]
    body += [(t, "  " + s) for t, s in other if t < length]
    body.sort(key=lambda x: x[0])
    lines += [s for _, s in body]
    lines.append("end")
    return "\n".join(lines) + "\n"


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("midi")
    ap.add_argument("out")
    ap.add_argument("--name")
    ap.add_argument("--bars", type=int)
    ap.add_argument("--track", type=int)
    ap.add_argument("--mode", choices=("fixed", "relative", "chord"), default="chord")
    ap.add_argument("--chord", help="comma-separated keys the clip was played against")
    ap.add_argument("--root", type=int, help="reference key for relative notes")
    a = ap.parse_args(argv)
    chord = [int(x) for x in a.chord.split(",")] if a.chord else None
    with open(a.midi, "rb") as f:
        text = convert(f.read(), a.name or a.midi.rsplit("/", 1)[-1].rsplit(".", 1)[0], a.bars, a.track,
                       a.mode, chord, a.root)
    with open(a.out, "w") as f:
        f.write(text)


if __name__ == "__main__":
    main(sys.argv[1:])
