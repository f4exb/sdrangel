<h1>LoRa demodulator plugin</h1>

<h2>Introduction</h2>

This plugin can be used to demodulate and decode LoRa transmissions.

  - To get an idea of what is LoRa: [here](https://www.link-labs.com/blog/what-is-lora)
  - A detailed inspection of LoRa modulation and protocol: [here](https://static1.squarespace.com/static/54cecce7e4b054df1848b5f9/t/57489e6e07eaa0105215dc6c/1464376943218/Reversing-Lora-Knight.pdf)

This plugin has been designed to work in conjunction with the LoRa modulator plugin. However it can receive transmissions from the RN2483 module when the Distance Enhancement is engaged (4 FFT bins per symbol) which happens with spread factors 11 and 12. Il is very difficult in general to get good decodes when only one FFT bin is used by symbol. It has not been tested with Semtech SX127x hardware.

This plugin is designed to experiment with chirp modulation and LoRa technique. It does not replace dedicated hardware for production grade links.

Note: this plugin is available in version 5 only (since 5.2.0).

<h2>Interface</h2>

![LoRa Demodulator plugin GUI](../../../doc/img/LoRaDemod_plugin.png)

<h3>1: Frequency shift from center frequency of reception</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

<h3>2: De-chirped channel power</h3>

This is the total power in the FFT of the de-chirped signal in dB. When no LoRa signal is detected this corresponds to the power received in the bandwidth (3). It will show a significant increase in presence of a LoRa signal that can be detected.

<h3>3: Bandwidth</h3>

This is the bandwidth of the LoRa signal. AS per LoRa specs the signal sweeps between the lower and the upper frequency of this bandwidth. The sample rate of the LoRa signal in seconds is exactly one over this bandwidth in Hertz.

In the LoRa standard there are 2 base bandwidths: 500 and 333.333 kHz. A 400 kHz base has been added. Possible bandwidths are obtained by a division of these base bandwidths by a power of two from 1 to 64. An extra divisor of 128 is provided to achieve smaller bandwidths that can fit in a SSB channel.

Thus available bandwidths are:

  - **500000** (500000 / 1) Hz
  - **400000** (400000 / 1) Hz not in LoRa standard
  - **333333** (333333 / 1) Hz
  - **250000** (500000 / 2) Hz
  - **200000** (400000 / 2) Hz not in LoRa standard
  - **166667** (333333 / 2) Hz
  - **125000** (500000 / 4) Hz
  - **100000** (400000 / 4) Hz not in LoRa standard
  - **83333** (333333 / 4) Hz
  - **62500** (500000 / 8) Hz
  - **50000** (400000 / 8) Hz not in LoRa standard
  - **41667** (333333 / 8) Hz
  - **31250** (500000 / 16) Hz
  - **25000** (400000 / 16) Hz not in LoRa standard
  - **20833** (333333 / 16) Hz
  - **15625** (500000 / 32) Hz
  - **12500** (400000 / 32) Hz not in LoRa standard
  - **10417** (333333 / 32) Hz
  - **7813** (500000 / 64) Hz
  - **6250** (400000 / 64) Hz not in LoRa standard
  - **5208** (333333 / 64) Hz
  - **3906** (500000 / 128) Hz not in LoRa standard
  - **3125** (400000 / 128) Hz not in LoRa standard
  - **2604** (333333 / 128) Hz not in LoRa standard

The LoRa signal is oversampled by two therefore it needs a baseband of at least twice the bandwidth. This drives the maximum value on the slider automatically.

<h3>4: De-chirped noise maximum power</h3>

This is the maximum power received in one FFT bin (the argmax bin) in dB when no signal is detected. It is averaged over 10 values.

<h3>5. De-chirped signal maximum power</h3>

This is the maximum power received in one FFT bin (the argmax bin) in dB when a signal is detected. It is averaged over 10 values.

<h3>6: De-chirped signal over noise ratio</h3>

The noise level reference is the noise maximum power just before the detected signal starts and the signal level the signal maximum power just before the detected signal stops. To get a significant reading you have to adjust correctly the number of preamble chirps (9) and the End Of Message squelch level (A.3) and/or the message length (A.4) so that signal boundaries are determined correctly.

Decode errors are very likely to happen when this value falls below 4 dB.

<h3>7: Spread Factor</h3>

This is the Spread Factor parameter of the LoRa signal. This is the log2 of the FFT size used over the bandwidth (3). The number of symbols is 2<sup>SF-DE</sup> where SF is the spread factor and DE the  Distance Enhancement factor (8)

<h3>8: Distance Enhancement factor</h3>

The LoRa standard specifies 0 (no DE) or 2 (DE active). The present range is extended to all values between 0 and 4 bits.

This is the log2 of the number of FFT bins used for one symbol. Extending the numbe of FFT bins per symbol decreases the probabilty to detect the wrong symbol as an adjacent bin. It can also overcome frequency drift on long messages.

In practice it is difficult to make correct decodes if only one FFT bin is used to code one symbol (DE=0).

<h3>A: Payload controls and indicators</h3>

![LoRa Demodulator payload controls](../../../doc/img/LoRaDemod_payload.png)

<h4>A.1: Coding scheme</h4>

In addition to the LoRa standard plain ASCII and TTY have been added for pure text messages. ASCII and TTY have no form of FEC.

  - **LoRa**: LoRa standard (see LoRa documentation)
  - **ASCII**: This is plain 7 bit ASCII coded characters. It needs exactly 7 effective bits per symbols (SF - DE = 7)
  - **TTY**: Baudot (Teletype) 5 bit encoded characters. It needs exactly 5 effective bits per symbols (SF - DE = 5)

<h4>A.2: Start/Stop decoder</h4>

You can suspend and resume decoding activity using this button. This is useful if you want to freeze the payload content display.

<h4>A.3: End Of Message squelch</h4>

This is used to determine the end of message automatically. It can be de-activated by turning the button completely to the right (as shown on the picture). In this case it relies on the message length set with (A.4).

During paylaod detection the maximum power value in the FFT (at argmax) P<sub>max</sub> is stored and compared to the current argmax power value P<sub>i</sub> if S<sub>EOM</sub> is this squelch value the end of message is detected if S<sub>EOM</sub> &times; S<sub>i</sub> &lt; S<sub>max</sub>

<h4>A.4: Expected message length in symbols</h4>

This is the expected number of symbols in a message. When a header is present in the payload it should match the size given in the header (A.11).

<h4>A.5: Auto mesasge length</h4>

LoRa mode only. Set message length (A.4) equal to the number of symbols specified in the message just received. When messages are sent repeatedly this helps adjusting in possible message length changes automatically.

<h4>A.6: Sync word</h4>

This is the message 1 byte sync word displayed in hexadecimal.

<h4>A.7: Expect header in message</h4>

LoRa mode only. Use this checkbox to tell if you expect or not a header in the message.

<h4>A.8: Number of FEC parity bits</h4>

LoRa mode only. This is the number of parity bits in the Hamming code used in the FEC. The standard values are 1 to 4 for H(4,5) to H(4,8) encoding. 0 is a non-standard value to specify no FEC.

When a header is expected this control is disabled because the value used is the one found in the header.

<h4>A.9: Payload CRC presence</h4>

Use this checkbox to tell if you expect a 2 byte CRC at the end of the payload.

When a header is expected this control is disabled because the value used is the one found in the header.

<h4>A.10: Packet length</h4>

This is the expected packet length in bytes without header and CRC.

When a header is expected this control is disabled because the value used is the one found in the header.

<h4>A.11: Number of symbols and codewords</h4>

This is the number of symbols (left of slash) and codewords (right of slash) used for the payload including header and CRC.

<h4>A.12: Header FEC indicator</h4>

Header uses a H(4,8) FEC. The color of the indicator gives the status of header parity checks:

  - **Grey**: undefined
  - **Red**: unrecoverable error
  - **Blue**: recovered error
  - **Green**: no errors

<h4>A.13: Header CRC indicator</h4>

The header has a one byte CRC. The color of this indicator gives the CRC status:

  - **Green**: CRC OK
  - **Red**: CRC error

<h4>A.14: Payload FEC indicator</h4>

The color of the indicator gives the status of payload parity checks:

  - **Grey**: undefined
  - **Red**: unrecoverable error. H(4,7) cannot distinguish between recoverable and unrecoverable error. Therefore this is never displayed for H(4,7)
  - **Blue**: recovered error
  - **Green**: no errors

<h4>A.13: Payload CRC indicator</h4>

The payload can have a two byte CRC. The color of this indicator gives the CRC status:

  - **Grey**: No CRC
  - **Green**: CRC OK
  - **Red**: CRC error

<h3>10: Message text window</h3>

This is where the messages formatted as strings are displayed. Use the sweep icon to clear both message text (10) and message binary (11) windows.

The format of a message line is the following:

![LoRa Demodulator message string window](../../../doc/img/LoRaDemod_message_string.png)

  - 1: Timestamp in HH:MM:SS format
  - 2: Sync word
  - 3: Message

<h3>11: Binary payload window</h3>

This is to display the message payload as hex formatted bytes with mesasge status.

![LoRa Demodulator message bytes window](../../../doc/img/LoRaDemod_message_binary.png)

<h4>11.1: Timestamp in HH:NN:SS format</h4>

<h4>11.2: Sync word</h4>

This is the sync word (byte) displayed in hex. Corrsponds to (A.5) in the current message.

<h4>11.3: De-chirped signal level</h4>

This is the de-chirped signal level in dB. Corrsponds to (5) in the current message.

<h4>11.4: De-chirped signal to noise ratio</h4>

This is the de-chirped signal to noise ratio in dB. Corrsponds to (6) in the current message.

<h4>11.5: Header FEC status</h4>

  - **n/a**: unknown or not applicable
  - **err**: unrecoverable error
  - **fix**: corrected error
  - **ok**: OK

Corresponds to (A.12) indicator in the current message

<h4>11.6: Header CRC status</h4>

  - **ok**: CRC OK
  - **err**: CRC error
  - **n/a**: not applicable

Corresponds to (A.13) indicator in the current message

<h4>11.7: Payload FEC status</h4>

  - **n/a**: unknown or not applicable
  - **err**: unrecoverable error
  - **fix**: corrected error
  - **ok**: OK

Corresponds to (A.14) indicator in the current message

If the end of message is reached before expectation then `ERR: too early` is displayed instead and no payload CRC status (next) is displayed

<h4>11.8: Payload CRC status</h4>

  - **ok**: CRC OK
  - **err**: CRC error
  - **n/a**: not applicable

Corresponds to (A.15) indicator in the current message

<h4>11.9: Displacement at start of line</h4>

16 bytes in 4 groups of 4 bytes are displayed per line starting with the displacement in decimal.

<h4>11.10: Bytes group</h4>

This is a group of 4 bytes displayed as hexadecimal values. The payload is displayed with its possible CRC and without the header.

<h3>12: Send message via UDP</h3>

Select to send the decoded message via UDP.

<h3>13: UDP address and port</h3>

This is the UDP address and port to where the decoded message is sent when (12) is selected.

<h3>B: De-chirped spectrum</h3>

This is the spectrum of the de-chirped signal when a LoRa signal can be decoded.

The frequency span corresponds to the bandwidth of the LoRa signal (3). Default FFT size is 2<sup>SF</sup> where SF is the spread factor (7).

Sequences of successful LoRa signal demodulation are separated by blank lines (genreated with a string of high value bins).

Controls are the usual controls of spectrum displays with the following restrictions:

  - The window type is non operating because the FFT analysis of the LoRa signal is always done with a Kaiser window
  - The FFT size can be changed however it is set to 2<sup>SF</sup> where SF is the spread factor and thus displays correctly
