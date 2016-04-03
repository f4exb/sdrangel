<h1>UDP source plugin</h1>

<h2>Introduction</h2>

By "source" one should undetstand a source of samples for the outside of SDRangel application. An UDP connection is established from the plugin to the given address and port and samples are directed to it.

The block size is fixed to 512 samples that is 2048 bytes. The receiving application must make sure it acknowledges this block size. UDP may fragment the block but there will be a point when the last UDP block will fill up a complete block of 2048 bytes. In particular in GNUradio the UDP source block must be configured with a 2048 bytes payload size.

<h2>Interface</h2>

![UDP Source plugin GUI](/doc/img/UDPsrc_plugin.png)

<h3>1: Frequency shift from center frequency of reception</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Use the "+/-" button on the left side to toggle between positive and negative shift.

<h3>2: Channel power</h3>

Total power in dB relative to a +/- 1.0 amplitude signal received in the pass band.

<h3>3: Type of samples</h3>

Combo box to specify the type of samples that are sent over UDP.

  - `S16LE I/Q`: Raw I/Q samples on signed 16 bits integers with Little Endian layout. Use it with software that accepts I/Q data as input like GNUradio with the `UDP source` block
  - `S16LE SSB`: AF of SSB demodulated signal as 16 bits signed integers with Little Endian layout. Use it with software that uses a SSB demodulated signal as input i.e. software that is based on the audio output of a SSB radio.
  - `S16LE NFM`: AF of FM demodulated signal as 16 bits signed integers with Little Endian layout. Use it with software that takes the FM demodulated audio or the discriminator output of a radio as input. Make sure you specify the appropriate signal bandwidth (see 7) according to the AF bandwidth needs.
  
<h3>4: Signal sample rate</h3>

Sample rate in samples per second of the signal that is sent over UDP. For raw I/Q output this is the sample rate of a complete I/Q sample (4 bytes). For demodulated outputs this is the sample rate of an "audio" sample (2 bytes).

<h3>5: Destination IP address</h3>

IP address of the remote destination to which samples are sent 

<h3>6: Destination port</h3>

Remote UDP port number to which samples are sent 

<h3>7: Signal bandwidth</h3>

The signal is bandpass filtered to this bandwidth (zero frequency centered) before being sent out as raw I/Q samples or before being demodulated for SSB and FM outputs. Thus a 20000 Hz bandwidth for example means +/-10000 Hz around center channel frequency.

<h3>8: Audio UDP port</h3>

There is a possible feedback of audio samples at 48 kHz into SDRangel using this port as the UDP port on the local machine to collect the audio samples.

<h3>9: Toggle audio feedback</h3>

This turns on or off the audio samples feedback

<h3>10: Mono/Stereo toggle for audio feedback</h3>

This toggles between mono or stereo audio feedback

<h3>11: Apply (validation) button</h3>

When any item of connection or stream configuration changes this button becomes active to make the changes effective when pressed. 

<h3>12: Boost</h3>

Amplify the signal being sent over UDP from the original. There are 4 levels of amplification from 0: none to 3: maximum

<h3>13: Audio volume</h3>

Volume of the audio feedback (when used).

<h3>14: Spectrum display</h3>

This is the spectrum display of the channel signal after bandpass filtering. Please refer to the Spectrum display description for details. 

This spectrum is centered on the center frequency of the channel (center frequency of reception + channel shift) and is that of a complex signal i.e. there are positive and negative frequency. The width of the spectrum is proportional of the sample rate. That is for a sample rate of S samples per seconds the spectrum spans from -S/2 to +S/2 Hz. 

