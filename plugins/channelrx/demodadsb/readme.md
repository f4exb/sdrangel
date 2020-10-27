<h1>ADS-B demodulator plugin</h1>

<h2>Introduction</h2>

This plugin can be used to receive and display ADS-B aircraft information. This is information about an aircraft, such as position, altitude, heading and speed, broadcast by aircraft on 1090MHz, in the 1090ES (Extended Squitter) format. 1090ES frames have a chip rate of 2Mchip/s, so the baseband sample rate should be set to be greater than 2MSa/s.

<h2>Interface</h2>

![ADS-B Demodulator plugin GUI](../../../doc/img/ADSBdemod_plugin.png)

<h3>1: Frequency shift from center frequency of reception value</h3>

Use the wheels to adjust the frequency shift in Hz from the center frequency of reception. Right click on a digit sets all digits on the right to zero. This effectively floors value at the digit position. Wheels are moved with the mousewheel while pointing at the wheel or by selecting the wheel with the left mouse click and using the keyboard arrows. Pressing shift simultaneously moves digit by 5 and pressing control moves it by 2. Left click on a digit sets the cursor position at this digit.

<h3>2: Channel power</h3>

Average total power in dB relative to a +/- 1.0 amplitude signal received in the pass band.

<h3>3: Level meter in dB</h3>

  - top bar (green): average value
  - bottom bar (blue green): instantaneous peak value
  - tip vertical bar (bright green): peak hold value

<h3>4: RF bandwidth</h3>

This is the bandwidth in MHz of the channel signal before demodulation.

<h3>5: SR - Channel Sample Rate</h3>

This specifies the channel sample rate the demodulator uses. Values of 4M-12MSa/s are supported, 2MSa/s steps. Ideally, this should be set to the same value as the baseband sample rate (the sample rate received from the radio). If it is different from the baseband sample rate, interpolation or decimation will be performed as needed to match the rates. However, interpolation currently requires a significant amount of processing power and may result in dropped samples.

Higher channel sample rates may help decode more frames, but will require more processing power.

<h3>6: Threshold</h3>

This sets the correlation threshold between the received signal and expected 1090ES preamble, that is required to be exceeded before the demodulator will try to decode a frame. Lower values should decode more frames, but will require more processing power.

<h3>7: Feed</h3>

Checking Feed enables feeding received ADS-B frames to aggregators such as ADS-B Exchange: https://www.adsbexchange.com The server name and port to send the frames to should be entered in the Server and Port fields. For ADS-B Exchange, set Server to feed.adsbexchange.com and Port to 30005. You can check if you are feeding data to ADS-B Exchange (after about 30 seconds) at: https://www.adsbexchange.com/myip/ Frames are forwarded in the Beast binary format as described here: https://wiki.jetvision.de/wiki/Mode-S_Beast:Data_Output_Formats

<h3>ADS-B Data</h3>

The table displays the decoded ADS-B data for each aircraft. The data is not all able to be transmitted in a single ADS-B frame, so the table displays an amalgamation of the latest received data of each type.

![ADS-B Demodulator Data](../../../doc/img/ADSBdemod_plugin_table.png)

* ICAO ID - 24-bit hexidecimal ICAO aircraft address. This is unique for each aircraft.
* Flight No. - Airline flight number or callsign.
* Latitude - Vertical position coordinate, in decimal degrees.
* Longitude - Horizontal position coordinate, in decimal degrees.
* Altitude - Altitude in feet.
* Speed - Speed (either ground speed, indicated airspeed, or true airspeed) in knots.
* Heading - The direction the aircraft is heading, in degrees.
* Climb - The vertical climb rate (or descent rate if negative) in feet/minute.
* Categoty - The vehicle category, such as Light, Large, Heavy or Rotorcraft.
* Range - The range (distance) to the aircraft from the receiving antenna in kilometres. The location of the receiving antenna is set under the Preferences > My Position menu.
* Az/El - The azimuth and elevation angles to the aircraft from the receiving antenna in degrees. These values can be sent to a rotator controller to track the aircraft.
* Updated - The local time at which the last ADS-B message was received.
* RX Frames - A count of the number of ADS-B frames received from this aircraft.
* Correlation - Displays the minimum, average and maximum of the preamable correlation for each recevied frame. These values can be used to help select a threshold setting.

A suffix of L in the latitude and longitude columns, indicates the position is based on a local decode, using a single received frame and the position of the radio station. No suffix will be added for a global decode, which is based upon receving and odd and even frame.
If an ADS-B frame has not been received from an aircraft for 60 seconds, the aircraft is removed from the table and map.

Double clicking in an ICAO ID cell will open a Web browser and search for the corresponding aircraft on https://www.planespotters.net/
Double clicking in an Flight No cell will open a Web browser and search for the corresponding flight on https://www.flightradar24.com/
Double clicking in an Az/El cell will set the aircraft as the active target. The azimuth and elevation to the aicraft will be sent to a rotator controller plugin. The aircraft text will be coloured green, rather than blue, on the map.
Double clicking on any other cell in the table will centre the map on the corresponding aircraft.

<h3>Map</h3>

The map displays aircraft locations and data geographically.

![ADS-B Demodulator Map](../../../doc/img/ADSBdemod_plugin_map.png)

The initial antenna location is placed according to My Position set under the Preferences > My Position menu. The position is only updated when the ADS-B demodulator plugin is first opened.

Aircraft are only placed upon the map when a position can be calculated, which can require several frames to be received.

To pan around the map, click the left mouse button and drag. To zoom in or out, use the mouse scroll wheel.

<h3>Attribution</h3>

Icons are by Alice Design, Alex Ahineev, Verry Obito, Sean Maldjia, Tinashe Mugayi, Andreas Vögele and Angriawan Ditya Zulkarnain from the Noun Project https://thenounproject.com/
