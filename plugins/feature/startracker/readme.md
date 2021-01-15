<h1>Star Tracker Feature Plugin</h1>

<h2>Introduction</h2>

The Star Tracker feature plugin is for use in radio astronomy and EME (Earth-Moon-Earth) communication.
It calculates the azimuth and elevation of celestial objects and can send them to the Rotator Controller or other plugins to point an antenna at that object.
The overhead position of the Sun, Moon and selected star can be displayed on the Map Feature.
The plugin can communicate with Stellarium, allowing Stellarium to control SDRangel as though it was a telescope and for the direction the antenna is pointing to be displayed in Stellarium.

<h2>Interface</h2>

![Star Tracker feature plugin GUI](../../../doc/img/StarTracker_plugin.png)

<h3>1: Start/Stop plugin</h3>

This button starts or stops the plugin. The plugin will only calculate azimuth and elevation or communicate with Stellarium when started.

<h3>2: Find target on map</h3>

Pressing this button centres the Map Feature (if open) on the current target.

<h3>3: Set latitude and longitude from My Position</h3>

When clicked, it sets the latitude, longitude and height fields to the values from SDRangel's My Position preferences.

<h3>4: Show settings dialog</h3>

Pressing this button displays a settings dialog, that allows you to set:

* The epoch used when entering RA and Dec. This can be either J2000 (which is used for most catalogues) or JNOW which is the current date and time.
* The units used for the display of the calculated azimuth and elevation. This can be either degrees, minutes and seconds or decimal degrees.
* Whether to correct for atmospheric refaction. You can choose either no correction, the Saemundsson algorithm, typically used for optical astronomy or the more accurate Positional Astronomy Library calculation, which can be used for >250MHz radio frequencies or light. Note that there is only a very minor difference between the two.
* Air pressure in millibars for use in refraction correction.
* Air temperature in degrees Celsius for use in refraction correction.
* Relative humidity in % for use in refraction correction.
* Height above sea level in metres for use in refraction correction.
* Temperature lapse rate in Kelvin per kilometer for use in refraction correction.
* Radio frequency being observed in MHz for use in refraction correction.
* The update period in seconds, which controls how frequently azimuth and elevation are re-calculated.
* The IP port number the Stellarium server listens on.
* Whether to start a Stellarium telescope server.
* Whether to draw the Sun in the map.
* Whether to draw the Moon on the map.
* Whether to draw the target star (or galaxy) on the map.

<h3>5: Latitude</h3>

Specifies the latitude in decimal degrees of the observation point (antenna location).

<h3>6: Longitude</h3>

Specifies the longitude in decimal degrees of the observation point (antenna location).

<h3>7: Time</h3>

Select the date and time at which the position of the target should be calculated. Select either Now, for the current time, or Custom to manually enter a date and time.

<h3>8: Target</h3>

Select a target object to track from the list.
To manually enter RA (right ascension) and Dec (declination) of an unlisted target, select Custom.
To allow Stellarium to set the RA and Dec, select Custom, and ensure the Stellarium Server option is checked in the Star Tracker Settings dialog.

| Target           | Type              | Details                                        | Flux density (Jy)                          |
|------------------|-------------------|------------------------------------------------|---------------------------------------------
| Sun              | Star              | Targets our Sun                                | 10k-10M (50MHz), 500k-10M (1.4GHz)         |
| Moon             | Moon              | Targets our Moon                               | 2 (50MHz), 1000 (1.4GHz)                   |
| PSR B0329+54     | Pulsar            | Strongest in Northern hemisphere (J0332+5434)  | 1.8 (50MHz), 1.5 (400MHz), 0.2 (1.4GHz)    |
| PSR B0833-45     | Pulsar            | Strongest in Southern hemisphere (J0835-4510)  | 5.4 (150MHz), 5.0 (400MHz), 1.0 (1.4GHz)   |
| Sagittarius A    | Galatic centre    | First detected source of extrasolar radio      | ~0.5 (<1GHz) for Sgr A*                    |
| Cassiopeia A     | Supernova         | Brightest extrasolar radio source              | 27k (50MHz), 10k (150MHz), 1768 (1.4GHz)   |
| Cygnus A         | Galaxy            | First radio galaxy                             | 22k (50MHz), 11k (150MHz), 1579 (1.4GHz)   |
| Taurus A (M1)    | Supernova/Pulsar  | Crab Nebular                                   | 2008 (50MHz), 1368 (150MHz), 829 (1.4GHz)  |
| Virgo A (M87)    | Galaxy            |                                                | 2635 (50MHz), 1209 (150MHz), 212 (1.4GHz)  |
| Custom           |                   | Manually enter RA and Dec                      |                                            |

References:

* ATNF Pulsar Catalogue - https://www.atnf.csiro.au/research/pulsar/psrcat/
* Cassiopeia A, Cygnus A, Taurus A, and Virgo A at ultra-low radio frequencies - https://research.chalmers.se/publication/516438/file/516438_Fulltext.pdf
* Repeating Jansky - https://www.gb.nrao.edu/~fghigo/JanskyAntenna/RepeatingJansky_memo10.pdf

<h3>9: Right Ascension</h3>

When target is set to Custom, you can specify the right ascension in hours of the target object. This can be specified as a decimal (E.g. 12.23, from 0 to 24) or in hours, minutes and seconds (E.g. 12h05m10.2s or 12 05 10.2). Whether the epoch is J2000 or JNOW can be set in the Star Tracker Settings dialog.

<h3>10: Declination</h3>

When target is set to Custom, you can specify the declination in degrees of the target object. This can be specified as a decimal (E.g. 34.6, from -90.0 to 90.0) or in degrees, minutes and seconds (E.g. 34d12m5.6s, 34d12'5.6"  34 12 5.6). Whether the epoch is J2000 or JNOW can be set in the Star Tracker Settings dialog.

<h3>11: Azimuth</h3>

Displays the calculated azimuth (angle in degrees, clockwise from North) to the object.

<h3>12: Elevation</h3>

Displays the calculated elevation (angle in degrees - 0 to horizon and 90 to zenith) to the object.

<h3>13: Elevation vs Time Plot</h3>

In order to assit in determining whether and when observations of the target object may be possible, an elevation vs time plot is drawn for the 24 hours encompassing the selected date and time.
Some objects may not be visible from a particular latitude for the specified time, in which case, the grahp title will indicate the object is not visible on that particular date.

<h2>Map</h2>

The Star Tracker feature can send the overhead position of the Sun, Moon and target Star to the Map. These can be enabled individually in the settings dialog. The Moon should be displayed with an approximate phase. Stars (or galaxies) are displayed as an image of a pulsar.

![StarTracker map](../../../doc/img/StarTracker_map.png)

When using the Find feature in the Map GUI, you can search for "Sun", "Moon" or "Star".

<h2>Stellarium Interface</h2>

In Star Tracker:

* Set target to Custom
* Press Show settings dialog and ensure Stellarium server is checked
* Press Start

Then in Stellarium:

* Enable Telescope Control plugin and restart
* Press the telescope button in the main toolbar
* Press "Configure telescopes..."
* Press "Add a new telescope"
* Set "Telescope controlled by" to "External softare or a remote computer"
* Set "Name" to "SDRangel" (Optional)
* Set "Coordinate system" to "J2000 (default)"
* Press OK
* Press Connect
* Enter Right Ascension/Declination or press "Current object" to get RA/Dec of currently selected object
* Press "Slew" to send the RA/Dec to Star Tracker

Star Tracker will continually send the RA/Dec of its target to Stellarium and this should be displayed in Stellarium with a crosshair/reticle and the label SDRangel (or whatever name you entered for the telescope).

To see the rough field of view of your antenna, open the Ocular configuration window and under Eyepieces, add a new eyepiece with name SDRangel.
Set aFOV to the half-power beam width of your antenna, focal length to 100 and field stop to 0.
Then select the SDRangel telescope reticle and press Ocular view.

<h2>Attribution</h2>

Icons are by Adnen Kadri and Erik Madsen, from the Noun Project Noun Project: https://thenounproject.com/

Icons are by Freepik from Flaticon https://www.flaticon.com/

<h2>API</h2>

Full details of the API can be found in the Swagger documentation. Here is a quick example of how to set the target to the Moon at the current time:

    curl -X PATCH "http://127.0.0.1:8091/sdrangel/featureset/0/feature/0/settings" -d '{"featureType": "StarTracker",  "StarTrackerSettings": { "target": "Moon", "dateTime": "" }}'

Or to a custom RA and declination on a given date and time:

    curl -X PATCH "http://127.0.0.1:8091/sdrangel/featureset/0/feature/0/settings" -d '{"featureType": "StarTracker",  "StarTrackerSettings": { "target": "Custom", "ra": "03h32m59.35s", "dec": "54d34m45.05s", "dateTime": "1921-04-15T10:17:05" }}'

To start tracking:

    curl -X POST "http://127.0.0.1:8091/sdrangel/featureset/0/feature/0/run"
