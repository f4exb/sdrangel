<h1>AIS demodulator plugin</h1>

<h2>Introduction</h2>

This plugin can be used to demodulate AIS (Automatic Identification System) messages. AIS can be used to track ships and other marine vessels at sea, that are equipped with AIS transponders. It is also used by shore-side infrastructure known as base stations, aids-to-navigation such as buoys and some search and rescue aircraft.

AIS is broadcast globally on 25kHz channels at 161.975MHz and 162.025MHz, with other frequencies being used regionally or for special purposes. This demodulator is single channel, so if you wish to decode multiple channels simultaneously, you will need to add one AIS demodulator per frequency. As most AIS messages are on 161.975MHz and 162.025MHz, you can set the center frequency as 162MHz, with a sample rate of 100k+Sa/s, with one AIS demod with an input offset -25kHz and another at +25kHz.

The AIS demodulators can send received messages to the [AIS feature](../../feature/ais/readme.md), which displays a table combining the latest data for vessels amalgamated from multiple demodulators and sends their positions to the [Map Feature](../../feature/map/readme.md) for display in 2D or 3D.

AIS uses GMSK/FM modulation at a baud rate of 9,600, with a modulation index of 0.5. The demodulator works at a sample rate of 57,600Sa/s.

Received AIS messages can be NMEA encoded and forwarded via UDP to 3rd party applications.

The AIS specification is ITU-R M.1371-5: https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.1371-5-201402-I!!PDF-E.pdf

<h2>Interface</h2>

The top and bottom bars of the channel window are described [here](../../../sdrgui/channel/readme.md)

![AIS Demodulator plugin GUI](../../../doc/img/AISDemod_plugin.png)

<h3>1: Frequency shift from center frequency of reception</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Left click on a digit sets the cursor position at this digit. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2.

<h3>2: Channel power</h3>

Average total power in dB relative to a +/- 1.0 amplitude signal received in the pass band.

<h3>3: Level meter in dB</h3>

  - top bar (green): average value
  - bottom bar (blue green): instantaneous peak value
  - tip vertical bar (bright green): peak hold value

<h3>4: BW - RF Bandwidth</h3>

This specifies the bandwidth of a LPF that is applied to the input signal to limit the RF bandwidth. While AIS channels are 25kHz wide, more messages seem to be able to be received if this is around 16kHz.

<h3>5: Dev - Frequency deviation</h3>

Adjusts the expected peak frequency deviation in 0.1 kHz steps from 1 to 6 kHz. The default is 2.4 kHz: ITU-R M.1371-5 section 2.3.2 specifies a modulation index of 0.5, and for continuous phase modulation the modulation index is twice the peak deviation divided by the bit rate, so 0.5 at 9,600 bit/s gives a peak deviation of 2,400 Hz. (4,800 Hz is the separation between the mark and space frequencies, which is twice the deviation.)

This setting scales the output of the FM discriminator, which is used to detect the preamble and drive the scope traces. It has no direct effect on the demodulator's symbol decisions.

<h3>6: TH - Correlation Threshold</h3>

The threshold for the normalised correlation between the received signal and the preamble (training sequence), from 0 to 1. Being normalised, it does not depend on signal level, so the same setting works for strong and weak signals. Real preambles correlate above 0.9 and the default of 0.6 sits well clear of the noise floor. A lower value may demodulate slightly weaker signals, but increases processor usage sharply, because every threshold crossing starts a sequence detection.

<h3>7: UDP</h3>

When checked, received messages are forwarded to the specified UDP address (8) and port (9).

<h3>8: UDP address</h3>

IP address of the host to forward received messages to via UDP.

<h3>9: UDP port</h3>

UDP port number to forward received messages to.

<h3>10 UDP format</h3>

The format the messages are forwarded via UDP in. This can be either binary (which is useful for SDRangel's PERTester feature) or NMEA (which is useful for 3rd party applications such as OpenCPN).

<h3>11: Find</h3>

Entering a regular expression in the Find field displays only messages where the source MMSI matches the given regular expression.

<h3>12: Use Date and Time from File</h3>

When checked, if the source device is a File Input device, the date and time used for
packet reception time is taken from the file playback time. Otherwise, the current system clock time is used.

<h3>13: Show / hide Slot Map</h3>

When checked shows the slot map (See below).

<h3>14: Start/stop Logging Messages to .csv File</h3>

When checked, writes all received AIS messages to a .csv file.

<h3>15: .csv Log Filename</h3>

Click to specify the name of the .csv file which received AIS messages are logged to.

<h3>16: Read Data from .csv File</h3>

Click to specify a previously written .csv log file, which is read and used to update the table.

<h3>17: Clear Messages from table</h3>

Pressing this button clears all messages from the table.

<h3>Slot Map</h3>

AIS uses TMDA (Time Division Multiple Access), whereby each one minute frame is divided into 2,250 26.6ms slots.
The slot map shows which slots within a frame are used. The slot map is drawn as bitmap of 50x45 pixels.

![AIS Slot Map](../../../doc/img/AISDemod_plugin_slotmap.png)

Slots are by category:

* Red: Class A Mobile
* Blue: Class B Mobile
* Green: Base Station
* Yellow: AtoN (Aid-to-Navigation)
* Cyan: Search and Rescue
* Magenta: Other (Man overboard / EPIRB / AMRD).

Due to SDR to SDRangel latency being unknown, the slot map is likely to have some offset, as slot timing is calculated based on the time messages
are demodulated in SDRangel.

<h3>Received Messages Table</h3>

The received messages table displays information about each AIS message received. Only messages with valid CRCs are displayed.

![AIS Received Messages Table](../../../doc/img/AISDemod_plugin_messages.png)

* Date - The date the message was received.
* Time - The time the message was received.
* MMSI - The Maritime Mobile Service Identity number of the source of the message. Double clicking on this column will search for the MMSI on https://www.vesselfinder.com/
* Country - The country with jurisdiction over station/vessel.
* Type - The type of AIS message. E.g. Position report, Base station report or Ship static and voyage related data.
* Id - Message type numeric identifier.
* Data - A textual decode of the message displaying the most interesting fields.
* NMEA - The message in NMEA format.
* Hex - The message in hex format.
* Slot - Time slot (0-2249).

Right clicking on the table header allows you to select which columns to show. The columns can be reordered by left clicking and dragging the column header. Right clicking on an item in the table allows you to copy the value to the clipboard.
