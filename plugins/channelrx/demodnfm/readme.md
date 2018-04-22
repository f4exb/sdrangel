<h1>NFM demodulator plugin</h1>

<h2>Introduction</h2>

This plugin can be used to listen to a narrowband FM modulated signal. "Narrowband" means that the bandwidth can vary from 5 to 40 kHz.

<h2>Interface</h2>

![NFM Demodulator plugin GUI](../../../doc/img/NFMdemod_plugin.png)

<h3>1: Frequency shift from center frequency of reception value</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2. Left click on a digit sets the cursor position at this digit. 

<h3>2: Channel power</h3>

Average total power in dB relative to a +/- 1.0 amplitude signal received in the pass band.

<h3>3: Level meter in dB</h3>

  - top bar (green): average value
  - bottom bar (blue green): instantaneous peak value
  - tip vertical bar (bright green): peak hold value

<h3>4: RF bandwidth</h3>

This is the bandwidth in kHz of the channel signal before demodulation. It can be set in steps as 5, 6.25, 8.33, 10, 12.5, 15, 20, 25 and 40 kHz. The expected one side frequency deviation is 0.4 times the bandwidth.

&#9758; The demodulation is done at the channel sample rate which is guaranteed not to be lower than the requested audio sample rate but can possibly be equal to it. This means that for correct operation in any case you must ensure that the sample rate of the audio device is not lower than the Nyquist rate required to process this channel bandwidth. 

&#9758; The channel sample rate is always the baseband signal rate divided by an integer power of two so depending on the baseband sample rate obtained from the sampling device you could also guarantee a minimal channel bandwidth. For example with a 125 kS/s baseband sample rate and a 8 kS/s audio sample rate the channel sample rate cannot be lower than 125/8 = 15.625 kS/s (125/16 = 7.8125 kS/s is too small) which is still OK for 5 or 6.25 kHz channel bandwidths.

<h3>5: AF bandwidth</h3>

This is the bandwidth of the audio signal in kHz (i.e. after demodulation). It can be set in continuous kHz steps from 1 to 20 kHz. 

<h3>6: Volume</h3>

This is the volume of the audio signal from 0.0 (mute) to 4.0 (maximum). It can be varied continuously in 0.1 steps using the dial button.

<h3>7: Delta/Level squelch</h3>

Use this button to toggle between AF (on) and RF power (off) based squelch.

<h3>8: Squelch threshold</h3>

<h4>Power threshold mode</h4>

Case when the delta/Level squelch control (7) is off (power). This is the squelch threshold in dB. The average total power received in the signal bandwidth before demodulation is compared to this value and the squelch input is open above this value. It can be varied continuously in 0.1 dB steps from 0.0 to -100.0 dB using the dial button.

<h4>Audio frequency delta mode</h4>

Case when the delta/Level squelch control (7) is on (delta). In this mode the squelch compares the power of the demodulated audio signal in a low frequency band and a high frequency band. In the absence of signal the discriminator response is nearly flat and the power in the two bands is more or less balanced. In the presence of a signal the lower band will receive more power than the higher band. The squelch does the ratio of both powers and the squelch is opened if this ratio is lower than the threshold given in percent. 

A ratio of 1 (100%) will always open the squelch and a ratio of 0 will always close it. The value can be varied to detect more distorted and thus weak signals towards the higher values. The button rotation runs from higher to lower as you turn it clockwise thus giving the same feel as in power mode. The best ratio for a standard NFM transmission is ~40%.

The distinct advantage of this type of squelch is that it guarantees the quality level of the audio signal (optimized for voice) thus remaining closed for too noisy signals received on marginal conditions or bursts of noise independently of the signal power.

&#9758; The signal used is the one before AF filtering and the bands are centered around 1000 Hz for the lower band and 6000 Hz for the higher band. This means that it will not work if your audio device runs at 8000 or 11025 Hz. You will need at least a 16000 Hz sample rate. Choose power squelch for lower audio rates.

&#9758; The chosen bands around 1000 and 6000 Hz are optimized for standard voice signals in the 300-3000 Hz range.

<h3>9: Squelch gate</h3>

This is the squelch gate in milliseconds. The squelch input must be open for this amount of time before the squelch actually opens. This prevents the opening of the squelch by parasitic transients. It can be varied continuously in 10ms steps from 10 to 500ms using the dial button.

<h3>10: CTCSS on/off</h3>

Use the checkbox to toggle CTCSS activation. When activated it will look for a tone squelch in the demodulated signal and display its frequency (see 10). 

<h3>11: CTCSS tone</h3>

This is the tone squelch in Hz. It can be selected using the toolbox among the usual CTCSS values and `--` for none. When a value is given and the CTCSS is activated the squelch will open only for signals with this tone squelch.

<h3>12: CTCSS tone value</h3>

This is the value of the tone squelch received when the CTCSS is activated. It displays `--` if the CTCSS system is de-activated.

<h3>13: Audio mute and audio output select</h3>

Left click on this button to toggle audio mute for this channel. The button will light up in green if the squelch is open. This helps identifying which channels are active in a multi-channel configuration.

If you right click on it it will open a dialog to select the audio output device. See [audio management documentation](../../../sdrgui/audio.md) for details.
