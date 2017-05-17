<h1>LimeSDR input plugin</h1>

<h2>Introduction</h2>

This input sample source plugin gets its samples from a [LimeSDR device](https://myriadrf.org/projects/limesdr/). LimeSDR plugins are not included in the Win32 binary distribution.

&#9888; LimeSuite library is difficult to implement due to the lack of documentation. The plugins should work normally when running as single instances. Support of both Rx and/or both Rx running concurrently is experimental.

LimeSDR is a 2x2 MIMO device so it has two receiving channels that can run concurrently. To activate the second channel when the first is already active just open a new source tab in the main window (Devices -> Add source device) and select the same LimeSDR device.

<h2>Build</h2>

The plugin will be built only if LimeSuite is installed in your system. To build and install LimeSuite from source do:

  - `sudo apt-get install libsqlite3-dev`
  - `git clone https://github.com/myriadrf/LimeSuite.git`
  - `cd LimeSuite`
  - `mkdir builddir`
  - `cd builddir`
  - `cmake -DCMAKE_INSTALL_PREFIX=/opt/install/LimeSuite`
  - `make -j8`
  - `make install`

Then add the following defines on `cmake` command line:

`-DLIMESUITE_INCLUDE_DIR=/opt/install/LimeSuite/include -DLIMESUITE_LIBRARY=/opt/install/LimeSuite/lib/libLimeSuite.so`

<h2>Installation from binary packages</h2>

&#9758; LimeSuite is built in the binary packages so there is no external dependency

<h2>Interface</h2>

![LimeSDR input plugin GUI](../../../doc/img/LimeSDRInput_plugin.png)

<h3>1: Common stream parameters</h3>

![SDR Daemon FEC stream GUI](../../../doc/img/SDRdaemonFEC_plugin_01.png)

<h4>1.1: Frequency</h4>

This is the center frequency of reception in kHz.

<h4>1.2: Start/Stop</h4>

Device start / stop button. 

  - Blue triangle icon: device is ready and can be started
  - Green square icon: device is running and can be stopped
  - Magenta (or pink) square icon: an error occured. In the case the device was accidentally disconnected you may click on the icon to stop, plug back in, check the source on the sampling devices control panel and start again.
  
<h4>1.3: Record</h4>

Record baseband I/Q stream toggle button

<h4>1.4: Stream sample rate</h4>

Baseband I/Q sample rate in kS/s. This is the device sample rate (4) divided by the decimation factor (6). 

<h3>2: Channel number</h3>

LimeSDR is a 2x2 MIMO device so it has two receiving channels. This shows the corresponding Rx channel index (0 or 1).

<h3>3: NCO toggle</h3>

The button is lit when NCO is active and dark when inactive.

Use this button to activate/deactivate the TSP NCO. The LMS7002M chip has an independent NCO in each Rx channel that can span the bandwidth received by the ADC. This effectively allows non zero digital IF.

<h3>4: Zero (reset) NCO frequency</h3>

USe this push button to reset the NCO frequency to 0 and thus center on the main passband of the ADC.

<h3>5: Center frequency with NCO engaged</h3>

This is the center frequency of the mix of LO and NCO combined and is the source passband center frequency when the NCO is engaged. Use the thumbwheels to adjust frequency as done with the LO (1.1). Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2. The boundaries are dynamically calculated from the LO center frequency and sample rate.

<h3>6-7: Auto correction options</h3>

These buttons control the local DSP auto correction options:

  - **DC**: (6) auto remove DC component
  - **IQ**: (7) auto make I/Q balance
  
<h3>8: LMS7002M hardware decimation factor</h3>

The TSP block in the LMS7002M hardware has a decimation chain that acts on both Rx channels. It is composed of 5 halfband decimation stages and therefore can achieve decimation between 1 (no decimation) and 32 in increasing powers of 2: 1, 2, 4, 8, 16, 32.

Thus the actual sample rate of the ADC is the stream sample rate (10) multiplied by this factor. 

<h3>9: Software decimation factor</h3>

The I/Q stream from the LimeSDR is doensampled by a power of two by software inside the plugin before being sent to the passband. Possible values are increasing powers of two: 1 (no decimation), 2, 4, 8, 16, 32.

<h3>10: Device stream sample rate</h3>

This is the LMS7002M device to/from host stream sample rate in S/s.

Use the wheels to adjust the sample rate. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2. 

Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. 

The LMS7002M uses the same clock for both the ADCs and DACs therefore this sample rate affects all of the 2x2 MIMO channels.

<h3>11: Rx hardware filter bandwidth</h3>

This is the Rx hardware filter bandwidth in kHz in the LMS7002M device for the given channel. Boundaries are updated automatically but generally are from 1.4 to 130 MHz in 1 kHz steps. Use the wheels to adjust the value. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2.

<h3>12: TSP FIR filter toggle</h3>

The TSP in the LMS7002M chip has a FIR filter chain per channel. Use this button to activate or deactivate the TSP FIR filter.

<h3>13: TSP FIR filter bandwidth</h3>

USe the wheels to adjust the bandwidth of the hardware TSP FIR filter. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2.

<h3>14: Gain</h2>

Use this slider to adjust the global gain of the LNA, TIA and PGA. LimeSuite software automatically set optimals values of the amplifiers to achive this global gain. This gain can be set between 0 and 70 dB in 1 dB steps. The value in dB appears at the right of the slider. 

<h3>15: Antenna select</h3>

Use this combo box to select the antenna input:

  - **No**: None
  - **Lo**: Selects the low frequency input (700 to 900 MHz nominally)
  - **Hi**: Selects the high frequncy input (2 to 2.6 GHz)
  - **Wo**: Selects the wideband input
  - **T1**: Selects loopback from TX #1 (experimental)
  - **T1**: Selects loopback from TX #2 (experimental)
  
<h3>16: Stream status indicator</h3>

This label turns green when status can be obtained from the current stream. Usually this means that the stream is up and running but not necessarily streaming data. The various status elements appear next on the same line (17, 18, 19)

<h3>17: Stream warning indicators</h3>

  - **U**: turns red if stream experiences underruns
  - **O**: turns red if stream experiences overruns  
  - **P**: turns red if stream experiences packet drop outs
  
<h3>18: Stream global (all Rx) throughput in MB/s</h3>

This is the stream throughput in MB/s and is usually about 3 times the sample rate for a single stream and 6 times for a dual Rx stream. This is due to the fact that 12 bits samples are used and although they are represented as 16 bit values only 12 bita travel on the USB link.

<h3>19: FIFO status</h3>

This is the fill percentage of the Rx FIFO in the LimeSuite interface. It should be zero most of the time.