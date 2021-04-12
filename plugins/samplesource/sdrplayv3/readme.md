<h1>SDRplay plugin</h1>

<h2>Introduction</h2>

This plugin supports input from SDRplay RSP devices using V3 of SDRplay's API, including RSP1, RSP1A, RSP2, RSPduo and RSPdx.

<h2>Driver Prerequisites</h2>

This plugin requires the SDRplay API V3.07 to have been installed and for the service to be running. It can be downloaded for Windows, Linux and Mac from: https://www.sdrplay.com/softwarehome/

<h2>Interface</h2>

![SDRplay v3 plugin GUI](../../../doc/img/SDRPlayV3_plugin.png)

<h3>1: Start/Stop</h3>

Device start / stop button.

  - Blue triangle icon: device is ready and can be started
  - Green square icon: device is running and can be stopped
  - Magenta (or pink) square icon: an error occurred. In the case the device was accidentally disconnected you may click on the icon to stop, plug back in, check the source on the sampling devices control panel and start again.

<h3>2: Sample rate</h3>

This is the sample rate at which IQ samples are transfered from the device to SDRangel, in kS/s (k).

<h3>3: Center frequency</h3>

This is the center frequency of reception in kHz.

<h3>4: Local oscillator frequency correction</h3>

This is the correction in tenths of ppm steps.

<h3>5: Tuner selection</h3>

Selects which tuner is used for input. For RSPduo this can be 1 or 2. Other RSP devices only have a single tuner.

<h3>6: Antenna port</h3>

Selects which antenna port is used. The antenna ports available depend upon the RSP device and tuner selected.

<h3>7: DC offset correction</h3>

Check this button to enable DC offset correction.

<h3>8: IQ imbalance correction</h3>

Check this button to enable IQ imbalance correction.

<h3>9: External reference clock output</h3>

Enable reference clock output. This is only available for RSP2 and DSPduo.

<h3>10: Bias tee</h3>

Enable bias tee. This is only available for RSP1A, RSP2, RSPduo tuner 2 and RSPdx.

<h3>11: AM notch filter</h3>

Enable AM notch filter. This is only available for RSPduo tuner 1.

<h3>12: MW/FM notch filter</h3>

Enable MW/FM notch filter. This is only available for RSP1A, RSP2, RSPduo and RSPdx.

<h3>13: DAB notch filter</h3>

Enable DAB notch filter. This is only available for RSP1A, RSPduo and RSPdx.

<h3>14: IF bandwidth</h3>

This selects the IF filter bandwidth. The following bandwidths are available:

  - 200 kHz
  - 300 kHz
  - 600 kHz
  - 1536 kHz
  - 5000 kHz
  - 6000 kHz
  - 7000 kHz
  - 8000 kHz

<h3>15: IF frequency</h3>

This selects the IF frequency between these values:

  - 0 for zero IF
  - 450 kHz: you have to set sample rate to 1792 kHz (16) and use decimation (17) with an infradyne position (18)
  - 1620 kHz: you have to set sample rate to 6400 kHz (16) and use decimation (17) with an infradyne position (18)
  - 2048 kHz: you have to set sample rate to 8192 kHz (16) and use decimation (17) with an infradyne position (18)

<h3>16: Sample rate</h3>

Sets the ADC IQ sample rats from 2M to 10.66M Hz.

<h3>17: Decimation</h3>

Decimation in powers of two from 1 (no decimation) to 64.

<h3>18: Decimated bandpass center frequency position relative the SDRplay center frequency</h3>

  - **Cen**: the decimation operation takes place around the SDRplay center frequency Fs.
  - **Inf**: the decimation operation takes place around Fs - Fc.
  - **Sup**: the decimation operation takes place around Fs + Fc.

With SR as the sample rate before decimation Fc is calculated as:

  - if decimation n is 4 or lower:  Fc = SR/2^(log2(n)-1). The device center frequency is on the side of the baseband. You need a RF filter bandwidth at least twice the baseband.
  - if decimation n is 8 or higher: Fc = SR/n. The device center frequency is half the baseband away from the side of the baseband. You need a RF filter bandwidth at least 3 times the baseband.

<h3>19. RF gain setting</h3>

Sets the LNA and mixer gain dB. The settings available depended upon the RSP device and frequency band.

<h3>20. IF AGC</h3>

Check this button to enable IF automatic gain control.

<h3>21. IF gain</h3>

Manual IF gain from 0 to -59 dB. Only enabled when IF AGC is disabled.
