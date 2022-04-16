<h1>Perseus input plugin</h1>

<h2>Introduction</h2>

This input sample source plugin gets its samples from a [Perseus device](http://microtelecom.it/perseus/).

<h2>Build</h2>

This plugin will be built only if the following conditions are met:

  - [My fork of libperseus-sdr library](https://github.com/f4exb/libperseus-sdr.git) is installed in your system. You will have to checkout the `fixes` branch which however is the default. There are a few fixes from the original mainly to make it work in a multi-device context.
  - The 24 bit Rx DSP chain is activated in the compilation with the `-DRX_SAMPLE_24BIT=ON` option on the cmake command line.

If you build it from source and install it in a custom location say: `/opt/install/libperseus` you will have to add `-DPERSEUS_DIR=/opt/install/libperseus` to the cmake command line.

&#9758; From version 3.12.0 the Linux binaries are built with the 24 bit Rx option and Perseus input plugin.

<h2>Interface</h2>

The top and bottom bars of the device window are described [here](../../../sdrgui/device/readme.md)

It has a limited number of controls compared to other source interfaces. This is because a lot of things are handled automatically within the Perseus:

  - gains
  - DC and IQ correction
  - filter selection based on frequency

![Perseus input plugin GUI](../../../doc/img/Perseus_plugin.png)

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

Baseband I/Q sample rate in kS/s. This is the device to host sample rate (4) divided by the software decimation factor (6).

<h3>2: LO ppm correction</h3>

This is the correction factor in ppm applied to the local oscillator in software. This is normally not needed the Perseus is already on spot. You can reset the ppm value anytime by pressing on button (3)

<h3>3: Reset LO ppm correction</h3>

This resets the LO ppm correction (zero the value).

<h3>4: Device to hast sample rate</h3>

This is the device to host sample rate in kilo samples per second (kS/s). The sample rate can be as low as 48 kS/s so there is no need for software decimation. Note that at 48 kS/s some slight rate mismatch can appear with the audio that has the same nominal rate. This may cause some occasional audio samples drops however hardly noticeable.

<h3>5: Wideband mode</h5>

Switch on this button to disable the preselection filters. The corresponding LED on the Perseus front panel will be lit accordingly.

<h3>6: Decimation factor</h3>

The I/Q stream from the Perseus to host is downsampled by a power of two before being sent to the passband. This is normally not needed for most use cases as the Perseus can go as low as 48 kS/s which is the lower limit for audio channel plugins (AM, FM, SSB, Digital voice). So it can be left to `1` most of the time. A software decimation by 2 or 4 is still provided for easier analysis of very narrowband or slow varying signals. Note that there is no dynamic gain with this decimation as the precision is already limited to 24 significant bits either for integer or floating point (float) processing.

<h3>7: Transverter mode open dialog</h3>

This button opens a dialog to set the transverter mode frequency translation options. The details about this dialog can be found [here](../../../sdrgui/gui/transverterdialog.md)

<h3>8: Attenuators control</h3>

Use this combo box to control the attenuators inside the Perseus:

  - 0 dB: no attenuation
  - 10 dB: 10 dB attenuator engaged
  - 20 dB: 20 dB attenuator engaged
  - 30 dB: 10 and 20 dB attenuators engaged

The LEDs on the Perseus front panel corresponding to each attenuator are lit accordingly.

<h3>9: ADC dither</h3>

Use this button to turn on or off the Perseus ADC dithering

<h3>10: ADC preamplifier</h3>

Use this button to turn on or off the Perseus ADC preamplifier

