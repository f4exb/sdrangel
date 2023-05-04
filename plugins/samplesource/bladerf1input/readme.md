<h1>BladeRF classic (v1) input plugin</h1>

<h2>Introduction</h2>

This input sample source plugin gets its samples from a [BladeRF1 device](https://www.nuand.com/bladerf-1) using LibbladeRF v.2. This is available in Linux distributions only.

<h2>Build</h2>

The plugin will be built only if the [BladeRF host library](https://github.com/Nuand/bladeRF) is installed in your system. If you build it from source and install it in a custom location say: `/opt/install/libbladeRF` you will have to add `-DBLADERF_INCLUDE_DIR=/opt/install/libbladeRF` to the cmake command line.

Note that libbladeRF v2 with git tag 2018.10-rc1 should be used (official release) thus:

  - The FX3 firmware version should be v2.3.1
  - The FPGA image version should be v0.9.0

The FPGA .rbf file should be copied to the folder where the `sdrangel` binary resides. You can download FPGA images from [here](https://www.nuand.com/fpga_images/)

The BladeRF Host library is also provided by many Linux distributions (check its version) and is built in the SDRangel binary releases.

<h2>Interface</h2>

The top and bottom bars of the device window are described [here](../../../sdrgui/device/readme.md)

![BladeRF1 input plugin GUI](../../../doc/img/BladeRF1Input_plugin.png)

<h3>1: Common stream parameters</h3>

![Remote source input stream GUI](../../../doc/img/RemoteInput_plugin_01.png)

<h4>1.1: Frequency</h4>

This is the center frequency of reception in kHz.

<h4>1.2: Start/Stop</h4>

Device start / stop button.

  - Blue triangle icon: device is ready and can be started
  - Green square icon: device is running and can be stopped
  - Magenta (or pink) square icon: an error occurred. In the case the device was accidentally disconnected you may click on the icon, plug back in and start again.

<h4>1.4: Stream sample rate</h4>

In device to host sample rate input mode (4A) this is the baseband I/Q sample rate in kS/s. This is the device to host sample rate (4) divided by the decimation factor (5).

In baseband sample rate input mode (4A) this is the device to host sample rate in kS/s. This is the baseband sample rate (4) multiplied by the decimation factor (5)

<h3>2: Auto correction options</h3>

These buttons control the local DSP auto correction options:

  - **DC**: auto remove DC component
  - **IQ**: auto make I/Q balance. The DC correction must be enabled for this to be effective.

<h3>3: XB-200 add-on control</h3>

This controls the optional XB-200 add-on when it is fitted to the BladeRF main board. These controls have no effect if the XB-200 board is absent. Options are:

  - **None**: XB-200 is ignored
  - **Bypass**: XB-200 is passed through
  - **Auto 1dB**: The 50, 144 and 220 MHz filters are switched on automatically according to the frequency of reception when it is within the -1 dB passband of the filters
  - **Auto 3dB**: The 50, 144 and 220 MHz filters are switched on automatically according to the frequency of reception when it is within the -3 dB passband of the filters
  - **Custom**: The signal is routed through a custom filter
  - **50M**: The signal is routed through the 50 MHz filter
  - **144M**: The signal is routed through the 144 MHz filter
  - **222M**: The signal is routed through the 222 MHz filter

<h3>4A: Device sample rate / Baseband sample rate input toggle</h3>

Use this toggle button to switch the sample rate input next (4) between device sample rate and baseband sample rate input. The button shows the current mode:

  - **SR**: device sample rate input mode. The baseband sample rate (1.4) is the device sample rate (4) divided by the decimation factor (5).
  - **BB**: baseband sample rate input mode. The device sample rate (1.4) is the baseband sample rate (4) multiplied by the decimation factor (5).

<h3>4: Sample rate</h3>

This is the BladeRF device ADC sample rate or baseband sample rate in samples per second (S/s). The control (4A) is used to switch between the two input modes.

The limits are adjusted automatically. In baseband input mode the limits are driven by the decimation factor (5). You may need to increase this decimation factor to be able to reach lower values.

Use the wheels to adjust the sample rate. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

<h3>5: Decimation factor</h3>

The I/Q stream from the BladeRF ADC is downsampled by a power of two before being sent to the baseband. Possible values are increasing powers of two: 1 (no decimation), 2, 4, 8, 16, 32, 64.

<h3>6: Baseband center frequency position relative to the BladeRF Rx center frequency</h3>

Possible values are:

  - **Cen**: the decimation operation takes place around the BladeRF Rx center frequency Fs
  - **Inf**: the decimation operation takes place around Fs - Fc.
  - **Sup**: the decimation operation takes place around Fs + Fc.

With SR as the sample rate before decimation Fc is calculated as:

  - if decimation n is 4 or lower:  Fc = SR/2^(log2(n)-1). The device center frequency is on the side of the baseband. You need a RF filter bandwidth at least twice the baseband.
  - if decimation n is 8 or higher: Fc = SR/n. The device center frequency is half the baseband away from the side of the baseband. You need a RF filter bandwidth at least 3 times the baseband.

<h3>7: Rx filter bandwidth</h3>

This is the Rx filter bandwidth in kHz in the LMS6002D device. Possible values are: 1500, 1750, 2500, 2750, 3000, 3840, 5000, 5500, 6000, 7000, 8750, 10000, 12000, 14000, 20000, 28000 kHz.

<h3>8: LNA gain</h2>

This is the LNA gain in dB. LNA is inside the LMS6002D chip and is placed before the RF mixer. Possible values are:

  - **0dB**: no gain
  - **3dB**
  - **6dB**

<h3>9: Variable gain amplifier #1 gain</h3>

The VGA1 gain can be adjusted from 5 dB to 30 dB in 1 dB steps. The VGA1 is inside the LMS6002D chip and is placed between the RF mixer and the baseband filter.

<h3>10: Variable gain amplifier #2 gain</h3>

The VGA2 gain can be adjusted from 0 dB to 30 dB in 3 dB steps. The VGA2 is inside the LMS6002D chip and is placed between the baseband filter and the ADC.
