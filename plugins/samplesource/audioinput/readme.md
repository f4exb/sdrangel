<h1>Audio input plugin</h1>

<h2>Introduction</h2>

This input sample source plugin gets its samples from an audio device.

<h2>Interface</h2>

![Audio input plugin GUI](../../../doc/img/AudioInput_plugin.png)

<h3>1: Start/Stop</h3>

Device start / stop button.

  - Blue triangle icon: device is ready and can be started
  - Green square icon: device is running and can be stopped
  - Magenta (or pink) square icon: an error occurred. In the case the device was accidentally disconnected you may click on the icon, plug back in and start again.

<h3>2: Device</h3>

The audio device to use.

<h3>3: Refresh devices</h3>

Refresh the list of audio devices.

<h3>4: Audio sample rate</h3>

Audio sample rate in Hz (Sa/s).

<h3>5: Decimation</h3>

A decimation factor to apply to the audio data. The baseband sample rate will be the audio sample, divided by this decimation factor.

<h3>6: Volume</h3>

A control to set the input volume. This is not supported by all input audio devices.

<h3>7: Channel Map</h3>

This controls how the left and right stereo audio channels map on to the IQ channels.

* I=L, Q=0 - The left audio channel is driven to the I channel. The Q channel is set to 0.
* I=R, Q=0 - The right audio channel is driven to the I channel. The Q channel is set to 0.
* I=L, Q=R - The left audio channel is driven to the I channel. The right audio channel is driven to the Q channel.
* I=R, Q=L - The right audio channel is driven to the I channel. The left audio channel is driven to the Q channel.
