<h1>ATV demodulator plugin</h1>

<h2>Introduction</h2>

This plugin can be used to view amateur analog television transmissions a.k.a ATV. The transmitted video signal can be black and white or color (PAL, NTSC) but only the black and white level (luminance) is retained and hence image is black and white. There is no provision to demodulate the audio subcarrier either. The modulation can be either AM or FM (SSB with carrier is only experimental). A plugin supporting audio can be used in the same passband to demodulate an audio carrier but this does not work for a subcarrier which excludes FM.

An optional rational downsampler with lowpass filtering can be used to set the channel sample rate to the 0.5 MS/s multiple which is the closest to the source sample rate. When the downsampler is not engaged the source feeds the channel directly and thus the source sample rate is used. A standard image quality for PAL standard modes requires a sample rate of at least 4 MS/s. The Airspy Mini 3 MS/s mode may still be acceptable. 

Experimental modes with smaller number of lines and FPS values can be used in conjunction with the [ATV Modulator plugin](https://github.com/f4exb/sdrangel/tree/master/plugins/channeltx/modatv) to reduce sample rate and occupied bandwidth. Acceptable images (shown in the screenshots here) can be obtained in FM with just 1.3 MHz bandwidth.

<h2>Interface</h2>

![ATV Demodulator plugin GUI](../../../doc/img/ATVDemod_plugin.png)

The interface is divided into three collapsable sections:

  - A: the RF settings
  - B: the video settings
  - C: the video monitor and scope in a tabbed panel arrangement

Each part is detailed next

<h2>A: RF settings</h2>

![ATV Demodulator plugin GUI RF](../../../doc/img/ATVDemod_pluginA.png)

<h3>1: Frequency shift from center frequency of reception direction</h3>

The "+/-" button on the left side of the dial toggles between positive and negative shift.

<h3>2: Frequency shift from center frequency of reception value</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position.

<h3>3: Rational downsampler toggle</h3>

Use this toggle button to enable or disable the rational downsampler. 

Without downsampling the sample rate given by the source plugin is directly applied to the channel.

When the downsampler is engaged the channel sample rate is the closest multiple of 0.5 MS/s below the source sample rate. e.g for a source sample rate of 1.6 MS/s this will be 1.5 MS/s. If a non null sample rate cannot be obtained the decimator is disabled and the source sample rate is used instead. 

When the downsampler is engaged the signal is lowpass filtered and the cutoff frequency can be adjusted with the in band filter cutoff slider (13). This works also when the decimation ratio is 1.0 e.g source sample rate is an exact multiple of 0.5 MS/s.

<h3>4: Channel sample rate</h3>

This is the channel sample rate in kS/s possibly downsampled from source when rational downsampler is engaged (3).

<h3>5: Number of points (or samples) per line</h3>

This is the number of points or samples per complete line including sync and padding. This is derived from the sample rate and line frequency as the ratio of the two. For example with a 625 lines &#215; 25 FPS signal the line frequency is 15625 Hz. If the channel sample rate is 1500 kS/s this yields 1500000/15625 = 96 points. If the ratio is not an integer then the integer part is taken.

Picture definition depends largely on this number and the larger the better but it is useless to have a much greater number than the number of points per line used in transmission.

<h3>6: BFO PLL lock indicator</h3>

&#9888; this is experimental.

When single sideband demodulation is selected (USB, LSB) the BFO is phased locked to the carrier. This indicator turns green if the PLL is locked.

<h3>7: BFO frequency adjustment</h3>

&#9888; this is experimental.

This allows adjstment of BFO frequency in 10 Hz steps from -5 to +5 kHz. You will have to look for the right value to lock to the carrier. See (6) for the lock indicator.

The BFO base frequency in Hz appears on the right. Actual frequency may change acoording to PLL locking to the carrier.

<h3>8: Channel power</h3>

Average total power in dB relative to a &#177;1.0 amplitude signal generated in the pass band.

<h3>9: Modulation</h3>

  - FM1: this is Frequency Modulation with approximative demodulation algorithm not using atan2
  - FM2: this is Frequency Modulation with less approximative demodulation algorithm still not using atan2
  - FM3: this is Frequency Modulation with atan2 approximation for phase calculation and then a discrete differentiation is applied
  - AM: this is Amplitude Modulation
  - USB: &#9888; USB demodulation synchronous to the carrier
  - LSB: &#9888; LSB demodulation synchronous to the carrier
  
For FM choose the algorithm that best suits your conditions. &#9888; only FM3 is accurate with regard to FM deviation (see 10).

&#9888; USB and LSB modes are experimental and do not show good results for present standards sample rates.

<h3>10: FM deviation adjustment</h3>

Using this button you can adjust the nominal FM deviation as a percentage of the channel bandwidth that is displayed on the right of the button. When a signal with this deviation is received the demodulated signal is in the range -0.5/+0.5 which is shifted to a 0/1 range video signal.

&#9888; The value is accurate only with the atan2 differential demodulator i.e. FM3. With FM1 and FM2 you will have to adjust it for best image results. You can use the scope as an aid to try to fit the video signal in the 0/1 range.   

<h3>11: FFT asymmetrical filter toggle</h3>

Use this button to enable/disable the FFT asymmetrical filter.

<h3>12: FFT asymmetrical filter opposite band cutoff frequency</h3>

For all modulations except LSB this is the lower side band.

This slider lets you adjust the opposite band cutoff frequency of the FFT asymmetrical filter. The value in kHz appears on the left of the slider.

<h3>13: FFT asymmetrical filter in band cutoff frequency</h3>

For all modulations except LSB this is the upper side band.

This slider lets you adjust the in band cutoff frequency of the FFT asymmetrical filter. The value in kHz appears on the left of the slider.

If the rational downsampler is engaged (3) this slider also controls the downsampler cutoff frequency.

<h2>B: Video settings</h2>

![ATV Demodulator plugin GUI Video settings](../../../doc/img/ATVDemod_pluginB.png)

<h3>1: Nominal number of lines</h3>

This is the total number of lines including all possible synchronization signals.

Choice is between 625, 525 and 405 lines. The number of image lines depends on the synchronization scheme.

<h3>2: Frames Per Second</h3>

This combo lets you chose between a 30, 25, 20 and 16 FPS. This is the resulting FPS. In interleaved modes the half frame rate is doubled.

<h3>3: Synchronization standard</h3>

This combo lets you set the TV standard type. This sets the number of lines per complete image, frame synchronization parameters and number of blank (black) lines. Choice is between:

  - PAL625L: this is based on the classical 625 lines PAL system. It uses 7 or 8 synchronization lines depending on the half frame (field). It has also 17 black lines on the top of each half frame.
  - PAL525L: the only difference with PAL625L is the number of black lines which is down to 15
  - 405L: this is not the British standard. It just follows the same scheme as the two above but with only 7 black lines per half frame

When the standard chosen matches the standard of transmission the image should appear in full size and proper aspect ratio.

<h3>4: Horizontal sync</h3>

Check/uncheck this box to toggle horizontal synchronization processing.

<h3>5: Vertical sync</h3>

Check/uncheck this box to toggle vertical synchronization processing.

<h3>6: Invert video</h3>

Check/uncheck this box to toggle video signal inversion. This does not work well in AM for now.

<h3>7: Half frames</h3>

Check this box to render only half of the frames for slow processors.

<h3>8: Reset defaults</h3>

Use this push button to reset values to a standard setting:

  - FM1 modulation
  - 625 lines
  - 25 FPS
  - PAL 625L standard
  - Horizontal and vertical syncs active
  - No video inversion
  - Interlacing
  - 100 mV sync level
  - 310 mV black level
  - 64 microsecond line length (middle)
  - 4.7 microsecond sync pulse length (middle)
  
<h3>9: Synchronization level</h3>

Use this slider to adjust the top level of the synchronization pulse on a 0 to 1V scale. The value in mV appears on the right of the slider. Nominal value: 100 mV.

<h3>10: Black level</h3>

Use this slider to adjust the black level of the video signal on a 0 to 1V scale. The value in mV appears on the right of the slider. Nominal value: 310 mV.

<h3>11: Line length</h3>

This is the line length in time units. The value appears on the right of the slider. Nominal value depends on the nominal line frequency. For example with 405 lines and 20 FPS. The line frequency is 405 &#215; 20 = 8100 Hz thus the nominal line time is the inverse of this value that is &#8776;123.45 &mu;s 

The slider step is set to a sample period in order to ensure that the adjustment is done with the best possible precision. For example at 1500 kS/s sample rate this will be the inverse of this value that is &#8776;666.67 ns. The middle position of the slider sets the nominal value and the slider step appears in the tooltip.

<h3>12: Horizontal synchronization pulse length</h3>

This is the length in time units of a horizontal or line synchronization pulse. The value appears on the right of the slider. Nominal value depends on the nominal line length as described above. The nominal pulse length is derived from the 4.7 &mu;s pulse of a 625 lines standard system with a 64 &mu;s line length. For example with a 405 lines &#215; 20 FPS transmission that has a line length of &#8776;123.45 &mu;s this is (4.7 / 64) &#215; 123.45 &#8776; 9.07 &mu;s. In practice you will adjust it to a slightly smaller value to be able to synchronize.

Similarly to the line length slider the slider step is set to a sample period in order to ensure that the adjustment is done with the best possible precision. The middle position of the slider sets the nominal value and the slider step appears in the tooltip.

<h2>C: Image</h2>

<h3>Monitor</h3>

Select monitor with the monitor tab on the left side.

![ATV Demodulator plugin GUI Video monitor](../../../doc/img/ATVDemod_pluginC_monitor.png)

This is where the TV image appears. Yes on the screenshot this is the famous [Lenna](https://en.wikipedia.org/wiki/Lenna). The original image is 512 &#215; 512 pixels so it has been cropped to fit the 4:3 format. The screen geometry ratio is fixed to 4:3 format. You will have to choose the standard (B.3) matching the transmission to ensure that the transmitted image fits perfectly. 

<h3>Scope</h3>

Select scope with the scope tab on the left side.

![ATV Demodulator plugin GUI Video scope](../../../doc/img/ATVDemod_pluginC_scope.png)

This is a scope widget fed with the video signal. Controls of the scope are the same as with the ChannelAnalyzerNG plugin. Please refer to the [source folder](https://github.com/f4exb/sdrangel/tree/master/plugins/channelrx/chanalyzerng) of this plugin for more details.

Note that the video signal is a real signal so the imaginary part is always null. Therefore only the "Real" mode for both the trace and the trigger is interesting.
