#!/usr/bin/env python3
"""Cross-check G3RUH decoding with Dire Wolf's independent generator/decoder.

Usage: python3 verify_direwolf.py /path/to/packet_g3ruh_test
Needs gen_packets and atest on PATH (Dire Wolf, test tools only).
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("test_binary", type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="packet-direwolf-") as directory:
    wav = Path(directory) / "generated.wav"
    pcm = Path(directory) / "generated.pcm"
    subprocess.run(["gen_packets", "-B", "9600", "-r", "38400", "-o", str(wav)],
                   check=True, capture_output=True)
    with wave.open(str(wav)) as audio:
        if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (1, 2, 38400):
            raise SystemExit("Unexpected generated WAV format")
        pcm.write_bytes(audio.readframes(audio.getnframes()))
    result = subprocess.run([str(args.test_binary.resolve()), str(pcm)],
                            check=True, text=True, capture_output=True)
    actual = [s[:-4] for s in result.stdout.splitlines() if re.fullmatch(r"[0-9a-f]+", s)]
    oracle = subprocess.run(["atest", "-B", "9600", "-h", str(wav)],
                            check=True, text=True, capture_output=True)
    text = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", oracle.stdout)
    expected = []
    current = ""
    for line in text.splitlines():
        match = re.match(r"  ([0-9a-f]{3}):  ((?:[0-9a-f]{2} ?)+)", line)
        if match:
            if match[1] == "000" and current:
                expected.append(current)
                current = ""
            current += match[2].replace(" ", "")
    if current:
        expected.append(current)
    if len(expected) != 4 or actual != expected:
        raise SystemExit(f"Dire Wolf mismatch: receiver {len(actual)} frames, oracle {len(expected)}")
print("PASS: 4 Dire Wolf generated frames match atest byte for byte (FCS excluded by atest)")
