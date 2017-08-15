<h1>UDP source plugin</h1>

<h2>Introduction</h2>

By "source" one should undetstand a source of samples for the outside of SDRangel application. An UDP connection is established from the plugin to the given address and port and samples are directed to it.

The UDP block size or UDP payload size is fixed at 512 bytes.

The receiving application must make sure it acknowledges this block size. UDP may fragment the block but there will be a point when the last UDP block will fill up a complete block of this amount of bytes. In particular in GNUradio the UDP source block must be configured with a 512 bytes payload size.

<h2>Interface</h2>

![UDP Source plugin GUI](/doc/img/UDPsrc_plugin.png)

<h3>1: Frequency shift from center frequency of reception</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arroews. Pressing shift simultanoeusly moves digit by 5 and pressing control moves it by 2.

<h3>2: Channel power</h3>

Total power in dB relative to a +/- 1.0 amplitude signal received in the pass band.

<h3>3: Type of samples</h3>

Combo box to specify the type of samples that are sent over UDP.

  - `S16LE I/Q`: Raw I/Q samples on signed 16 bits integers with Little Endian layout. Use it with software that accepts I/Q data as input like GNUradio with the `UDP source` block. The output is interleaved I and Q samples
  - `S16LE NFM`: AF of FM demodulated signal as 16 bits signed integers with Little Endian layout. Use it with software that takes the FM demodulated audio or the discriminator output of a radio as input. Make sure you specify the appropriate signal bandwidth (see 7) according to the AF bandwidth needs. The output is a repetition of NFM samples on real part and on imaginary part this facilitates integration wtih software expecting a stereo type of input with the same samples on L and R channels. With GNURadio just use a complex to real block.
  - `S16LE NFM Mono`: This is the same as above but only one sample is output for one NFM sample. This can be used with software that accept a mono type of input like `dsd`. With GNURadio you can use a short to float block but remember that the UDP payload size is now 1024 bytes so you have to change it in the UDP source block. 
  - `S16LE USB`: AF of USB demodulated signal as 16 bits signed integers with Little Endian layout. Use it with software that uses a SSB demodulated signal as input i.e. software that is based on the audio output of a SSB radio. The output is the I/Q binaural output of the demodulator.
  - `S16LE LSB`: AF of LSB demodulated signal as 16 bits signed integers with Little Endian layout. Use it with software that uses a SSB demodulated signal as input i.e. software that is based on the audio output of a SSB radio. The output is the I/Q binaural output of the demodulator.
  - `S16LE LSB Mono`: AF of the LSB part of a SSB demodulated signal as "mono" (I+Q)*0.7 samples that is one sample per demodulator output sample. This can be used with software that accepts mono type of input. Remember that as these are single 16 bits samples the UDP payload size is 1024 bytes (not 2048).
  - `S16LE USB Mono`: AF of the USB part of a SSB demodulated signal as "mono" (I+Q)*0.7 samples that is one sample per demodulator output sample. This can be used with software that accepts mono type of input. Remember that as these are single 16 bits samples the UDP payload size is 1024 bytes (not 2048).
  - `S16LE AM Mono`: AF of the enveloppe demodulated signal i.e. channel magnitude or sqrt(I² + Q²) as "mono" samples that is one sample per demodulator output sample. This can be used with software that accepts mono type of input. Remember that as these are single 16 bits samples the UDP payload size is 1024 bytes (not 2048)    
  
<h3>4: Signal sample rate</h3>

Sample rate in samples per second of the signal that is sent over UDP. The actual byte rate depends on the type of sample which corresponds to a number of bytes per sample.

<h3>5: Remote IP address</h3>

IP address of the remote destination to which samples are sent 

<h3>6: Remote data port</h3>

Remote UDP port number to which samples are sent 

<h3>7: Signal bandwidth</h3>

The signal is bandpass filtered to this bandwidth (zero frequency centered) before being sent out as raw I/Q samples or before being demodulated for SSB and FM outputs. Thus a 20000 Hz bandwidth for example means +/-10000 Hz around center channel frequency.

<h3>8: Local audio UDP port</h3>

There is a possible feedback of audio samples at 48 kHz into SDRangel using this port as the UDP port on the local machine to collect the audio samples.

<h3>9: Toggle audio feedback</h3>

This turns on or off the audio samples feedback

<h3>10: Mono/Stereo toggle for audio feedback</h3>

This toggles between mono or stereo audio feedback

<h3>11: FM deviation</h3>

This is the maximum expected FM deviation in Hz for NFM demodulated samples. Therefore it is active only for `NFM` types of sample formats. A positive deviation of this amount from the central carrier will result in a sample output value of 32767 (0x7FFF) corresponding to a +1.0 real value. A negative deviation of this amount from the central carrier will result in a sample output value of -32768 (0x8000) corresponding to a -1.0 real value.  

<h3>12: Boost</h3>

Amplifies the input passband signal before processing. The level of amplification is the log2 of the amplification factor applied therefore it varies from 0 dB (0) to +30 dB (3) in 10 dB steps.

<h3>13: Audio volume</h3>

Volume of the audio feedback (when used).

<h3>14: Apply (validation) button</h3>

When any item of connection or stream configuration changes this button becomes active to make the changes effective when pressed. 

<h3>15: Spectrum display</h3>

This is the spectrum display of the channel signal after bandpass filtering. Please refer to the Spectrum display description for details. 

This spectrum is centered on the center frequency of the channel (center frequency of reception + channel shift) and is that of a complex signal i.e. there are positive and negative frequencies. The width of the spectrum is proportional of the sample rate. That is for a sample rate of S samples per seconds the spectrum spans from -S/2 to +S/2 Hz. 

