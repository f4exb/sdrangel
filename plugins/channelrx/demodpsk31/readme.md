<h1>PSK31 Demodulator Plugin</h1>

<h2>Introduction</h2>

This channel receives PSK31 text. It performs carrier and symbol-clock recovery,
differential BPSK decoding, and Varicode decoding at the standard 31.25 baud rate.
The decoder supports the same extended Latin-1 Varicode table as the [PSK31 modulator](../../channeltx/modpsk31/readme.md).

<h2>Interface</h2>

The top and bottom bars of the channel window are described [here](../../../sdrgui/channel/readme.md).

![PSK31 Demodulator plugin GUI](../../../doc/img/PSK31Demod_plugin.png)

<h3>1: Frequency offset</h3>

Tune the channel marker to the center of the PSK31 signal. The carrier-recovery
loop corrects a small remaining frequency and phase error. With the default
100 Hz bandwidth, tune to within about 10 Hz for reliable acquisition. The
carrier status label shows the residual error after the loop has locked.

<h3>2: Channel power and level meter</h3>

Channel power is displayed in dB relative to a full-scale signal. The level meter
shows average power, instantaneous peak power, and peak hold.

<h3>3: RF bandwidth</h3>

This sets the input filter bandwidth. The default 100 Hz bandwidth is appropriate
for most PSK31 signals. A narrower setting rejects more adjacent-channel energy;
a wider setting allows a larger initial tuning error.

<h3>4: UDP</h3>

When checked, received characters are forwarded to the specified UDP address (5) and port (6).

<h3>5: UDP address</h3>

IP address of the host to forward received characters to via UDP.

<h3>6: UDP port</h3>

UDP port number to forward received characters to.

<h3>7: Carrier status</h3>

Shows whether the demodulator is locked to the carrier. Once locked, it also
shows the residual frequency error and the estimated SNR. The SNR is measured
from the carrier-corrected constellation over one second of locked samples. Its
noise power is normalized to a 100 Hz reference bandwidth, so changing the RF
bandwidth does not introduce the usual 3 dB change for each doubling or halving
of bandwidth. The first SNR value is displayed after one complete second of
carrier lock; samples collected before acquisition are excluded.

<h3>8: Start/stop Logging Messages to .txt File</h3>

When checked, writes all received characters to the .txt file specified by (9).

<h3>9: .txt Log Filename</h3>

Click to specify the name of the .txt file which received characters are logged to.

<h3>10: Clear</h3>

Clear the received text view (11) of received characters.

<h3>11: Received text</h3>

Decoded text is appended to the text view.

<h2>Measured performance</h2>

The sample-level benchmark sends a
raised-cosine PSK31 waveform with 256 idle symbols followed by the 30-character
test frame `CQ CQ DE SDRANGEL 0123456789\r\n`. A frame passes only when every
character is correct. Run it with:

```
sdrangelbench -t psk31 -a perf
```

Measurements on the 1000 sample/s internal receiver with the default 100 Hz RF
bandwidth gave these results:

| Test | Result |
|---|---|
| AWGN, acquired carrier | 32% exact frames at 7 dB SNR; 62% at 8 dB; 88% at 9 dB; 96% at 10 dB; 100% at every tested point from 11 through 16 dB |
| Residual frequency, random starting phase | 10/10 exact frames at every offset from -14 Hz through +11 Hz; use +/-10 Hz as the tuning target |
| Sample-clock error | 5/5 exact frames with zero character errors at every tested point from -5000 through +5000 ppm |
| Noiseless input level | 8/8 timing phases at every tested level from -10 through -60 dBFS |
| Noise only at -40 dBFS | Zero lock acquisitions and zero decoded characters in one simulated hour |
| CW interferer at 25 Hz separation | 5/5 exact frames at -20 dB relative to the wanted signal; 3/5 at -10 dB; 0/5 at equal power |
| CW interferer at 50 Hz separation | 5/5 exact frames at -10 dB relative to the wanted signal; 0/5 at equal power |
| CW interferer at 75 or 100 Hz separation | 5/5 exact frames with the interferer 10 dB stronger than the wanted signal |
| Core demodulator CPU throughput | 5.60-5.93 million internal samples/s, or 5600-5900 times real time at 1000 samples/s |

SNR is average wanted-signal power divided by complex AWGN power in a 100 Hz
reference bandwidth, after carrier acquisition. The demodulator estimates signal
and noise from the in-phase and quadrature power of the recovered BPSK carrier,
then scales the measured noise from the selected RF bandwidth to 100 Hz. In the
regression test, a 20 dB input measured 21.25 dB with a 100 Hz RF bandwidth and
20.84 dB with a 200 Hz RF bandwidth. At 31.25 baud, 10 dB in 100 Hz corresponds
to about 15 dB Eb/N0. The frequency result measures acquisition after roughly
8.2 seconds of idle symbols; the loop's configured tracking clamp is wider than
its measured acquisition range. Input level alone is not a sensitivity figure
because the usable floor depends on noise and receiver dynamic range.

The CPU result is the range from three one-second runs of the RelWithDebInfo
Windows build on an Intel Core i9-14900K. It repeatedly feeds representative
BPSK samples directly to the demodulator sink. At the 1000 sample/s internal
rate, this corresponds to roughly 0.018% of one CPU core. It measures the
channel-specific carrier recovery, symbol timing, and decoding path; device
input, channelization, scope rendering, and other application overhead are not
included. Run this focused measurement with:

```
sdrangelbench -t psk31 -a cpu
```

The receiver qualifies carrier lock over eight symbols before decoding and drops
lock after two failed symbol checks. Run the standalone noise test with:

```
sdrangelbench -t psk31 -a noise
```
