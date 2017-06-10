<h1>Channel Analyzer (New Generation) plugin</h1>

<h2>A. Introduction</h2>

This is an enhancement of the Channel Analyzer plugin with a new scope display hence the "New Generation" or "NG" qualifier.

This plugin can be used to analyze the complex signal received in its passband. You can visualize the spectrum (Channel Spectrum section) or waveforms related to the complex signal (Channel Scope section). These waveforms can be:

  - Real part
  - Imaginary part
  - Magnitude linear
  - Power i.e. squared magnitude log (dB)
  - Phase
  - Phase derivative (instant frequency)
  
The same waveforms can be used to trigger the scope trace

<h2>B. General interface</h2>

![Channel Analyzer NG plugin GUI](../../../doc/img/ChAnalyzerNG_plugin.png)

The interface is essentially divided in the following sections

  1. Channel controls
  2. Scope view (Channel scope)
  3. Scope global controls
  4. Scope trace control
  5. Scope trigger control
  
Note: the spectrum view (Channel spectrum) is not presented here.  

<h2>C. Channel controls</h2>

![Channel Analyzer NG plugin controls](../../../doc/img/ChAnalyzerNG_plugin_settings.png)

<h3>1: Frequency shift from center frequency of reception</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arroews. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2.

<h3>3: Toggle the rational downsampler</h3>

The input channel sample rate is given by the source device sample rate possibly downsampled by a power of two in the source device plugin. This input sample rate can be optionnaly downsampled to any value using a rational downsampler. This allows a precise control of the timings independently of the source plugin sample rate. Some devices are flexible on their sample rate some like the Airspy are not.

<h3>4: Rational downsampler output rate</h3>

Use the wheels to adjust the sample rate that will be used in the rest of the signal processing in the channel. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. The minimum value is 2000 S/s and the maximum value is the source plugin output sample rate. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arroews. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2.

<h3>5: Downsampler by a power of two</h3>

This combo can select a further downsampling by a power of two. This downsampling applies on the signal coming either directly from the source plugin when the rational downsampler is disabled or from the output of the rational downsampler if it is engaged.

<h3>6: Processing sample rate</h3>

This is the resulting sample rate that will be used by the spectrum and scope visualizations

<h3>7. Channel power</h3>

Average total power in dB relative to a +/- 1.0 amplitude signal received in the pass band.

<h3>8. Select bandpass filter cut-off frequency</h3>

This slider controls the cut-off frequency of the bandpass filter (or lowpass filter in SSB mode - see next) applied before the signal is sent to the spectrum and scope

<h3>9. Bandpass filter cut-off frequency</h3>

This is the cut-off frequency of the bandpass filter in kHz

<h3>10. SSB filtering</h3>

When this toggle is engaged the signal is filtered either above (USB) or below (LSB) the channel center frequency. The bandpass filter now acts as a lowpass filter and the highpass filter gets active. The sideband is selected upon the sign of the bandpass filter: if positive the USB is selected else the LSB. 

When SSB is off the lowpass filter is not active and the lowpass filter is actually a bandpass filter around the channel center frequency. 

<h3>11. Select highpass filter cut-off frequency</h3>

When the SSB feature is engaged this controls the cut-off frequency of the highpass filter

<h3>12. Hghpass filter cut-off frequency</h3>

This is the cut-off frequency of the highpass filter in kHz

<h2>D. Scope global controls line</h2>

![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_scope1.png)

<h3>1. X only display</h3>

![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_x.png)

Scope displays are categorized in the X display for trace index 0 and Y display for traces with indexes > 0. Thus the trace index 0 is named the "X" trace and traces with index > 0 are named "Yn" traces where n is the trace index thus Y1, Y2, Y3, Y4 as there is a maximum of 5 traces.

This button selects the "X" trace display only on the whole surface of the scope screen

<h3>2. Y only display</h3>

![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_y.png)

This button selects the display of all "Yn" traces on the whole surface of the scope screen

<h3>3. X and Y displays horizontally arranged</h3>

![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_xyh.png)

This button selects the display of "X" and "Yn" traces side by side with "X" trace on the left

<h3>4. X and Y displays vertically arranged</h3>

![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_xyv.png)

This button selects the display of "X" and "Yn" traces on top of each other with "X" trace on the top

<h3>5. All traces and polar display</h3>

![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_polar.png)

This button selects the display of all traces on the left side of the screen and the polar display of the "Yn" traces using the "X" trace as the x coordinates.

<h3>6. Select trace intensity</h3>

This button lets you adjust the traces intensity. The value in percent of the maximum intensity appears as a tooltip

<h3>7. Select grid intensity</h3>

This button lets you adjust the grid intensity. The value in percent of the maximum intensity appears as a tooltip

<h3>8. Displayed trace length</h3>

This slider lets you adjust the length of the traces on display. Each step further divides the length of the full trace controlled by (10). The duration of the full length shown on display appears on the left of the slider and the corresponding number of samples appears as a tooltip. 

<h3>9. Trace offset</h3>

This slider lets you move the start of traces on display. Each step moves the trace by an amount of time corresponding to 1/100 of the length of the full trace controlled by (10). The time offset from the start of the traces appears on the left of the slider and the corresponding number of samples appears as a tooltip. 

<h3>10. Trace length</h3>

This slider lets you control the full length of the trace. Each step increases the corresponding amount of samples by 4800 samples with a minimum of 4800 samples and a maximum of 20*4800 = 96000 samples. The duration of a full trace appears on the left of the slider and he corresponding number of samples appears as a tooltip.

<h3>11. Trace sample rate</h3>

This is the sample rate used by the scope and corresponds to the final sample rate after the whole decimation chain. It should be the same amount as the one displayed on the plugin control (C.6)

<h2>E. Scope trace control line</h2>

![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_scope2.png)

Traces are arranged as the X trace for trace index 0 and Y traces for traces with indexes > 0. The Y traces are suffixed by a trace number corresponding to the trace index thus we have Y1, Y2, Y3 and Y4 traces as there is a maximum of 5 traces. In the polar display the X trace is used for the x coordinates and the Y traces for y coordinates hence their name.

<h3>1.Select trace</h3>

This button lets you select which trace is affected by the controls. The trace name appears on the left of the button. The Y scale and the top right overlay on the displays also correspond to the selected trace (4).

<h3>2. Add/delete trace</h3>

The top "+" button lets you add a new trace. The trace controls are copied from the current selected trace. The bottom "-" button deletes the currently selected trace. You can never delete the first trace (X trace). Use the move buttons (see next) when you want to change the X trace.

<h3>3. Move trace</h3>

The top button with the up arrow lets you move the currently selected trace up by one index position. Thus the trace at the next position takes the current position and the trace at the current position takes the next position. This movement can wrap around so when the last trace is selected and moved up it will take the place of the first trace which is the X trace.

The bottom button with the down arrow lets you move the currently selected trace down by one index position. Thus the trace at the position below takes the current position and the trace at the current position takes the position below. This movement does not wrap around so the first trace (the X trace) cannot be moved down.

<h3>4. Projection selector</h3>

To construct a trace which represents real values the incoming complex signal must be "projected" on the real axis. This combo lets you select the projection scheme:

  - Real: take the real part
  - Imag: take the imaginary part
  - Mag: calculate magnitude in linear representation. This is just the module of the complex sample
  - MagDB: calculate power in log representation as 10*log10(x) or decibel (dB) representation. This is the squared module of the complex sample expressed in decibels
  - Phi: instantaneous phase. This is the argument of the comlpex sample.
  - dPhi: instantaneous derivative of the phase. This is the difference of arguments between successive samples thus represents the instantaneous frequency.

in the MagDB mode when the trace is selected (1) the display overlay on the top right of the trace shows 3 figures. From left to right: peak power in dB, average power in dB and peak to average difference in dB.
  
![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_overlay_dB.png)

<h3>5. Source select</h3>

This is for future use when more than one incoming complex signals can be applied. The signal index appears on the right of the button

<h3>6. Amplitude adjustment</h3>

This slider lets you adjust the amplitude scale. The full scale value appears on the left of the slider. The unit depends on the projection.

<h3>7. Offset adjustement</h3>

This pair of sliders let you offset the trace vertically. The offset value from reference appears on the left of the slider. The reference is either:

  - central zero value for Real, Imag, Phi and dPhi projections
  - bottom zero value for MagLin projection
  - bottom -200 dB value for MagDB projection
  
The top slider is a coarse adjustement. Each step moves the trace by an amount that depends on the projection type:

  - Real, Imag: 0.01
  - Mag: 0.005
  - MagDB: 1 dB
  - Phi, dPhi: 0.01

The bottom slider is a fine adjustment. Each step moves the trace by an amount that depends on the projection type:

  - Real, Imag: 50.0E-6
  - Mag: 25.0sE-6
  - MagDB: 0.01 dB
  - Phi, dPhi: 50.0E-6

<h3>8. Trace delay adjustment</h3>

This pair of sliders let you control the time offset of the trace from the global starting point. The time offset value appears on the left of the slider and the corresponding number of samples appears as a tooltip.

The top slider is a coarse adjustement. Each step moves trace by 100 samples

The bottom slider is a fine adjustment. Each step moves trace by 1 sample

<h3>9. Trace display enable</h3>

By default the trace display is enabled and this checkbox is checked. You can optionnally "mute" the trace by unchecking this checkbox.

<h3>10. Trace color</h3>

This area shows the current trace color. When clicking on it a color chooser dialog appears that lets you change the color of the current trace

<h3>11. Memory select</h3>

The last 15 traces are stored in memory and this button lets you browse through traces in memory. The memory index appears on the left of the button. Traces in memory are sorted from latest (1) to oldest (15). The memory index 0 is the current live trace. When indexes > 0 are selected the live trace is suspended.

It is the complex signal that is memorized actually so when a trace in memory is selected you can still use the global and trace controls to change the display. In particular the projection mode and the number of traces can be changed. Only the full trace length cannot be modified. When in memory mode the triggers are disabled since they only apply to a live trace.

<h2>F. Trigger control line</h2>
 
![Channel Analyzer NG plugin scope1 controls](../../../doc/img/ChAnalyzerNG_plugin_scope3.png)

<h3>1. Select trigger</h3>

This button lets you select which triger condition is affected by the controls. The trigger index appears on the left of the button.

Up to 10 triggers (index 0..9) can be chained to give the final trigger top. The first trigger condition (index 0) is tested then if the trigger is raised the next trigger condition (index 1) is activated then when trigger is raised it passes control to the next trigger etc... until the last trigger is raised then the trace process starts. This established to point in time of the trigger. Optionnally the trace can start some time before this point (this is pre-trigger - see 11)

<h3>2. Add/delete trigger</h3>

The top "+" button lets you add a new trigger condition. The trigger condition details are copied from the current selected trigger. The bottom "-" button deletes the currently selected trigger. You can never delete the first trigger (index 0).

<h3>3. Move trigger</h3>

The top button with the up arrow lets you move the currently selected trigger up by one index position. Thus the trigger at the next position takes the current position and the trigger at the current position takes the next position. This movement can wrap around so when the last trigger is selected and moved up it will take the place of the first trigger at index 0.

The bottom button with the down arrow lets you move the currently selected trigger down by one index position. Thus the trigger at the position below takes the current position and the trigger at the current position takes the position below. This movement does not wrap around so the first trigger (at index 0) cannot be moved down.

<h3>4. Projection selector</h3>

The trigger tests a threshold from real values therefore the incoming complex signal must be "projected" on the real axis before testing. The projection schemes are the same used in for calculating the traces (E.4):

Real: take the real part
Imag: take the imaginary part
Mag: calculate magnitude in linear representation. This is just the module of the complex sample
MagDB: calculate power in log representation as 10*log10(x) or decibel (dB) representation. This is the squared module of the complex sample expressed in decibels
Phi: instantaneous phase. This is the argument of the comlpex sample.
dPhi: instantaneous derivative of the phase. This is the difference of arguments between successive samples thus represents the instantaneous frequency.

<h3>5. Trigger repetition</h3>

This number of trigger conditions must be met before the actual trigger is raised.

<h3>6. Positive edge select</h3>

This button selects the positive edge triggering. Trigger is raised only of the current sample is above threshold and the previous sample is below threshold

<h3>7. Positive edge select</h3>

This button selects the negative edge triggering. Trigger is raised only of the current sample is below threshold and the previous sample is above threshold

<h3>8. Both edges select</h3>

This button selects both signal edges triggering. Trigger is raised if the signal crosses threshold in any direction. This actually combines the positive and negative edge testing with an or condition.

<h3>9. Trigger level adjustment</h3>

This pair of sliders let you adjust the trigger level, The level appears on the left of the sliders. 

The top slider is a coarse adjustement. Each step moves the trigger level by an amount that depends on the projection type:

  - Real, Imag: 0.01
  - Mag: 0.005
  - MagDB: 1 dB
  - Phi, dPhi: 0.01
  
The bottom slider is a fine adjustment. Each step moves the trigger level by an amount that depends on the projection type:

  - Real, Imag: 50.0E-6
  - Mag: 25.0sE-6
  - MagDB: 0.01 dB
  - Phi, dPhi: 50.0E-6
  
<h3>10: Trigger delay</h3>

The actual trigger top can be moved forward by a number of samples. This pair of slider lets you adjust this delay. The delay in time units appears at the left of the sliders and the amount of samples as a tooltip

The top slider is a coarse adjustement. Each step moves the delay by a trace length. The bottom slider is a fine adjustment. Each step moves the delay by 20 samples

<h3>11. Pre-trigger delay</h3>

The trace can start an amount of time before the trigger top. This pair of sliders let you adjust this amount of time which is displayed at the left of the sliders. The corresponding number of samples appear as a tooltip.

The top slider is a coarse adjustement. Each step moves the delay by a hundreth of the trace length. The bottom slider is a fine adjustment. Each step moves the delay by 20 samples.

<h3>12. Trigger line color</h3>

This area shows the current trigger line color. When clicking on it a color chooser dialog appears that lets you change the color of the current trigger line color. This line appears when the selected trace projection matches the trigger projecion.

<h3>13. One-shot trigger</h3>

This button toggles a one shot trigger. When the (final) trigger is raised only one trace is processed until the button is released.

<h3>14. Freerun</h3>

When active the triggers are disabled and traces are processed continuously. This is the default at plugin start time. 