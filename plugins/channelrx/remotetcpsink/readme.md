<h1>Remote TCP sink channel plugin</h1>

<h2>Introduction</h2>

This plugin sends I/Q samples from the baseband via TCP/IP across a network to a client application.
The client application could be SDRangel using the [Remote TCP Input](../../samplesource/remotetcpinput/readme.md) plugin or a rtl_tcp compatible application.
This means that applications using rtl_tcp protocol can connect to the wide variety of SDRs supported by SDRangel.

<h2>Interface</h2>

![Remote TCP sink channel plugin GUI](../../../doc/img/RemoteTCPSink.png)

<h3>1: Frequency shift from center frequency of reception</h3>

This is the shift of the channel center frequency from the device center frequency.
This is used to select the desired part of the signal when the channel sample rate is lower than the baseband sample rate.

<h3>2: Gain</h3>

Sets a gain figure in dB that is applied to I/Q samples before transmission via TCP/IP.
This option may be useful for amplifying very small signals from SDRs with high-dynamic range (E.g. 24-bits), when the network sample bit-depth is 8-bits.

<h3>3: Sample rate</h3>

Specifies the channel and network sample rate in samples per second. If this is different from the baseband sample rate, the baseband signal will be decimated to the specified rate.

<h3>4: Sample bit depth</h3>

Specifies number of bits per I/Q sample transmitted via TCP/IP.

<h3>5: IP address</h3>

IP address of the local network interface on which the server will listen for TCP/IP connections from network clients.

<h3>6: Port</h3>

TCP port on which the server will listen for connections.

<h3>7: Protocol</h3>

Specifies the protocol used for sending IQ samples and metadata to clients via TCP/IP.

- RTL0: Compatible with rtl_tcp - limited to 8-bit IQ data.
- SDRA: Enhanced version of protocol that allows device settings to be sent to clients and for higher bit depths to be used (8, 16, 24 and 32).
