<h1>SSB modulator plugin</h1>

<h2>Introduction</h2>

This plugin can be used to generate a single sideband or double sidebands modulated signal.

<h2>Interface</h2>

The top and bottom bars of the channel window are described [here](../../../sdrgui/channel/readme.md)

![SSB Modulator plugin GUI](../../../doc/img/SSBModulator_plugin.png)

&#9758; In order to toggle USB or LSB mode in SSB mode you have to set the "BW" in channel filter cutoff control (8) to a positive (USB) or negative (LSB) value. The above screenshot shows a USB setup. See the (7) to (9) paragraphs below for details.

&#9758; The channel marker in the main spectrum display shows the actual band received taking in channel filtering into account.

<h3>1: Frequency shift from center frequency of transmission</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of transmission. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

<h3>2: Channel power</h3>

Average total power in dB relative to a +/- 1.0 amplitude signal generated in the pass band.

<h3>3: Binaural mode</h3>

Use this button to toggle between monaural and binaural mode. Monaural is classical single sideband or double sidebands modulation. In binaural mode I and Q samples are taken from the left and right stereo channels (or reversed).

When in monaural mode the icon shows a single loudspeaker and when in binaural mode it shows a pair of loudspeakers.

<h3>4: Reverse left and right channels in binaural mode</h3>

Effective only in binaural mode: reverses left and right audio channels so that the left is connected to Q and the right to the I complex signal channel.

<h3>5: Sideband flip</h3>

Flip LSB/USB. Mirror filter bandwidth around zero frequency and change from LSB to USB or vice versa. Works in SSB mode only.

<h3>6: SSB/DSB</h3>

Selects between SSB and DSB operation. When in SSB mode the icon shows a single sideband spectrum (USB side). When in DSB mode the icon shows a double sideband spectrum. In SSB mode the shape of the icon represents LSB or USB operation.

<h3>7: Spectrum display frequency span</h3>

The transmitted signal in the sideband (SSB) or sidebands (DSB) sample rate of 48 kHz is further decimated by a power of two before being applied to the channel spectrum display and used to set the in channel filter limits. This effectively sets the total available bandwidth depending on the decimation:

  - 1 (no decimation): 24 kHz (SSB) or 48 kHz (DSB)
  - 2: 12 kHz (SSB) or 24 kHz (DSB)
  - 4: 6 kHz (SSB) or 12 kHz (DSB)
  - 8: 3 kHz (SSB) or 6 kHz (DSB)
  - 16: 1.5 kHz (SSB) or 3 kHz (DSB)

The span value display is set as follows depending on the SSB or DSB mode:

  - In SSB mode: the span goes from zero to the upper (USB: positive frequencies) or lower (LSB: negative frequencies) limit and the absolute value of the limit is displayed.
  - In DSB mode: the span goes from the lower to the upper limit of same absolute value and &#177; the absolute value of the limit is displayed.

This is how the Span (7) and bandpass (8, 9) filter controls look like in the 3 possible modes:

**DSB**:

![SSB Demodulator band controls DSB](../../../doc/img/SSBDemod_plugin_dsb.png)

  - Decimation factor is 4 hence span is 6 kHz from -3 to +3 kHz and &#177;3.0k is displayed
  - In channel filter bandwidth is 5.2 kHz from -2.6 to +2.6 kHz and &#177;2.4k is displayed
  - In channel filter "low cut" is disabled and set to 0

**USB**:

![SSB Demodulator band controls USB](../../../doc/img/SSBDemod_plugin_usb.png)

  - Decimation factor is 4 hence span is 3 kHz from 0 to 3 kHz and 3.0k is displayed
  - In channel filter upper cutoff is 2.4 kHz and 2.6k is displayed
  - In channel filter lower cutoff is 0.3 kHz and 0.3k is displayed
  - Hence in channel filter bandwidth is 2.3 kHz

**LSB**:

![SSB Demodulator band controls LSB](../../../doc/img/SSBDemod_plugin_lsb.png)

  - Decimation factor is 4 hence span is 3 kHz from 0 to -3 kHz and -3.0k is displayed
  - In channel filter lower cutoff is -2.6 kHz and -2.6k is displayed
  - In channel filter upper cutoff is -0.3 kHz and -0.3k is displayed
  - Hence in channel filter bandwidth is 2.3 kHz

<h3>8: "BW": In channel bandpass filter cutoff frequency farthest from zero</h3>

Values are expressed in kHz and step is 100 Hz.

  - In SSB mode this is the upper (USB: positive frequencies) or lower (LSB: negative frequencies) cutoff of the in channel single side band bandpass filter. The value triggers LSB mode when negative and USB when positive
  - In DSB mode this is half the bandwidth of the double side band in channel bandpass filter therefore the value is prefixed with the &#177; sign.

<h3>9: "Low cut": In channel bandpass filter cutoff frequency closest to zero</h3>

Values are expressed in kHz and step is 100 Hz.

  - In SSB mode this is the lower cutoff (USB: positive frequencies) or higher cutoff (LSB: negative frequencies) of the in channel single side band bandpass filter.
  - In DSB mode it is inactive and set to zero (double side band filter).

<h3>10: Volume</h3>

This is the volume of the audio signal from 0.0 (mute) to 2.0 (maximum). It can be varied continuously in 0.1 steps using the dial button. The Loudspeaker button is the audio mute toggle.

<h3>11: Level meter in %</h3>

  - top bar (beige): average value
  - bottom bar (brown): instantaneous peak value
  - tip vertical bar (bright red): peak hold value

You should aim at keeping the peak value below 100% using the volume control

<h3>12: Audio compression</h3>

Use this button to toggle audio compression on or off.

<h3>13: Input source control</h3>

![Modulator input source control GUI](../../../doc/img/SSBModulator_plugin_cmp.png)

<h4>13.1: Audio compressor</h4>

Activate/deactivate it for file and audio input only.

<h4>13.2: Audio compressor input gain</h4>

Gain in dB before compression

<h4>13.3: Audio compressor threshold</h4>

Threshold in dB above which compression applies a.k.a. "knee" point. The lower the value the harder is the compression and consequently higher the distortion.

<h4>13.4: Tone input select</h4>

Switches to the tone input. You must switch it off to make other inputs available.

<h4>13.5: Morse keyer input select</h4>

Switches to the Morse keyer input. You must switch it off to make other inputs available.

<h4>13.6: Tone frequency (kHz)</h4>

Adjusts the tone frequency from 0.1 to 2.5 kHz in 0.01 kHz steps

<h4>13.7: Audio input select and select audio input device</h4>

Left click to switch to the audio input. You must switch it off to make other inputs available.

Right click to select audio input device. See [audio management documentation](../../../sdrgui/audio.md) for details.

<h4>13.8: Audio feedback</h4>

Left click to activate audio feedback.

Right click to select audio output device for audio feedback. See [audio management documentation](../../../sdrgui/audio.md) for details.

<h3>14: CW (Morse) text</h3>

Enter the text to be keyed when Morse input is active and in text mode

<h3>15: Clear CW text</h3>

Clears the CW (Morse) text

<h3>16: Morse keyer controls</h3>

![Morse keyer control GUI1](../../../doc/img/ModCWControls1.png)

<h4>16.1: CW keying speed</h4>

Sets the CW speed in Words Per Minute (WPM). This is based on the word "PARIS" sent 5 times. For 5 WPM the dot length is 240 ms. In other terms the dot length is calculated as 1.2 / WPM seconds. The dot length is used as the base to compute other timings:

  - Element (dot or dash) silence separator: 1 dot length
  - Dash: 3 dot lengths
  - Character silence separator: 3 dot lengths
  - Word silence separator: 7 dot lengths

<h4>16.2: Dots keying</h4>

Switch this button to send dots continuously

<h4>16.3: Dashes keying</h4>

Switch this button to send dashes continuously

<h4>16.4: Text keying</h4>

Switch this button to send the text typed into the text box (13)

<h4>16.5: Text auto repeat</h4>

Switch this button to auto repeat the text keying

<h4>16.6: Text play/stop</h4>

Use this button to stop sending text. When resuming keying restarts at the start of text

![Morse keyer control GUI2](../../../doc/img/ModCWControls2.png)

&#9888; WARNING: what follows is not really useful if you do not use a proper Morse keyer with direct audio feedback. There is a significant audio delay either with the direct monitoring or by monitoring the transmitted signal so keying with this audio as feedback is not practical

16.7: Activate morse keys keyboard control

This disables text or continuous dots or dashes. Toggle input from keyboard. Occasionally the focus may get lost and you will have to deactivate and reactivate it to recover the key bindings.

16.8: Iambic or straight

Choose iambic or straight keying style. When straight is selected the dot or dash key may be used.

16.9: Register dot key

Click on the button and while selected type a character or character and modifier (Shift + key for example) to select which key is used for dots. The key or key sequence appears next (here dot `.`)

16.10: Register dash key

Click on the button and while selected type a character or character and modifier (Shift + key for example) to select which key is used for dashes. The key or key sequence appears next (here dot `.`)

<h3>17: Audio file path</h3>

The path to the selected audio file to be played or dots if unselected

<h3>18: Audio file play controls</h3>

![Modulator audio file play control GUI](../../../doc/img/ModFileControls.png)

<h4>18.1: Audio file select</h4>

Opens a file dialog to select the audio file to be played. It must be 48 kHz F32LE raw format. If binaural mode is selected it takes a 2 channel (stereo) file else it should be mono.
Using sox a .wav file can be converted with this command: `sox piano.wav -t raw -r 48k -c 1 -b 32 -L -e float piano.raw` (mono) or `sox piano.wav -t raw -r 48k -c 2 -b 32 -L -e float piano.raw` (stereo)

<h4>18.2: Audio file loop</h4>

Audio replay file at the end

<h4>18.3: Play/pause file play</h4>

Toggle play/pause file play. When paused the slider below (20) can be used to randomly set the position in the file when re-starting.

<h3>19: Play file current position</h3>

This is the current audio file play position in time units relative to the start

<h3>20: Play file length</h3>

This is the audio file play length in time units

<h3>21: Play file position slider</h3>

This slider can be used to randomly set the current position in the file when file play is in pause state (button 17.3)

<h3>22: Channel spectrum display</h3>

This is the channel spectrum display. Details on the spectrum view and controls can be found [here](../../../sdrgui/gui/spectrum.md)
