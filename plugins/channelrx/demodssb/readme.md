<h1>SSB/DSB demodulator plugin</h1>

<h2>Introduction</h2>

This plugin can be used to listen to a single sideband or double sidebands modulated signals. 

<h2>Interface</h2>

![SSB Demodulator plugin GUI](../../../doc/img/SSBDemod_plugin.png)

<h3>1: Frequency shift from center frequency of reception direction</h3>

The "+/-" button on the left side toggles between positive and negative shift.

<h3>2: Frequency shift from center frequency of reception value</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors at the digit position.

<h3>3: Channel power</h3>

Average total power in dB relative to a +/- 1.0 amplitude signal received in the pass band.

<h3>4: Monaural/binaural toggle</h3>

  - Monaural: the scalar signal is routed to both left and riht audio channels
  - Binaural: the complex signal is fed with the real part on the left channel and the imaginary part to the rigth channel
  
<h3>5: Invert left and right channels</h3>

Inverts left and right audio channels. Useful in binaural mode only.

<h3>6: SSB/DSB demodulation</h3>

Toggles between SSB (icon with one sideband signal) and DSB (incon with double sidebands signal)

<h3>7: Level meter in dB</h3>

  - top bar (green): average value
  - bottom bar (blue green): instantaneous peak value
  - tip vertical bar (bright green): peak hold value

<h3>8: Span</h3>

Signal span (i.e. sample rate) befor demodulation

<h3>9: Bandwidth</h3>

This is the upper limit in kHz of the signal filtered after demodulation. 

<h3>10: Bandwidth</h3>

SSB only: this is the lower limit in kHz of the signal filtered after demodulation. 

<h3>11: Volume</h3>

This is the volume of the audio signal from 0.0 (mute) to 10.0 (maximum). It can be varied continuously in 0.1 steps using the dial button.

<h3>12: Audio mute</h3>

Use this button to toggle audio mute for this channel. The button will light up in green if the squelch is open. This helps identifying which channels are active in a multi-channel configuration.

<h3>13: Spectrum display</h3>

This is the spectrum display of the demodulated signal (SSB) or translated signal (DSB)