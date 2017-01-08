<h1>HackRF output plugin</h1>

<h2>Introduction</h2>

This output sample sink plugin sends its samples to a [HackRF device](https://greatscottgadgets.com/hackrf/). It is supported in Linux only.

<h2>Build</h2>

The plugin will be built only if the [HackRF host library](https://github.com/mossmann/hackrf) is installed in your system. If you build it from source and install it in a custom location say: `/opt/install/libhackrf` you will have to add `-DLIBHACKRF_INCLUDE_DIR=/opt/install/libhackrf/include -DLIBHACKRF_LIBRARIES=/opt/install/libhackrf/lib/libhackrf.so` to the cmake command line.

The HackRF Host library is also provided by many Linux distributions and is built in the SDRangel binary releases.

<h2>Interface</h2>

![HackRF output plugin GUI](../../../doc/img/HackRFOutput_plugin.png)

<h3>1: Start/Stop</h3>

Device start / stop button. 

  - Blue triangle icon: device is ready and can be started
  - Red square icon: device is running and can be stopped
  - Magenta (or pink) square icon: an error occured. In the case the device was accidentally disconnected you may click on the icon, plug back in and start again.

Please note that HackRF is a half duplex device so if you have the Rx open in another tab you have to stop it first before starting the Tx for it to work properly. In a similar manner you should stop the Tx before resuming the Rx.

The settings on Tx or Rx tab are reapplied on start so provided the half duplex is handled correctly as stated above these settings can be considered independent.
  
<h3>2: Baseband sample rate</h3>

This is the baseband sample rate in kS/s before interpolation (5) to produce the final stream that is sent to the HackRF device. Thus this is the device sample rate (8) divided by the interpolation factor (5).
  
<h3>3: Frequency</h3>

This is the center frequency of transmission in kHz.

<h3>4: Local Oscillator correction</h3>

Use this slider to adjust LO correction in ppm. It can be varied from -10.0 to 10.0 in 0.1 steps and is applied in software.

<h3>5: Interpolation factor</h3>

The baseband stream is interpolated by this value before being sent to the HackRF device. Possible values are:

  - **1**: no interpolation
  - **2**: multiply sample rate by 2
  
Larger values introduce performance problems.

<h3>6: Bias tee</h3>

Use this checkbox to toggle the +5V power supply on the antenna connector.

<h3>7:RF amp</h3>

Use this checkbox to toggle the output amplifier (PA). This PA gives an additional gain of 14 dB. 

According to HackRF documentation the output power when the PA is engaged and the Tx VGA (10) is at full power (47dB) is the following:

  - 10 MHz to 2150 MHz: 5 dBm to 15 dBm, generally increasing as frequency decreases
  - 2150 MHz to 2750 MHz: 13 dBm to 15 dBm
  - 2750 MHz to 4000 MHz: 0 dBm to 5 dBm, increasing as frequency decreases
  - 4000 MHz to 6000 MHz: -10 dBm to 0 dBm, generally increasing as frequency decreases

<h3>8: Device sample rate</h3>

This is the HackRF device DAC sample rate in kS/s. Possible values are: 2400, 3200, 4800, 5600, 6400, 8000, 9600, 12800, 19200 kS/s. 

<h3>9: Tx filter bandwidth</h3>

This is the Tx filter bandwidth in kHz. Possible values are: 1750, 2500, 3500, 5000, 5500, 6000, 7000, 8000, 9000, 10000, 12000, 14000, 15000, 20000, 24000, 28000 kHz.

<h3>10: Tx variable gain amplifier gain</h3>

The Tx VGA gain can be adjusted from 0 dB to 47 dB in 1 dB steps. See (7) for an indication on maximum output power.