# modemdab

A native DAB/DAB+ receiver (ETSI EN 300 401, transmission mode I), used by the
DAB Demodulator plugin.

## What it does

From complex baseband at 2.048 MS/s it produces:

- **The ensemble:** services, labels, PTy, language, user applications, and
  date and time.
- **Audio:** of the selected service, as 16-bit stereo PCM. DAB+ (HE-AAC v2)
  is decoded by FAAD2 and DAB (MPEG Layer II) by FFmpeg's libavcodec; without
  a codec, the service is still found, but no audio is produced.
- **Data:** the dynamic label with DL Plus tags, MOT SlideShow images and
  Journaline, from X-PAD or a packet mode component.
- **Service and Programme Information (SPI):** station names, descriptions
  and logos, and programme schedules, from the ensemble's SPI data service
  (MOT directory mode), whatever service is selected.
- **Announcements:** traffic, news, alarm and other announcements (FIG 0/18
  and 0/19). The selected service's audio switches to them, for the types
  chosen (`setAnnouncementTypes()`); alarms always.
- **Service following:** where a service can also be received: other
  ensembles (FIG 0/24), and linked DAB, FM and DRM services (FIG 0/6), with
  their frequencies (FIG 0/21).
- **TII:** the identifiers of the transmitters received.
- **Statistics:** including the channel bit error rate (estimated by
  re-encoding the Viterbi output), the carrier to noise ratio from the null
  symbol (overall and per carrier), and the MER of the demodulated symbols.

| Files | Responsibility |
| --- | --- |
| `dabtables` | Mode I constants, the phase reference, frequency interleaving, puncturing, UEP/EEP profiles, PRBS, CRCs, character sets, PTy and language names |
| `dabofdm` | DC offset removal, channel filter, null symbol and phase reference acquisition, carrier and clock tracking, FFT window placement from the impulse response (searching again within 0.3 s when input samples are lost), differential and three-symbol demodulation, and soft bits weighted by each carrier's noise (from the null symbol) |
| `dabconvolutional` | Mother code, depuncturing and the 64-state soft Viterbi decoder (shared with DRM, in `modemdr`), with an optional thresholded soft output (SOVA) |
| `dabensemble` | FIC decoding and the FIG database, with current and next configurations; announcements, service linking, frequency information and other ensembles' services |
| `dabmsc` | Sub-channel time deinterleaving and decoding; packet mode |
| `dabaudioframing` | DAB+ superframes (Fire code, RS(120,110), AUs) and Layer II frames (header CRC) |
| `dabaudiodecoder` | FAAD2 and FFmpeg backends with concealment |
| `dabpad` | F-PAD/X-PAD, the dynamic label, DL Plus and MSC data groups |
| `dabtii` | TII detection |
| `dabcoherent` | Decoder aided (near coherent) demodulation, from the whole multiplex decoded and re-encoded |
| `dabreceiver` | The receiver: service selection, detection, callbacks, status and an optional decoding thread |
| `dabfft` | FFTs: FFTW when built in, kissfft otherwise |
| `dabprofile` | Time per stage, with `MODEMDAB_PROFILE` set |

MOT (slideshow, Broadcast Website and SPI files), SPI, Journaline, the CRCs, inflate, the
character sets and the Viterbi decoder are shared with `modemdrm`, in `modemdr`.

## Performance

One DAB+ service with its data takes about 2.3% of a core (44x real time) on
the development machine. The Viterbi decoder has SSE2 (x86) and NEON (AArch64)
forward passes (the scalar pass, for other architectures, has the same butterfly form;
`MODEMDR_SCALAR` or `MODEMDAB_SCALAR` selects it, to compare).

`setThreaded(true)` moves decoding to a second thread. Callbacks are still
made in the caller's thread, from `process()`.

## Detection

`setDetection()` chooses how the carriers are demodulated:

| Detection | AWGN threshold | CPU | Delay |
| --- | ---: | ---: | --- |
| `ThreeSymbol` (multiple symbol differential, the default) | 5.1 dB | 3.2% | - |
| `Coherent` (three-symbol, then decoder aided) | 4.4 dB | 8-9% | about 0.4 s more |
| `Differential` (for comparison) | 5.3 dB | 2.3% | - |

The thresholds are for a DAB+ service at EEP 3-A in a full multiplex. CPU is
for the 11B recording, a multiplex of 14 sub-channels (coherent detection
takes more in weak signals: 13-14% at 6 dB). In fading, coherent detection is
about 1 dB better near the threshold at 60 km/h, and about equal at 120 km/h.

`Coherent` decodes every sub-channel, re-encodes it, and demodulates the
frame again with references averaged over many symbols:

- **Unreliable bits:** bits that the Viterbi decoder's soft output marks
  unreliable are not used (`setReliabilityThreshold()`).
- **Fallback:** where too few neighbours are known for the references to beat
  differential detection, the three-symbol soft bits are kept. In weak signals
  few decoded bits are reliable, so this keeps it from being worse than
  three-symbol.
- **More passes:** `setIterations()` adds passes. Two gain about 0.2 dB in
  AWGN but lose about 0.3 dB in a static echo, at twice the cost.

The detection can be changed while running; the audio restarts. The DAB
Demodulator's Coherent button selects between `ThreeSymbol` and `Coherent`.
