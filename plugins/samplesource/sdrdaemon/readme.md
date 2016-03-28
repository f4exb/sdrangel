<h1>SDRdaemon plugin</h1>

<h2>Introduction</h2>

This input sample source plugin gets its samples over tbe network from a SDRdaemon server using UDP connection. SDRdaemon refers to the SDRdaemon utility found in [this](https://github.com/f4exb/sdrdaemon) Github repostory.

<h2>Interface</h2>

![SDR Daemon plugin GUI](/doc/img/SDRdaemon_plugin.png)

<h3>1: Frequency</h3>

This is the center frequency in kHz sent in the meta data from the distant SDRdaemon instance and corresponds to the center frequency of reception.

<h3>2: Auto correction options</h3>

These buttons control the local DSP auto correction options:

  - **DC**: auto remove DC component
  - **IQ**: auto make I/Q balance
  
<h3>3: Date/time</h3>

This is the current timestamp of the block of data sent from the receiver. It is refreshed about every second. This may not and is usually not the timestamp of the samples currently shown in the displays and in the audio since there is a failty large buffer in place to damper the variations of data receiving speed due to the network (see: 5.5: Main buffer length in seconds).

<h3>9: Main buffer R/W pointers gauge</h3>

There are two gauges separated by a dot in the center. Ideally these gauges should not display any value thus read and write pointers are always half a buffer apart.

  - The left gauge is the negative gauge. It is the value in percent of buffer size from the write pointer position to the read pointer position when this difference is less than half of a buffer distance. It means that the writes are leading or reads are lagging.
  - The right gauge is the positive gauge. It is the value in percent of buffer size of the difference from the read pointer position to the write pointer position when this difference is less than half of a buffer distance. It menas that the writes are lagging or reads are leading.

<h3>4: Lock and sizes</h3>

![SDR Daemon status1 GUI](/doc/img/SDRdaemon_plugin_04.png)

<h4>4.1: Stream lock</h4>

Turns green when stream is locked to meta data. Meta data carries the number of UDP packets in following frame. Lock status is obtained when the correct number of UDP packets have been received when the next meta data block occurs.

<h4>4.2: Frame size</h4>

Data is sent in frames with one meta data header. This is the size of such a frame. Frames may be compressed however the frame will be copied uncompressed to the main buffer.

<h4>4.3: Sample rate as sent in the meta data</h4>

The receiver sends the stream data rate with this nominal value

<h4>4.4: Actual stream sample rate</h4>

When the auto follow sample rate is engaged the actual system sample rate may differ from the nominal sample rate sent in the meta data. This is the actual system sample rate.

<h4>4.5: Skew rate</h4>

This is the difference in percent between nominal sample rate sent in the meta data and actual system sample rate

<h4>4.6: Main buffer R/W pointers positions</h4>

Read and write pointers should always be a half buffer distance buffer apart. This is the difference in percent of the main buffer size from this ideal position.

  - When positive it means that the read pointer is leading
  - When negative it means that the write pointer is leading (read is lagging)
  
This corresponds to the value shown in the gauges above (9)

<h3>5: Compressed stream status and auto follow control buttons</h3>

![SDR Daemon status2 GUI](/doc/img/SDRdaemon_plugin_05.png)

<h4>5.1: Compressed stream</h4>

When lit in green it means the stream is compressed using LZ4.

<h4>5.2: Compression ratio</h4>

This is the ratio between the compressed data size and the original data size.

<h4>5.3: CRC OK ratio</h4>

This is the percentage of received data blocks with a correct CRC. It should be as closed to 100% as possible.

<h4>5.4: LZ4 successful decodes ratio</h4>

This is the percentage of LZ4 data blocks that were successfully uncompressed. It should be as closed to 100% as possible.

<h4>5.5: Main buffer length in seconds</h4>

This is the main buffer (writes from UDP / reads from DSP engine) length in units of time (seconds). Initially the write pointer is at the start of buffer and the read pointer is on the middle. Thus it takes half a buffer length in time to get the first useful sample. The minimum length is 8s and can be as long as to fit 50 average read chunks.

<h4>5.6: Reset buffer indexes push button</h4>

This forces the write and read pointers in their initial position regardless of the contents of the buffer. The write pointer position is set at the start of buffer and the read pointer position is set at the middle.

<h4>5.7: Auto lock main buffer R/W pointers position toggle</h4>

When set it engages the auto lock of R/W pointers position. It will try to maintain a half buffer distance between read and write pointers.

<h4>5.8:  Auto lock to actual stream sample rate</h4>

This is rarely necessary. Only use it when you suspect that the sender data sample rate is not exactly as advertised in the meta data. This is normally exclusive of the auto lock R/W pointers position however the GUI allows both. You are advised to chose only one of the two.

<h3>6: Network parameters</h3>

![SDR Daemon status3 GUI](/doc/img/SDRdaemon_plugin_06.png)

<h4>6.1: Local interface IP address</h4>

Address of the network interface on the local (your) machine to which the SDRdaemon server sends samples to.

<h4>6.2: Local data port</h4>

UDP port on the local (your) machine to which the SDRdaemon server sends samples to.

<h4>6.3 Distant configuration port</h4>

TCP port on the distant machine hosting the SDRdaemon instance to send control messages to. The IP address of the host where the SDRdaemon instance runs is guessed from the address sending the data blocks hence it does not need to be specified.

<h4>6.4: Validation button</h4>

Whenever the address (6.1), data port (6.2) or configuration port (6.3) change this button is enabled to validate the new values.

<h3>7: Configuration parameters</h3>

![SDR Daemon status4 GUI](/doc/img/SDRdaemon_plugin_07.png)

<h4>7.1: Center frequency in kHz</h4>

This is the center frequency in kHz to which the hardware attached to the SDRdaemon instance will get tuned to.

<h4>7.2: Decimation factor</h4>

These are successive powers of two from 0 (1) to 6 (64). The SDRdaemon instance will decimate the samples coming from the attached hardware by this value. Thus the sample rate (see 7.5) will be decimated by the same value before it is sent over through the network.

<h4>7.3: Center frequency position</h4>

The center frequency in the passband wil be set either:

  - below the local oscillator (NCO) or infradyne. Actually -1/4th the bandwidth.
  - above the local oscillator (NCO) or supradyne. Actually +1/4th the bandwidth.
  - centered on the local oscillator or zero IF.
  
<h4>7.4: Send data to the distant SDRdaemon instance</h4>

Whenever any of the parameters change this button gets enabled. When clicked a message is sent on the configuration port of the distant machine to which the SDRdaemon listens for instructions. Leave time for the buffering system to stabilize to get the samples flow through normally.

<h4>7.5: Sample rate in kS/s</h4>

The sample rate of the hardware device attached to the SDRdaemon instance will be set to this value in kS/s.

<h4>7.6: Other parameters hardware specific</h4>

THese are the parameters that are specific to the hardware attached to the distant SDRdaemon instance. You have to know which device is attached to send the proper parameters. Please refer to the SDRdaemon documentation or its line help to get information on these parameters.