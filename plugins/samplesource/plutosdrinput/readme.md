<h1>PlutoSDR input plugin</h1>

<h2>Introduction</h2>

This input sample source plugin gets its samples from a [PlutoSDR device](https://wiki.analog.com/university/tools/pluto). This is also known as the ADALM-Pluto. ADALM stands for Analog Devices Active Learning Module and is targeting students in electrical engineering and digital signal processing. Of course it can be used as a radio device like any other SDR.

&#9758; PlutoSDR is physically implemented as a 1x1 SISO device although the AD9363 chip does have a second Rx and a second Tx channel. Revision C of the board claims to have pads to allow hackers connecting the second ports externally however for now only the first Rx channel is supported by this plugin.

&#9758; When running the Pluto on Ethernet interface you have to create a non discoverable device reference in the [user arguments dialog](https://github.com/f4exb/sdrangel/blob/master/sdrgui/deviceuserargs.md) from the main window Preferences &gt; Devices menu. You must use the `PlutoSDR` hardware ID then specify the device address with a `uri` parameter in the user arguments for example: `uri=ip:192.168.1.10`. Note that this will become effective once SDRangel is restarted.

<h2>Build</h2>

The plugin will be built only if libiio is installed in your system. To build and install libiio from source do:

  - `git clone https://github.com/analogdevicesinc/libiio.git`
  - `cd libiio`
  - `git checkout v0.10 # or latest stable release`
  - `mkdir build`
  - `cd build`
  - `cmake -DCMAKE_INSTALL_PREFIX=/opt/install/libiio -DINSTALL_UDEV_RULE=OFF ..`
  - `make -j8`
  - `make install`

Then add the following defines on `cmake` command line when compiling SDRangel:

`-DIIO_DIR=/opt/install/libiio`

<h2>Interface</h2>

The top and bottom bars of the device window are described [here](../../../sdrgui/device/readme.md)

![PlutoSDR input plugin GUI](../../../doc/img/PlutoSDRInput_plugin.png)

<h3>1: Common stream parameters</h3>

![PlutoSDR input stream GUI](../../../doc/img/PlutoSDRInput_plugin_1.png)

<h4>1.1: Frequency</h4>

This is the center frequency of reception in kHz. The limits are set as those of the AD9364: from 70 to 6000 MHz. PlutoSDR can be fooled to think it has a AD9364 chip with a very simple software hack described [here](https://wiki.analog.com/university/tools/pluto/users/customizing).

AD9363 extended frequency range is not guaranteed but would work normally particularly in the lower range.

<h4>1.2: Start/Stop</h4>

Device start / stop button.

  - Blue triangle icon: device is ready and can be started
  - Green square icon: device is running and can be stopped
  - Magenta (or pink) square icon: an error occurred. In the case the device was accidentally disconnected you may click on the icon to stop, plug back in, check the source on the sampling devices control panel and start again.

<h4>1.4: ADC sample rate</h4>

This is the sample rate at which the ADC runs in kS/s (k) or MS/s (M) before hardware decimation. Hardware decimation is only partially controlled by the user using the FIR decimation factor (12). The value here is the value returned by the device interface therefore it is always exact.

<h4>1.5: Stream sample rate</h4>

In device to host sample rate input mode (8A) this is the baseband I/Q sample rate in kS/s. This is the device to host sample rate (8) divided by the software decimation factor (5).

In baseband sample rate input mode (8A) this is the device to host sample rate in kS/s. This is the baseband sample rate (8) multiplied by the software decimation factor (5)

<h3>2: LO ppm correction</h3>

Use this slider to adjust LO correction in ppm. It can be varied from -20.0 to 20.0 in 0.1 steps and is applied in hardware. This applies to the oscillator that controls both the Rx and Tx frequency therefore it is also changed on the Tx plugin if it is active.

<h3>3: Transverter mode open dialog</h3>

This button opens a dialog to set the transverter mode frequency translation options. The details about this dialog can be found [here](../../../sdrgui/gui/transverterdialog.md)

<h3>4: Auto correction options</h3>

  - **SW** section: These buttons control the software DSP auto correction options
    - **DC**: auto remove DC component
    - **IQ**: auto make I/Q balance. The DC correction must be enabled for this to be effective.

  - **HW** section: These buttons control the hardware AD9363 DC and I/Q compensation
    - **RFDC**: RF DC correction
    - **BBDC**: Baseband DC correction
    - **IQ**: I/Q imbalance correction.

&#9758; There is a reason why you would like to remove any I/Q correction: this is for the reception of signals that are symmetric in I and Q (real signals). More details [here](https://wiki.analog.com/university/tools/pluto/users/non_quad) something not very well known about SDR and DSP.

<h3>5: Software decimation factor</h3>

The I/Q stream from the PlutoSDR is downsampled by a power of two by software inside the plugin before being sent to the passband. Possible values are increasing powers of two: 1 (no decimation), 2, 4, 8, 16, 32, 64.

<h3>6: Decimated bandpass center frequency position relative the the PlutoSDR Rx center frequency</h3>

  - **Cen**: the decimation operation takes place around the PlutoSDR Rx center frequency Fs
  - **Inf**: the decimation operation takes place around Fs - Fc.
  - **Sup**: the decimation operation takes place around Fs + Fc.

With SR as the sample rate before decimation Fc is calculated as:

  - if decimation n is 4 or lower:  Fc = SR/2^(log2(n)-1). The device center frequency is on the side of the baseband. You need a RF filter bandwidth at least twice the baseband.
  - if decimation n is 8 or higher: Fc = SR/n. The device center frequency is half the baseband away from the side of the baseband. You need a RF filter bandwidth at least 3 times the baseband.

<h3>7: Antenna (input) connection</h3>

The AD9363 has many port options however as only the A balanced input is connected you should leave it as the default. This is a provision for people who want to hack the board. The different values may be found in the AD9363 documentation.

<h3>8: Device to host sample rate / Baseband sample rate input toggle</h3>

Use this toggle button to switch the sample rate input next (8) between device to host sample rate and baseband sample rate input. The button shows the current mode:

  - **SR**: device sample rate input mode. The baseband sample rate (1.5) is the device to host sample rate (8) divided by the software decimation factor (5).
  - **BB**: baseband sample rate input mode. The device sample rate (1.5) is the baseband sample rate (8) multiplied by the software decimation factor (5).

<h3>9: Sample rate</h3>

This is the device to host sample rate or baseband sample rate in samples per second (S/s). The control (8) is used to switch between the two input modes.

The limits are adjusted automatically. In baseband input mode the limits are driven by the software decimation factor (5). You may need to increase this decimation factor to be able to reach lower values.

Use the wheels to adjust the sample rate. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows.

The minimum sample rate depends on the hardware FIR decimation factor (13) and is the following:

  - no decimation: 25/12 MS/s thus 2083336 S/s (next multiple of 4)
  - decimation by 2: 25/24 MS/s thus 1041668 S/s
  - decimation by 4: 25/48 MS/s thus 520834 S/s

The maximum sample rate is fixed and set to 20 MS/s

<h3>10: Rx analog filter bandwidth</h3>

This is the Rx analog filter bandwidth in kHz in the AD9363 device. It can be varied from 200 kHz to 14 MHz in 1 kHz steps. Use the wheels to adjust the value. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

<h3>11: Hardware FIR filter toggle</h3>

The AD9363 chip has an optional FIR filter in the Rx decimation chain as the last decimation block. Use this button to activate or deactivate the filter.

The FIR filter settings are the same on Rx and Tx side therefore any change here is automatically forwarded to the Tx GUI.

<h3>12: Hardware FIR filter bandwidth</h3>

Use the wheels to adjust the bandwidth of the hardware FIR filter. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

The filter limits are calculated as 0.05 and 0.9 times the FIR filter input frequency for the lower and higher limit respectively. The FIR filter input frequency is the baseband sample rate (5) multiplied by the FIR interpolation factor (9)

For bandwidths greater than 0.2 times the FIR filter input frequency the filter is calculated as a windowed FIR filter with a Blackman-Harris window. This has a high out of band rejection value at the expense of a slightly smoother roll off compared to other filters. The bandwidth value sets the -6 dB point approximately.

For bandwidths between 0.05 and 0.2 times the FIR filter input frequency the window used is a Hamming window giving a sharper transition.

<h3>13: Hardware FIR decimation factor</h3>

The FIR filter block can provide a decimation by 1 (no decimation), 2 or 4. This controls the minimum possible baseband sample rate as already discussed in (8).

<h3>14: Hardware FIR gain</h3>

The FIR filter can introduce a gain that can be set to -12, -6, 0 or 6 dB. The FIR has a fixed gain of 6 dB so to maximize dynamic range one would set the gain at -6 dB so that the overall gain is set at 0 dB.

<h3>15: Gain mode</h3>

Use this combo to select between gain options:

  - **Man**: manual: the overall gain in dB can be set with the gain button (15)
  - **Slow**: slow AGC: gain button (15) is disabled
  - **Fast**: fast AGC: gain button (15) is disabled
  - **Hybr**: hybrid AGC: gain button (15) is disabled

See AD9363 documentation for details on AGC options.

<h3>16: Global manual gain</h3>

Use this button to adjust the global gain manually in manual gain mode. This button is disabled when AGC modes are selected with combo (15).

<h3>17: Actual gain fetched from device</h3>

This is the actual gain in dB set in the device.

<h3>18: Indicative RSSI</h3>

This is the indicative RSSI of the receiver.

<h3>19: Board temperature</h3>

This is the board temperature in degrees Celsius updated every ~5s.
