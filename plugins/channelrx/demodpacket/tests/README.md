# Packet Demod regression tests

The tests compile the production detector, framer, CRC, settings, and (for the
integration target) sink. They do not substitute a second receive implementation.
The synthetic transmitter uses independent HDLC stuffing and bitwise CRC, plus
SDRangel's existing Packet Modulator LFSR. Waveform shaping is an analytic
Gaussian integral, independent of the receive filter and clock recovery.

## DSP, framing and settings

From the repository root, with CMake, a C++17 compiler and Qt Core/Gui development
packages (Qt 5 or Qt 6):

```sh
cmake -S plugins/channelrx/demodpacket/tests -B build-packet-tests \
  -DCMAKE_BUILD_TYPE=Debug -DPACKET_TEST_SANITIZERS=ON
cmake --build build-packet-tests --parallel 2
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-packet-tests --output-on-failure
```

Coverage: 36 cases, each comparing 40 complete frames including FCS. Rectangular
and BT=0.5 Gaussian symbols, both polarities, five fractional starting phases,
clock errors through +/-1000 ppm, carrier offsets through +/-1500 Hz with 20 Hz/s
drift, four deviations from 1.5 to 4.5 kHz, and deterministic noise (sigma=0.03 per
unit-amplitude I/Q component). These are correctness cases, not a sensitivity or
packet-error-rate benchmark. Also checks reset/reacquisition, shared flags,
stuffing-heavy/binary payloads, bad CRC rejection, wrong-mode rejection, old and
invalid presets, idle framing, and AFSK framing/Chase recovery.

## Full sink and plugin integration

Use the normal SDRangel build dependencies. This builds the server plugin and a
test which drives its actual `feed()` path, including NCO, RF filtering,
resampling, FM discrimination and the packet message queue:

```sh
cmake -S . -B build-packet-server -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_GUI=OFF -DBUILD_SERVER=ON -DBUILD_BENCH=OFF \
  -DENABLE_EXTERNAL_LIBRARIES=OFF -DENABLE_LIBUNWIND=OFF \
  -DENABLE_FEATURE=OFF -DENABLE_CHANNELTX=OFF -DENABLE_CHANNELMIMO=OFF \
  -DPACKETDEMOD_BUILD_TESTS=ON -DPACKETDEMOD_TEST_SANITIZERS=ON
cmake --build build-packet-server --target demodpacketsrv packetdemod_sink_test --parallel 2
UBSAN_OPTIONS=halt_on_error=1 ./build-packet-server/packetdemod_sink_test
```

Eight cases compare ten frames each at input rates 32, 38.4, 48 and 96 kHz,
residual offsets +/-1 kHz, and 250 ppm clock error. The same sink then switches
to both AFSK correlator and MLSE reception (three frames each), and back to
G3RUH (ten frames each), using partial settings updates. The AFSK test includes
noise before/after its burst because the MLSE activity gate needs a noise-floor
measurement. API adapter checks exercise valid modes, rejection of invalid
modes without mutation, and preservation of RF bandwidth on mode-only updates.

The integration test compiles the production channel, baseband, sink, settings
and API adapter directly, so it works with both Qt 5 and Qt 6 without linking
against the loadable plugin. Add `-DENABLE_QT6=ON` for a Qt 6 server build. CI
builds the plugin and runs the integration test with both Qt versions.

Sanitizers instrument the test and these compiled Packet Demod sources; linked
SDRangel/Qt libraries use the normal build flags. In containers where LeakSanitizer
cannot enumerate processes, set `ASAN_OPTIONS=detect_leaks=0`. This leaves address,
undefined-behaviour and float-conversion checks enabled, but does not test leaks.

## Independent modem interoperability

With Dire Wolf `gen_packets` and `atest` on PATH:

```sh
python3 plugins/channelrx/demodpacket/tests/verify_direwolf.py \
  build-packet-tests/packet_g3ruh_test
```

This generates G3RUH audio with Dire Wolf and compares all four decoded packets
against `atest`, byte for byte. Dire Wolf is only a test oracle, not a plugin
dependency. Tested locally with Dire Wolf 1.7.

## Public IQ capture

The [cubehub sample](https://github.com/cubehub/samples/blob/0f4ecdf503d52fe9d9a17610c8eef04ac6e97033/ax25_fsk9600_1024k_i16.wav)
is a 1.024 Msps int16 IQ recording, identified as ESTCube-1 in the
[original discussion](https://github.com/jgaeddert/liquid-dsp/issues/9).
Download that file separately; it is not redistributed in this PR. The preparation
script requires NumPy and SciPy and verifies the original file's SHA-256.

```sh
python3 plugins/channelrx/demodpacket/tests/replay_estcube.py \
  /path/to/ax25_fsk9600_1024k_i16.wav build-packet-server/packetdemod_sink_test
```

The script applies a fixed -14 kHz translation, anti-aliased 3/80 resampling and
fixed gain, then passes IQ through the production sink. Expected result: four
237-byte AX.25 frames, including valid FCS, matching `estcube-expected.hex`.
One short capture is an interoperability regression fixture, not evidence of
performance across all satellites, weak-signal conditions or receiver hardware.
