#!/usr/bin/env python3
"""Checks midi_to_phrase on SMF files built here."""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import midi_to_phrase as m  # noqa: E402


def vlq(v):
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.insert(0, 0x80 | (v & 0x7F))
        v >>= 7
    return bytes(out)


def smf(events, ppq=96):
    """events: (tick, bytes) in order."""
    body, last = b"", 0
    for t, e in events:
        body += vlq(t - last) + e
        last = t
    body += vlq(0) + b"\xff\x2f\x00"
    return b"MThd" + struct.pack(">IHHH", 6, 0, 1, ppq) + b"MTrk" + struct.pack(">I", len(body)) + body


def check(cond, what):
    if not cond:
        print("FAIL:", what)
        sys.exit(1)


# C major triad held at tick 0 against which E5 and F4 are played.
clip = smf([(0, b"\x90\x3c\x64"), (0, b"\x90\x40\x64"), (0, b"\x90\x43\x64"),
            (48, b"\x80\x3c\x00"), (48, b"\x80\x40\x00"), (48, b"\x80\x43\x00"),
            (96, b"\x90\x4c\x50"), (120, b"\x80\x4c\x00"), (144, b"\x90\x41\x50"), (168, b"\x90\x41\x00"),
            (168, b"\xb0\x4a\x20"), (180, b"\xe0\x00\x40")])
text = m.convert(clip, "t", mode="chord")
lines = text.splitlines()
check(lines[0] == "oscairn-phrase 1", "header")
check("length 1920" in lines, "one bar at 480 ppq")
check("  note 0 p0 v100 g240" in lines, "root as position 0, gate scaled 96->480 ppq")
check("  note 480 p1+1 v80 g120" in lines, "E5 is position 1 an octave up")
check("  note 720 r65 v80 g120" in lines, "F is not a chord tone: relative key")
check("  cc 840 74 32" in lines, "controller kept")
check("  bend 900 0" in lines, "centre bend is 0")
check(lines[-1] == "end", "track closed")
fixed = m.convert(clip, "t", mode="fixed").splitlines()
check("  note 480 k76 v80 g120" in fixed, "fixed mode keeps the key")
check("  root 60" in fixed, "relative notes need the root")
two = m.convert(clip, "t", mode="chord", bars=2).splitlines()
check("length 3840" in two, "--bars")
try:
    m.convert(b"RIFF....", "t")
    check(False, "non-SMF rejected")
except ValueError:
    pass
print("ok")
