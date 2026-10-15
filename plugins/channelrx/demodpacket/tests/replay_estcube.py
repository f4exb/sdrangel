#!/usr/bin/env python3
"""Replay the public cubehub capture through the production PacketDemodSink.

Requires numpy/scipy for capture preparation only; the plugin has no new deps.
Download separately (not redistributed here):
https://raw.githubusercontent.com/cubehub/samples/0f4ecdf503d52fe9d9a17610c8eef04ac6e97033/ax25_fsk9600_1024k_i16.wav
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("capture", type=Path)
parser.add_argument("sink_test", type=Path)
args = parser.parse_args()
digest = hashlib.sha256(args.capture.read_bytes()).hexdigest()
expected_digest = "a74fd9ba869d02b4668c84bae3de6a1b096c960613823bc24f6fd04416b6cdb8"
if digest != expected_digest:
    raise SystemExit("Capture SHA-256 does not match the pinned fixture")
rate, samples = wavfile.read(args.capture)
if rate != 1024000 or samples.ndim != 2 or samples.shape[1] != 2 or samples.dtype != np.int16:
    raise SystemExit("Unexpected capture format")
iq = (samples[:, 0] + 1j * samples[:, 1]) / 32768.0
# The four bursts are centered near +14 kHz. Apply one fixed tuning correction,
# then an anti-aliased rational resample. No symbol decisions or frame gating.
iq *= np.exp(-2j * np.pi * 14000 * np.arange(len(iq)) / rate)
iq = resample_poly(iq, 3, 80)
# Fixed gain uses the integer sink's dynamic range without clipping.
packed = (np.column_stack((iq.real, iq.imag)) * 100).astype("<f4")
if np.max(np.abs(packed)) >= 1:
    raise SystemExit("Unexpected amplitude: fixture would clip")
with tempfile.TemporaryDirectory(prefix="packet-estcube-") as directory:
    path = Path(directory) / "capture.cf32"
    packed.tofile(path)
    result = subprocess.run([str(args.sink_test.resolve()), str(path)],
                            check=True, text=True, capture_output=True)
actual = [s for s in result.stdout.splitlines() if re.fullmatch(r"[0-9a-f]+", s)]
expected = Path(__file__).with_name("estcube-expected.hex").read_text().splitlines()
if actual != expected:
    raise SystemExit(f"Replay mismatch: decoded {len(actual)} frames; expected {len(expected)} exact frames")
print("PASS: ESTCube fixture, 4 exact 237-byte AX.25 frames including FCS")
