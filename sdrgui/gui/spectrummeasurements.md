<h1>Spectrum measurements controls</h1>

This dialog controls which measurements are carried out on the spectrum and what is displayed on the spectrum window.

<h2>No measurements</h2>

![Spectrum Measurements dialog - none](../../doc/img/Spectrum_Measurement_dialog_none.png)

The spectrum is displayed the usual way without the measurements table

<h2>Peaks</h2>

Shows the n largest peaks in magnitude

![Spectrum Measurements dialog - peak](../../doc/img/Spectrum_Measurement_dialog_peak.png)


  - **Display results table**: lets you choose whether the table is displayed at the left, right, top or bottom of the spectrum view. This control is common to all measurement types
  - **Highlight on spectrum**: controls whether the artifacts are displayed or not on the spectrum view. This control is common to all measurement types having artifacts
  - **Results precision**: controls the number of decimal places displayed on the power readings in dB. This control is common to all measurement types
  - **Peaks**: controls the number of peaks

![Spectrum Measurements - peaks](../../doc/img/Spectrum_Measurement_Peak.png)

<h2>Channel Power</h2>

Channel power measures the total power within a user-defined bandwidth. Channel shift is adjustable with the dialog.

![Spectrum Measurements dialog - channel power](../../doc/img/Spectrum_Measurement_dialog_ChannelPower.png)

  - **Results precision**: controls the number of decimal places displayed on the power readings in dB
  - **Center frequency offset**: channel offset from the center in Hz
  - **Channel bandwidth**: bandwidth of the channel in Hz

Spectrum display with the highlight artifact on:

![Spectrum Measurements - channel power](../../doc/img/Spectrum_Measurement_ChannelPower.png)

<h2>Adjacent channel power</h2>

The adjacent channel power measurement measures the power in a channel of user-defined bandwidth and compares it to the power in the left and right adjacent channels. Channel shift and adjacent channels width and separation are also adjustable with the dialog.

![Spectrum Measurements dialog - adjacent channel power](../../doc/img/Spectrum_Measurement_dialog_AdjChannelPower.png)

  - **Center frequency offset**: channels offset from the center in Hz
  - **Channel bandwidth**: bandwidth of the center channel in Hz
  - **Channel spacing**: Center channel to adjacent channel centers spacing in Hz
  - **Adjacent channel bandwidth**: Adjacent channels bandwidth in Hz

Spectrum display with the highlight artifact on:

![Spectrum Measurements - adjacent channel power](../../doc/img/Spectrum_Measurement_AdjChannelPower.png)

The results table displays:

  - **Left power**: power in the left channel in dB
  - **Left ACPR**: difference in power between the left channel and the center channel in dB
  - **Center power**: power in the center channel in dB
  - **Right ACPR**: difference in power between the right channel and the center channel in dB
  - **Right power**: power in the right channel in dB

<h2>Occupied Bandwidth</h2>

The occupied bandwidth measurement measures the bandwidth that contains 99% of the total power.

![Spectrum Measurements dialog - occupied bandwidth](../../doc/img/Spectrum_Measurement_dialog_OccupiedBandwidth.png)

  - **Center frequency offset**: channel offset from the center in Hz
  - **Channel bandwidth**: bandwidth of the channel in Hz, over which the total power should be measured

Spectrum display with the highlight artifact on:

![Spectrum Measurements - occupied bandwidth](../../doc/img/Spectrum_Measurement_OccupiedBandwidth.png)

<h2>3dB Bandwidth</h2>

The 3dB bandwidth measurement measures the bandwidth of a signal, calculated as the frequency difference between where the power falls by 3dB either side of the maximum peak.

![Spectrum Measurements - 3dB bandwidth](../../doc/img/Spectrum_Measurement_3dBBandwidth.png)

<h2>SNR</h2>

The SNR measurement simultaneously calculates SNR, SNFR, THD, THD+N, SINAD and SFDR.

![Spectrum Measurements dialog - SNR](../../doc/img/Spectrum_Measurement_dialog_SNR.png)

 - **Harmonics**: controls the number of harmonics for SNR, THD or THD+N measurements (see next)

Spectrum display with the highlight artifact on:

![Spectrum Measurements - SNR](../../doc/img/Spectrum_Measurement_SNR.png)

The fundamental, harmonics and SFDR are highlighted.

<h3>SNR: Signal to Noise Ratio</h3>

The SNR measurement estimates a signal-to-noise ratio. The fundamental signal is the largest peak (i.e. FFT bin with highest magnitude). The bandwidth of the signal is assumed to be the width of the largest peak, which includes adjacent bins with a monotonically decreasing magnitude. Noise is summed over the full bandwidth (i.e all FFT bins), with the fundamental and user-specified number of harmonics being replaced with the noise median from outside of these regions. The noise median is also subtracted from the signal, before the SNR is calculated.

<h3>SNFR: Signal to Noise Floor Ratio</h3>

The SNFR measurement estimates a signal-to-noise-floor ratio. This is similar to the SNR, except that the noise used in the ratio, is only the median noise value calculated from the noise outside of the fundamental and harmonics, summed over the bandwidth of the signal. One way to think of this, is that it is the SNR if all noise outside of the signal's bandwidth was filtered.

<h3>THD: Total Harmonic Distortion</h3>

THD is measured as per SNR, but the result is the ratio of the total power of the harmonics to the fundamental.

<h3>THD+N: Total Harmonic Distortion Plus Noise</h3>

THD+N is measured as per SNR, but the result is the ratio of the total power of the harmonics and noise to the fundamental.

<h3>SINAD: Signal to Noise and Distortion Ratio</h3>

SINAD is measured as per SNR, but the result is the ratio of the fundamental to the total power of the harmonics and noise.

<h3>SFDR: Spurious Free Dynamic Range</h3>

SFDR is a measurement of the difference in power from the largest peak (the fundamental) to the second largest peak (the strongest spurious signal).

<h2>Specifications</h2>

The measurements table has a Spec column that allows entry of user-defined specifications for the Current, Mean, Min and Max values to be checked against.
For example, you might choose to enter >50 in the SFDR row, to check that SFDR is greater than 50dB.
Each time the specification fails to be met, the value in the Fails column is incremented.
Values out of specification are highlighted in red.
The Spec column supports the following operators: <, <=, >, >= and =.

![Spectrum Measurements - specifications](../../doc/img/Spectrum_Measurement_Specification.png)
