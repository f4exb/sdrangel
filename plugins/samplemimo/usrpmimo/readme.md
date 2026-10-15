<h1>USRP MIMO plugin</h1>

<h2>Introduction</h2>

This MIMO plugin sends and receives multiple streams of I/Q samples to and from a [USRP device](https://www.ettus.com/product-category/usrp-bus-series/) that has more than one Rx or Tx channel, such as the B210, X310 or N320.

Up to two Rx and two Tx channels are supported. On devices with more channels (E.g. N310), the first two channels are used.

The Rx channels are received using a single UHD stream and the Tx channels are transmitted using a single UHD stream,
so that samples for each channel are time aligned, which is required for applications such as direction finding and beamforming.

The Rx and Tx sides can be started and stopped independently.

The center frequency is common to both channels on each side (Rx and Tx), as is required for coherent operation.
On devices where the two channels share an LO (E.g. B210), this is also a hardware requirement.
The sample rate is common to Rx and Tx.

Settings are applied to the device in a separate thread, so that settings that take a long time to apply
(E.g. sample rate, clock source and bandwidth) do not block the GUI.

<h2>Interface</h2>

The top and bottom bars of the device window are described [here](../../../sdrgui/device/readme.md)

![USRP MIMO plugin GUI](../../../doc/img/USRPMIMO_plugin.png)

<h3>1: Stream selection</h3>

Selects which side (Rx or Tx) and which channel (0 or 1) the settings below apply to.
Settings common to both channels of a side (E.g. center frequency, LO offset, bandwidth, decimation or interpolation)
are shown for the selected side. Settings that are per channel (gain, gain mode and antenna) are shown for the selected channel.

<h3>2: Spectrum source</h3>

Selects which side (Rx or Tx) and which channel is displayed in the main spectrum.

<h3>3: Rx and Tx start/stop</h3>

Starts or stops the Rx or Tx side. The button color indicates the state:

  - Grey: device is not running
  - Blue: device is idle
  - Green: device is running
  - Red: device is in error

<h3>4: Baseband sample rate</h3>

This is the baseband sample rate of the selected side, after software decimation or interpolation.

<h3>5: Center frequency</h3>

This is the center frequency in kHz of the selected side. It is common to both channels.

<h3>6: LO offset</h3>

The LO of the selected side is offset from the center frequency by this amount (in kHz), and the digital NCOs in the device are used to
compensate. This can be used to move the DC spike and LO leakage away from the center of the band. It should be less than half of the sample rate.

<h3>7: Clock source</h3>

Selects the reference clock source for the device (E.g. internal, external or gpsdo). If the selected clock is not detected, the device
falls back to the internal clock, and this is shown here.

<h3>8: DC offset and IQ imbalance correction</h3>

Enables automatic DC offset and IQ imbalance correction in the device, for both Rx channels. These are not available for Tx.

<h3>9: Analog low pass filter bandwidth</h3>

Sets the bandwidth of the analog low pass filter, in kHz, for both channels of the selected side.

<h3>10: Transverter mode</h3>

Opens the transverter dialog for the selected side. See the [transverter dialog](../../../sdrgui/gui/transverterdialog.md) documentation for details.

<h3>11: Sample rate</h3>

This is the device to host (SR) or baseband (BB) sample rate in samples per second. The button on the left switches between the two modes.
The device to host sample rate is common to the Rx and Tx sides.

<h3>12: Master clock rate</h3>

This is the sample rate between the FPGA and RFIC, as calculated by UHD from the requested sample rate. It is common to Rx and Tx.

<h3>13: Software decimation or interpolation factor</h3>

This is the software decimation (Rx) or interpolation (Tx) factor applied to the samples of the selected side, to obtain the baseband sample rate.

<h3>14: Gain</h3>

The gain lock button, when engaged, applies gain settings to both channels of the selected side.

The gain mode (Rx only) selects between automatic gain control and manual gain. The slider sets the gain in dB, when manual gain is selected.

<h3>15: Antenna</h3>

Selects the antenna for the selected channel.

<h3>16: Board temperature</h3>

This is the temperature of the board in degrees C, for devices that have a temperature sensor (E.g. the AD9361 on the B210).

<h3>17: Stream status indicators</h3>

These show the status of the stream for the selected side:

  - Stream: turns green when the stream is running
  - **O**/**U**: turns red if the Rx stream experiences overruns or the Tx stream experiences underruns
  - **T**/**D**: turns red if the Rx stream experiences timeouts or the Tx stream drops packets

The indicators are reset when the side is restarted.

Hovering over an indicator shows statistics for the current (or last) run: the total number of events, the average number per minute, the number in the last minute and the time of the last event.
