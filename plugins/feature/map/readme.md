<h1>Map Feature Plugin</h1>

<h2>Introduction</h2>

The Map Feature plugin displays a world map. On top of this, it can plot data from other plugins, such as APRS symbols from the APRS Feature, aircraft from the ADS-B Demodulator or the Sun, Moon and Stars from the Star Tracker.

<h2>Interface</h2>

![Map feature plugin GUI](../../../doc/img/Map_plugin.png)

<h3>1: Source Channels</h3>

This displays the list of channels the Map is displaying data from.

<h3>2: Find</h3>

To centre the map on an object or location, enter:

* An object name.
* Latitude and longitude. This can be in decimal degrees (E.g: -23.666413, -46.573550) or degrees, minutes and seconds (E.g: 50°40'46.461"N 95°48'26.533"W or 33d51m54.5148sS 151d12m35.6400sE).
* A Maidenhead locator (E.g: IO86av).

<h3>3: Display Names</h3>

When checked, names of objects are displayed in a bubble next to each object.

<h3>4: Delete</h3>

When clicked, all items will be deleted from the map.

<h3>Map</h3>

The map displays objects reported by other SDRangel channels and features.

* The antenna location is placed according to My Position set under the Preferences > My Position menu. The position is only updated when the Map plugin is first opened.
* To pan around the map, click the left mouse button and drag. To zoom in or out, use the mouse scroll wheel.
* Clicking on an object in the map will display a text bubble with additional information about the object.

<h2>API</h2>

Full details of the API can be found in the Swagger documentation. Here is a quick example of how to centre the map on an object from the command line:

    curl -X POST "http://127.0.0.1:8091/sdrangel/featureset/0/feature/0/actions" -d '{"featureType": "Map",  "MapActions": { "find": "M7RCE" }}'

And to centre the map at a particular latitude and longitude:

    curl -X POST "http://127.0.0.1:8091/sdrangel/featureset/0/feature/0/actions" -d '{"featureType": "Map", "MapActions": { "find": "51.2 0.0" }}'
