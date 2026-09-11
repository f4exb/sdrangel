# Receiving guide

What to tune to, which demodulator to use, and what to watch out for. Frequencies are from the
plugin documentation; call `get_docs` for the full readme of any plugin named here, and
`describe_settings` before setting any key.

## Choosing a device sample rate

The device sample rate sets how much spectrum you can see and process at once. Rules of thumb:

- A narrowband mode (voice, APRS, AIS, pagers) needs only tens of kHz, so a low device rate such
  as 250 kS/s to 1 MS/s is enough and uses far less CPU.
- The channel's own bandwidth is set separately, on the channel, not on the device.
- Several channels can share one device as long as each one's offset falls inside the device
  bandwidth. That is the cheap way to receive two nearby frequencies at once.
- A few modes need a minimum rate, listed below. Setting less than the minimum produces nothing.

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
| AIS | `AISDemod` | 161.975 MHz and 162.025 MHz | The two channels are 50 kHz apart, so one device at 162 MHz covers both with two channels at offsets of -25 kHz and +25 kHz |
| DSC distress and calling | `DSCDemod` | 2187.5 kHz, 8414.5 kHz, 16804.5 kHz, 156.525 MHz | The VHF one is marine channel 70 - but isn't yet supported |
| NAVTEX | `NavtexDemod` | 518 kHz, 490 kHz, 4209.5 kHz | 518 kHz is English international, 490 kHz national languages |

## Amateur and data

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| APRS and AX.25 packet | `PacketDemod` | 144.800 MHz in Europe, 144.390 MHz in North America | Add the APRS feature to decode and map the packets |
| M17 digital voice | `M17Demod` | 144 and 430 MHz bands | Also carries data packets |
| FT8 and other digital HF | `FT8Demod` | HF band segments | Needs an HF capable receiver |
| SSB and CW | `SSBDemod`, `WDSPRx` | HF | `WDSPRx` has the more capable receiver chain |
| RTTY | `RTTYDemod` | HF | |
| DMR, dPMR, D-Star, YSF | `DSDDemod` | Varies by band plan | Needs the AMBE feature or hardware for voice |

## Weather, time and utility

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| Radiosondes (RS41) | `RadiosondeDemod` | 400 to 406 MHz | Balloons are up around 00:00 and 12:00 UTC. Add the Radiosonde feature for decoding and mapping |
| NOAA APT weather images | `APTDemod` | NOAA 15 at 137.620 MHz, NOAA 18 at 137.912 MHz, NOAA 19 at 137.100 MHz. Satellites No longer in service. | Satellite passes only. Use the Satellite Tracker feature to know when |
| Time signals | `RadioClock` | MSF 60 kHz, DCF77 77.5 kHz, TDF 162 kHz, WWVB 60 kHz, JJY 40 kHz | Needs a receiver that covers VLF or LF |
| Inmarsat STD-C | `InmarsatDemod` | 1537.70, 1541.45, 1537.10 MHz | L band, needs a suitable antenna and often an LNA |

## Broadcast

| What | Channel | Frequency | Notes |
|------|---------|-----------|-------|
| Broadcast FM | `BFMDemod` | 87.5 to 108 MHz | Use this rather than `WFMDemod` for broadcast: it decodes stereo and RDS |
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
| POCSAG pagers | `PagerDemod` | Regional, commonly 138 to 160 MHz | FSK with a 4.5 kHz shift |
| End of train devices | `EndOfTrainDemod` | 457.9375 MHz North America and India, 477.7 MHz Australia, 450.2625 MHz New Zealand | |

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
- **The squelch is closed.** `capture_audio` reports the peak level and warns when the audio is
  silent, which usually means squelch rather than a broken setup.
- **Gain is too low or too high.** Too low buries the signal in noise; too high overloads the
  front end. Check the device report and the spectrum.
- **The antenna is wrong for the band.** Nothing in software fixes a 2 metre whip at 1090 MHz.
- **A bias tee is needed** for a powered antenna or LNA, and is off by default.
- **The satellite or balloon is not overhead.** APT, radiosondes and Inmarsat all depend on this.
