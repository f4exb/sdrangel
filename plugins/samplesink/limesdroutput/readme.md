<h1>LimeSDR output plugin</h1>

<h2>Introduction</h2>

This output sample sink plugin sends its samples to a [LimeSDR device](https://myriadrf.org/projects/limesdr/).

&#9888; LimeSuite library is difficult to implement due to the lack of documentation. The plugins should work normally when running as single instances. Support of both Rx and/or both Rx running concurrently is experimental.

LimeSDR is a 2x2 MIMO device so it has two transmitting channels that can run concurrently. To activate the second channel when the first is already active just open a new sink tab in the main window (Devices -> Add sink device) and select the same LimeSDR device.

<h2>Build</h2>

The plugin will be built only if LimeSuite is installed in your system. Please use version tagged v17.06.0 or maybe later. To build and install LimeSuite from source do:

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

![LimeSDR output plugin GUI](../../../doc/img/LimeSDROutput_plugin.png)

<h3>1: Start/Stop</h3>

Device start / stop button. 

  - Blue triangle icon: device is ready and can be started
  - Green square icon: device is running and can be stopped
  - Magenta (or pink) square icon: an error occured. In the case the device was accidentally disconnected you may click on the icon to stop, plug back in, check the source on the sampling devices control panel and start again.
  
<h3>2A: DAC sample rate</h3>

This is the sample rate at which the DAC runs in kS/s (k) or MS/s (M) after hardware interpolation (8). Thus this is the host to device sample rate (10) multiplied by the hardware interpolation factor (8).

&#9758; Note that changing the hardware interpolation factor (8) or the host to device sample rate (10) may change the ADC clock sample rate and therefore the Rx side hardware decimation factor and/or device to host sample rate. In fact ADC and DAC sample rates can be equal or related by a factor of 2. 

<h3>2B: Baseband sample rate</h3>

This is the baseband sample rate in kS/s before interpolation (9) to produce the final stream that is sent to the LimeSDR device. Thus this is the host to device sample rate (10) divided by the software interpolation factor (9).

Transmission latency depends essentially in the delay in the sample FIFO. The size of sample FIFO is calculated to give a fixed delay of 250 ms or 48000 samples whichever is bigger. Below is the delay in seconds vs baseband sample rate in kS/s from 48 to 250 kS/s. The 250 ms delay is reached at 192 kS/s:

![BladeRF output plugin FIFO delay other](../../../doc/img/LimeSDROutput_plugin_fifodly.png) 

<h3>3: Center frequency</h3>

This is the center frequency of transmission in kHz.

<h3>3A: Center frequency units</h3>

This is the center frequency units thus kHz (fixed)

<h3>4: Channel number</h3>

LimeSDR is a 2x2 MIMO device so it has two transmitting channels. This shows the corresponding Tx channel index (0 or 1).

<h3>5: NCO toggle</h3>

The button is lit when NCO is active and dark when inactive.

Use this button to activate/deactivate the TSP NCO. The LMS7002M chip has an independent NCO in each Tx channel that can span the bandwidth sent to the DAC. This effectively allows non zero digital IF.

<h3>6: Zero (reset) NCO frequency</h3>

Use this push button to reset the NCO frequency to 0 and thus center on the main passband of the DAC.

<h3>7: Center frequency with NCO engaged</h3>

This is the center frequency of the mix of LO and NCO combined and is the sink passband center frequency when the NCO is engaged. Use the thumbwheels to adjust frequency as done with the LO (1.1). Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2. The boundaries are dynamically calculated from the LO center frequency, sample rate and hardware interpolation factor.

&#9758; In the LMS7002M TSP block the NCO sits after the interpolator (see Fig.14 of the [datasheet](http://www.limemicro.com/wp-content/uploads/2015/09/LMS7002M-Data-Sheet-v2.8.0.pdf) p.7) so it runs at the actual DAC rate. Hence the NCO limits are calculated as +/- half the device to host sample rate multiplied by the hardware interpolation factor. For example with a 4 MS/s device to host sample rate (10) and a hadrware interpolation of 16 (8) you have +/- 32 MHz span around the LO for the NCO. In this example you can tune all HF frequencies with the center frequency set at its lowest (30 MHz). 

<h3>7A: External clock control</h3>

Use this button to open a dialog that lets you choose the external clock frequency and enable or disable it. When disabled the internal 30.72 MHz VCTCXO is used.

![LimeSDR input plugin gain GUI](../../../doc/img/LimeSDR_plugin_extclock.png)

<h4>7A.1: Exrernal clock frequency</h4>

Can be varied from 5 to 300 MHz

Use the thumbwheels to adjust frequency as done with the LO (1.1). Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2. The boundaries are dynamically calculated from the LO center frequency, sample rate and hardware decimation factor.

<h4>7A.2: Enable/disable external clock input</h7A>

Use this checkbox to enable or disable the external clock input

<h4>7A.3: Confirm changes</h4>

Use the "OK" button to confirm your changes
  
<h4>7A.4: Dismiss changes</h4>

Use the "Cancel" button to dismiss your changes

<h3>8: LMS7002M hardware interpolation factor</h3>

The TSP block in the LMS7002M hardware has an interpolation chain that acts on both Tx channels. It is composed of 5 halfband interpolation stages and therefore can achieve interpolation between 1 (no interpolation) and 32 in increasing powers of 2: 1, 2, 4, 8, 16, 32.

Thus the actual sample rate of the DAC is the stream sample rate (10) multiplied by this factor. In the screenshot example this yields a 40 MS/s rate at the DAC.

<h3>9: Software interpolation factor</h3>

The I/Q stream from the baseband is upsampled by a power of two by software inside the plugin before being sent to the LimeSDR device. Possible values are increasing powers of two: 1 (no interpolation), 2, 4, 8, 16, 32.

<h3>10: Host to device stream sample rate</h3>

This is the LMS7002M device to/from host stream sample rate in S/s. It is the same for the Rx and Tx systems.

Use the wheels to adjust the sample rate. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arroews.

The LMS7002M uses the same clock for both the ADCs and DACs therefore this sample rate affects all of the 2x2 MIMO channels.

<h3>11: Tx hardware filter bandwidth</h3>

This is the Tx hardware filter bandwidth in kHz in the LMS7002M device for the given channel. Boundaries are updated automatically but generally are from 5 to 130 MHz in 1 kHz steps. Use the wheels to adjust the value. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2.

<h3>12: TSP FIR filter toggle</h3>

The TSP in the LMS7002M chip has a FIR filter chain per channel. Use this button to activate or deactivate the TSP FIR filter.

<h3>13: TSP FIR filter bandwidth</h3>

Use the wheels to adjust the bandwidth of the hardware TSP FIR filter. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2.

<h3>14: Gain</h2>

Use this slider to adjust the global gain of the Tx chain. LimeSuite software automatically set optimal values of the amplifiers to achive this global gain. This gain can be set between 0 and 70 dB in 1 dB steps. The value in dB appears at the right of the slider.

<h3>15: Antenna selection</h3>

  - ** No **: no output
  - ** Lo **: Tx low range 30 MHz to 1.9 GHz
  - ** Hi **: Tx high range: 2 GHz and above

<h3>16: Stream status indicator</h3>

This label turns green when status can be obtained from the current stream. Usually this means that the stream is up and running but not necessarily streaming data. The various status elements appear next on the same line (16, 17, 18)

<h3>17: Stream warning indicators</h3>

  - **U**: turns red if stream experiences underruns
  - **O**: turns red if stream experiences overruns  
  - **P**: turns red if stream experiences packet drop outs
  
<h3>18: Stream global (all Tx) throughput in MB/s</h3>

This is the stream throughput in MB/s and is usually about 3 times the sample rate for a single stream and 6 times for a dual Tx stream. This is due to the fact that 12 bits samples are used and although they are represented as 16 bit values only 12 bita travel on the USB link.

<h3>19: FIFO status</h3>

This is the fill percentage of the Tx FIFO in the LimeSuite interface. In normal conditions this should be ~100%. On the picture the Tx is not started.

<h3>20: Board temperature</h3>

This is the board temperature in degrees Celsius updated every ~5s. Before the first probe the display marks "00C" this is normal.