# Receiving guide

What to tune to, which demodulator to use, and what to watch out for. Frequencies are from the
plugin documentation, or from the standard named where a readme has no table; call `get_docs`
for the full readme of any plugin named here, and `describe_settings` before setting any key.

Several bands differ by region: the APRS frequency, pager bands, the broadcast FM raster and
airband channel spacing all do. Where a table below offers alternatives, check where the
receiver is with `get_location`, or ask, rather than picking one.

## Choosing a device sample rate

The device sample rate sets how much spectrum you can see and process at once. Rules of thumb:

- A narrowband mode (voice, APRS, AIS, pagers) needs only tens of kHz, so a low device rate such
  as 250 kS/s to 1 MS/s is enough and uses far less CPU.
- The channel's own bandwidth is set separately, on the channel, not on the device.
- Several channels can share one device as long as each one's offset falls inside the device
  bandwidth. That is the cheap way to receive two nearby frequencies at once.
- A few modes need a minimum rate, listed below. Setting less than the minimum produces nothing.
- The key differs by device: an RTL-SDR has `devSampleRate` and `log2Decim` (the baseband is
  the rate divided by 2 to the power of `log2Decim`), most others `sampleRate`; `describe_settings`
  with the device's hwType shows which. `listen` and `scan` set the rate themselves for an
  RTL-SDR and the test source, and say so when they have left another device's rate as it was.

Minimum device sample rates:

| Mode | Minimum | Why |
|------|---------|-----|
| ADS-B | 2 MS/s | 2 Mchip/s chip rate. Higher rates decode more distant aircraft |
| DAB | 2.048 MS/s | Fixed by the standard |
| Broadcast FM | 240 kS/s | Must exceed the channel RF bandwidth. 256 kS/s is recommended for a 180 kHz channel |

## Aircraft

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| ADS-B and Mode S | `ADSBDemod` | 1090 MHz | Needs at least 2 MS/s. Recommend 2.4 MS/s for RTL SDR. Feeds aircraft to the map |
| Airband voice | `AMDemod` | 118 to 137 MHz | Amplitude modulation, not FM. 8 kHz channel bandwidth |
| VOR navaids | `VORDemod`, `VORDemodMC` | 108 to 117.975 MHz | `VORDemodMC` receives several VORs at once for a position fix |
| ILS localizer | `ILSDemod` | 108 to 112 MHz | Horizontal guidance |
| ILS glide slope | `ILSDemod` | 329 to 335 MHz | Vertical guidance, paired with the localizer |

## Ships

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| AIS | `AISDemod` | 161.975 MHz and 162.025 MHz | The two channels are 50 kHz apart, so one device at 162 MHz covers both with two channels at offsets of -25 kHz and +25 kHz. `listen` with mode `ais` at 162000000 does exactly that, and adds the AIS feature, which lists the ships and puts them on the map |
| DSC distress and calling | `DSCDemod` | 2187.5 kHz, 8414.5 kHz, 16804.5 kHz, 156.525 MHz | The VHF one is marine channel 70 - but isn't yet supported |
| NAVTEX | `NavtexDemod` | 518 kHz, 490 kHz, 4209.5 kHz | 518 kHz is English international, 490 kHz national languages |

## Amateur and data

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| APRS and AX.25 packet | `PacketDemod` | 144.800 MHz in Europe, 144.390 MHz in North America | `listen` with mode `aprs` adds the APRS feature, which decodes and maps the stations; mode `packet` is the demodulator alone |
| M17 digital voice | `M17Demod` | 144 and 430 MHz bands | Also carries data packets |
| FT8 and other digital HF | `FT8Demod` | HF band segments | Needs an HF capable receiver |
| SSB and CW | `SSBDemod`, `WDSPRx` | HF | `WDSPRx` has the more capable receiver chain |
| RTTY | `RTTYDemod` | HF | |
| DMR, dPMR, D-Star, YSF, NXDN | `DSDDemod` | Varies by band plan | `listen` with mode `dmr`, `dpmr`, `dstar`, `ysf` or `nxdn` sets the deviation and baud rate the standard needs. Needs the AMBE feature or hardware for voice |

## Weather, time and utility

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| Radiosondes (RS41) | `RadiosondeDemod` | 400 to 406 MHz | Balloons are up around 00:00 and 12:00 UTC. The Radiosonde feature decodes and maps them; `listen` with mode `sonde` adds it |
| NOAA APT weather images | `APTDemod` | NOAA 15 at 137.620 MHz, NOAA 18 at 137.912 MHz, NOAA 19 at 137.100 MHz | Historical: the NOAA APT satellites were retired in 2025, so there is nothing live to receive. The demodulator still decodes recordings |
| Time signals | `RadioClock` | MSF 60 kHz, DCF77 77.5 kHz, TDF 162 kHz, WWVB 60 kHz, JJY 40 kHz | Needs a receiver that covers VLF or LF |
| Inmarsat STD-C | `InmarsatDemod` | 1537.70, 1541.45, 1537.10 MHz | L band, needs a suitable antenna and often an LNA |

## Broadcast

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| Broadcast FM | `BFMDemod` | 87.5 to 108 MHz | Use this rather than `WFMDemod` for broadcast: it decodes stereo and RDS. Stations sit on a 100 kHz raster (87.5, 87.6, 87.7...) everywhere except the Americas, where they are on the odd tenths 200 kHz apart; scan with a 100 kHz step, as a 200 kHz step from 87.5 MHz lands beside every even-tenth station and never on it |
| Wideband FM | `WFMDemod` | Various | Plain wideband FM with no RDS |
| DAB | `DABDemod` | Band III | Fixed 2.048 MS/s |
| AM broadcast | `AMDemod` | LW, MW, SW bands | |

### Checking DAB reception quality

After starting the device and selecting a programme, wait briefly and call
`get_channel_report` again. Do not treat `sync: 1`, a populated programme list or
`audioActive: 1` alone as proof of clean reception: DAB audio can still be distorted.

Check `snr` in `DABDemodReport`. If it is 0 dB or less while synchronized audio is active,
receiver overload is a likely cause. With an RTL-SDR, read its supported gain values from
`get_device_report`, reduce `gain` by one supported step with `set_device_settings`, wait
briefly, and re-read the channel report. Repeat while SNR improves, and use the gain that
gives the best positive SNR rather than the highest available gain. If lower gain makes
synchronization or SNR worse, restore the better setting and investigate frequency,
antenna and local interference instead.

## Short range and IoT

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| LoRa | `ChirpChatDemod` | 433, 868, 915 MHz ISM bands | Bandwidth and spreading factor must match the transmitter exactly or nothing decodes |
| Meshtastic | `MeshtasticDemod` | 433 MHz (EU_433), 868 MHz (EU_868), 902 MHz (US) | Set the region to match the network |
| MeshCore | `MeshcoreDemod` | Same ISM bands as Meshtastic | |
| POCSAG pagers | `PagerDemod` | Regional, commonly 138 to 160 MHz. 153.350 MHz in UK. | FSK with a 4.5 kHz shift |
| End of train devices | `EndOfTrainDemod` | 457.9375 MHz North America and India, 477.7 MHz Australia, 450.2625 MHz New Zealand | |

## Scanning for activity

`scan` steps a Frequency Scanner over a range or a list and reports what was active. Read
`active` for the answer: every frequency above the threshold with its level, strongest first.
`heard` is where the scanner parked and for how long, which distinguishes intermittent signals
such as airband calls; on a band of continuous carriers it parks on the first one it finds and
stays, so `heard` names one station where `active` names them all. It only finds what it
lands on, so the step has to match the channel spacing the band actually uses:
a step that is too coarse lands beside the channels rather than on them, and a channel wide
enough to catch the neighbour then reports it 100 kHz off and weaker, where nothing decodes.
Give `stepFrequency` only to override the spacing below; without it `scan` uses the spacing
usual for the mode.

| What | `mode` | Range | Spacing | Notes |
|------|--------|-------|---------|-------|
| Broadcast FM | `bfm` | 87.5 to 108 MHz | 100 kHz | The Americas use only the odd tenths, 200 kHz apart; everywhere else every tenth. `scan` insists on 100 kHz for this mode |
| Airband voice | `am` | 118 to 137 MHz | 25 kHz | Europe also uses 8.33 kHz channels (`stepFrequency` 8333 from a 25 kHz frequency lands on all of them). The whole band at 8.33 kHz is over the 2000 frequency limit, so take it in sub bands |
| Marine VHF, PMR446, amateur FM | `nfm` | 156 to 162 MHz marine, 446.0 to 446.2 MHz PMR, 144 to 146 and 430 to 440 MHz amateur | 12.5 kHz (PMR446 6.25 kHz, marine 25 kHz) | |
| Digital voice | `dmr`, `dpmr`, `dstar`, `ysf`, `nxdn` | Same bands as `nfm` | 12.5 kHz (dPMR and NXDN 2400 baud 6.25 kHz) | Each sets the DSD demodulator's FM deviation and baud rate for the standard (DMR 5.4 kHz, dPMR 2.7 kHz at 2400 baud, D-Star 3.5 kHz, YSF 7 kHz, NXDN 2.7 kHz; `nxdn24` for the 2400 baud variant), which the decoder needs to recover symbols; `dsd` alone is the plugin's defaults, D-Star's. DMR gets both time slots on. Detects the carrier; decoding voice needs the AMBE feature or hardware |
| POCSAG pagers | `pager` | Regional, commonly 138 to 160 MHz | 12.5 kHz | Finds the carriers; the messages themselves are shown only in the GUI, the API carries none |
| Radiosondes (RS41) | `sonde` | 400 to 406 MHz | 10 kHz | Only worth it around launch times, 00:00 and 12:00 UTC. Decoded frames come from `get_packets` |
| SSB and CW | `ssb`, `usb`, `lsb` | HF band segments | 3 kHz | The scanner measures power in a 3 kHz channel; a busy band segment needs the threshold raised |

Do not scan for these; they are on fixed frequencies, so `listen` or `add_channel` is the tool:

- **ADS-B**: 1090 MHz only.
- **DAB**: fixed Band III blocks 5A to 13F (174.928 to 239.200 MHz, 1.712 MHz apart). Start a
  `dab` channel on each block in turn and read the programme list from its report.
- **AIS**: 161.975 and 162.025 MHz.
- **DSC**: 2187.5 kHz, 8414.5 kHz, 16804.5 kHz and 156.525 MHz.
- **NAVTEX**: 518, 490 and 4209.5 kHz.
- **APRS**: 144.800 MHz in Europe, 144.390 MHz in North America.
- **Time signals, Inmarsat, end of train**: the frequencies in the tables above.

## Frequencies asked for by name

| Name | Frequency |
|------|-----------|
| Airband guard (emergency) | 121.500 MHz |
| Airband air to air | 123.450 MHz |
| Marine channel 16 (distress and calling) | 156.800 MHz |
| Marine channel 70 (DSC, data not voice) | 156.525 MHz |
| Marine channels 6, 12, 13, 67 | 156.300, 156.600, 156.650, 156.375 MHz |
| PMR446 channels 1 to 16 | 446.00625 MHz plus 12.5 kHz per channel, to 446.19375 MHz |
| 2 m amateur FM calling | 145.500 MHz in Europe, 146.520 MHz in North America |
| 70 cm amateur FM calling | 433.500 MHz in Europe, 446.000 MHz in North America |

DAB Band III blocks, for one `dab` channel per block (1.712 MHz apart; the ensemble on each
depends on the country, and its report names it):

| Block | MHz | Block | MHz | Block | MHz | Block | MHz |
|-------|-----|-------|-----|-------|-----|-------|-----|
| 5A | 174.928 | 7A | 188.928 | 9A | 202.928 | 11A | 216.928 |
| 5B | 176.640 | 7B | 190.640 | 9B | 204.640 | 11B | 218.640 |
| 5C | 178.352 | 7C | 192.352 | 9C | 206.352 | 11C | 220.352 |
| 5D | 180.064 | 7D | 194.064 | 9D | 208.064 | 11D | 222.064 |
| 6A | 181.936 | 8A | 195.936 | 10A | 209.936 | 12A | 223.936 |
| 6B | 183.648 | 8B | 197.648 | 10B | 211.648 | 12B | 225.648 |
| 6C | 185.360 | 8C | 199.360 | 10C | 213.360 | 12C | 227.360 |
| 6D | 187.072 | 8D | 201.072 | 10D | 215.072 | 12D | 229.072 |
| 13A | 230.784 | 13B | 232.496 | 13C | 234.208 | 13D | 235.776 |
| 13E | 237.488 | 13F | 239.200 | | | | |

## Reading the results

Decoded output lands in one of three places, depending on the mode. Reading the wrong one
looks like an empty band.

| Mode | Where the output is | What to read |
|------|---------------------|--------------|
| Voice: `nfm`, `am`, `ssb`, `bfm`, `wfm`, `dsd` | Audio on the default output | `capture_audio` records a few seconds and reports the peak level, which tells silence (squelch closed, or nothing there) from speech |
| Broadcast FM | Channel report | `BFMDemodReport.rdsReport`: station name, programme type, radio text, once RDS has locked |
| DAB | Channel report | `DABDemodReport`: `sync`, `snr`, `ensembleName`, `programs`; select a programme with `set_channel_settings` and read `audioActive` |
| ADS-B | Map items | `get_map_items` lists the aircraft with position, altitude, heading and callsign; the channel report only has counts and the tracked target |
| AIS, APRS and AX.25, LoRa, M17, Meshtastic, MeshCore, Inmarsat, radiosondes | Packet buffer | `get_packets`, which decodes AIS position reports, APRS positions and RS41 frames. Ships, stations and sondes also appear in `get_map_items` when the Map feature is present |
| VOR | Channel report | `VORDemodReport.radial` with `validRadial`, and the Morse ident |
| Pagers, DSC, NAVTEX, RTTY, FT8 | GUI only | The decoded messages are shown in the plugin's window; the API carries the channel power only. Say so rather than reporting the band as empty |

How long to wait before reading: RDS a few seconds, the full station name up to ten; DAB sync
about two seconds and the programme list five to ten; ADS-B ten seconds if aircraft are in
range; AIS half a minute, as each ship transmits every few seconds at most; APRS minutes,
as stations transmit rarely; radiosonde frames once a second while a sonde is up; VOR a few
seconds for a valid radial. A single read immediately after tuning proves nothing; read the
report again after the interval before concluding a signal has no data.

## What has been on a band over time

One spectrum is one instant. `get_spectrum_history` reads the history the GUI keeps for
scrolling the waterfall, switching scrolling on if it is off, and reports per bin the maximum,
the mean and the occupancy over the last `seconds`, plus a `signals` list: every run of bins
that rose above the floor, with its frequency, width, peak, `dutyCycle` and when it was first
and last seen. A carrier has a duty cycle near 1, a voice channel a fraction, a pager burst
almost nothing; a hopper shows as many short signals. It costs no channels and no retuning, so
it is the first thing to try on an unfamiliar band, before `scan`. Only rows taken with the
device tuned and sampling as it is now count, so after a retune wait for new history.
`get_waterfall_image` shows the same history as a picture, frequency across and time down,
for the cases where the shape matters: drift, hopping, burst timing.

## Judging a signal level

Power figures (`signalDb`, `channelPowerDB`, the scanner's `power`) are relative to the device
and its gain, so no fixed number means "signal present". Compare against the noise floor
instead: `scan` measures it, reports it as `noiseFloorDb` and sets its own threshold 12 dB
above it. Treat a channel within about 6 dB of the floor as empty, and a reading of -150 as
"not settled yet", which `listen` says when it sees it. A real signal stands out by 15 dB or
more, a broadcast FM station typically by 20 to 30 dB. When everything reads at the floor,
the receiver is not hearing anything at all: check the device is running, the gain, and the
antenna, in that order.

## Gain

`tune_gain` finds the gain for you: it steps the device through its range, measures the noise
floor and the strongest signal (or the one at a given frequency) at each, and applies the gain
where the signal stands furthest above the floor, preferring the lower of any that tie. On a
quiet band it goes by the floor alone: the lowest gain at which the antenna's noise, not the
converter's, sets the floor, short of overload. Use it
after tuning to the band of interest and before judging weak signals; the reply's table shows
where the front end starts to overload. It turns the device's AGC off, which a manual gain
requires. It knows the RTL-SDR, HackRF, Airspy, LimeSDR, PlutoSDR and USRP; for another
device read `describe_settings` and give it the gain key and values. `listen` and `scan` run
it themselves when they create a device set or move one to another band, and say what they
set; pass `tuneGain` false to keep a gain you have chosen. Once the gain is applied the main
spectrum is autoscaled, so its reference level and range suit the new gain; `autoscale`
false leaves the display as it was. A change of mode or a move within a
band does not trigger it, as the gain suits the band and the antenna rather than the mode.

RTL-SDR `gain` is in tenths of a decibel (402 is 40.2 dB) and only the values in
`get_device_report`'s supported list take effect; `agc` hands control to the tuner, which is
poor on weak signals. `listen` and `scan` set 40 dB with AGC off on a device set they create,
and leave one they found exactly as it was set. Too little gain shows as every channel at
the floor; too much as a collapsing SNR on a strong signal, distortion, and spurious copies
of strong stations across the band. When a strong signal decodes badly, reduce gain by one
supported step and read the report again, as the DAB section above describes; keep the
setting that gives the best SNR rather than the highest level.

RDS is the first casualty of overload: a broadcast FM station can sound clean and still decode
no RDS groups at all when the front end is compressed, and the report may then claim stereo
while showing no `pid`. On a strong-signal site the RTL-SDR decodes RDS best with only a few dB
of gain. Decimation matters too: a 180 kHz channel in a 256 kS/s baseband sits against the edge
of the decimation filters, so `listen` and `scan` use 512 kS/s or more for broadcast FM, and
put the channel far enough from the device centre that the DC spike is outside it.

## Utility channels

These do not demodulate a protocol but are useful for looking and recording.

| What | Channel | Use |
|------|---------|-----|
| `FileSink` | Records baseband IQ to a file | The capture tools use this |
| `SigMFFileSink` | Records IQ in SigMF format | For interchange with other tools |
| `ChannelAnalyzer` | Inspects a signal's spectrum and waveform | For working out what an unknown signal is |
| `FreqScanner` | Scans a range for activity | For finding active frequencies |
| `ChannelPower` | Measures power in a channel | For comparing signal strengths |
| `HeatMap` | Maps signal strength against position | Coverage surveys |

## Common reasons nothing is received

- **The device is not running.** Call `start_device`. Settings can be changed while stopped, but
  no samples flow.
- **The signal is outside the device bandwidth.** A channel's `inputFrequencyOffset` is relative
  to the device centre frequency, and must fall within half the sample rate either side.
- **The sample rate is below the minimum** for the mode, most often with ADS-B.
- **The device cannot tune there.** An RTL-SDR covers about 24 to 1766 MHz; below that (DSC,
  NAVTEX, FT8, AM and SSB on HF, the time signals) it receives only in direct sampling mode
  (`noModMode` in its settings, with poor sensitivity) or through an upconverter. Check the
  device's readme for its range before tuning outside VHF and UHF.
- **The radio is already in use.** One dongle serves one device set. `listen` and `scan` reuse the
  set that holds it; `add_deviceset` for the same hardware fails or fights for it. Put a second
  frequency on the same set as another channel, if its offset fits the baseband.
- **The squelch is closed.** `capture_audio` reports the peak level and warns when the audio is
  silent, which usually means squelch rather than a broken setup.
- **Gain is too low or too high.** Too low buries the signal in noise; too high overloads the
  front end. Check the device report and the spectrum.
- **The antenna is wrong for the band.** Nothing in software fixes a 2 metre whip at 1090 MHz.
- **A bias tee is needed** for a powered antenna or LNA, and is off by default.
- **The satellite or balloon is not overhead.** APT, radiosondes and Inmarsat all depend on this.
