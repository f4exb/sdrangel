<h1>Map Feature Plugin</h1>

<h2>Introduction</h2>

The Map Feature plugin displays a world map in 2D and 3D. It can display street maps, satellite imagery as well as custom map types.
On top of this, it can plot data from other plugins, such as:

* APRS symbols from the APRS Feature,
* Aircraft from the ADS-B Demodulator,
* Ships from the AIS Demodulator,
* Satellites from the Satellite Tracker,
* Satellite imagery from APT Demodulator,
* The Sun, Moon and Stars from the Star Tracker,
* Weather balloons from the Radiosonde feature,
* RF Heat Maps from the Heap Map channel,
* Radials and estimated position from the VOR localizer feature,
* ILS course line and glide path from the ILS Demodulator,
* DSC geographic call areas,
* SID paths,
* Pager messages that contain coordinates.

As well as internet and built-in data sources:

* AM, FM and DAB transmitters in the UK and DAB transmitters in France,
* Airports, NavAids and airspaces,
* Beacons based on the IARU Region 1 beacon database and International Beacon Project,
* Radio time transmitters,
* GRAVES radar,
* Ionosonde station data,
* Navtex transmitters,
* VLF transmitters,
* KiwiSDRs,
* Spy Servers,
* Weather radar,
* Satellite infra-red data (clouds),
* Sea marks,
* Satellite imagery from NASA GIBS (Global Imagery Browse Services).

It can also create tracks showing the path aircraft, ships, radiosondes and APRS objects have taken, as well as predicted paths for satellites.

![2D Map feature](../../../doc/img/Map_plugin_beacons.png)

![3D Map feature](../../../doc/img/Map_plugin_apt.png)

3D Models are not included with SDRangel. They must be downloaded by pressing the Download 3D Models button in the Display Settings dialog (21).

<h2>Interface</h2>

![Map feature plugin GUI](../../../doc/img/Map_plugin.png)

<h3>1: Find</h3>

To centre the map on an object or location, enter:

* An object name.
* Latitude and longitude. This can be in decimal degrees (E.g: -23.666413, -46.573550) or degrees, minutes and seconds (E.g: 50°40'46.461"N 95°48'26.533"W or 33d51m54.5148sS 151d12m35.6400sE).
* A Maidenhead locator (E.g: IO86av).
* An address (E.g: St Katharine's & Wapping, London EC3N 4AB)

<h3>2: Map Type</h3>

Allows you to select a 2D map type. The available types will depend upon the Map provider
selected under Display Settings (7).

<h3>3: Maidenhead locator conversion</h3>

When checked, opens the Maidenhead locator converter dialog, which allows conversion between addresses, latitude and longitude and Maidenhead locators.

<h3>4: Display Beacon dialog</h3>

When clicked, opens the Beacon dialog. Initially, no beacons will be listed. To download the IARU Region 1 beacon list, click the download button in the top right.
The beacons will then be displayed in the table and on the map.

* Double clicking in a cell in the beacon table in the Callsign or Location columns, will centre the map on that beacon.
* Double clicking on the Frequency column will set the Device center frequency.

![Beacon dialog](../../../doc/img/Map_plugin_beacon_dialog.png)

<h3>5: Display IBP Beacon Project Dialog</h3>

When clicked, opens the International Beacon Project dialog. The IBP consists of 18 HF beacons around the world,
that each transmit once on each of the frequencies 14.100, 18.110, 21.150, 24.930 and 28.200MHz every 3 minutes, for 10 seconds.
The IBP dialog shows which of the beacons should currently be transmitting.

![IBP beacon dialog](../../../doc/img/Map_plugin_ibp_beacon_dialog.png)

* Double clicking in a cell in the Callsign column will centre the map on that beacon.
* Double clicking in a cell in the Frequency column will set the Device center frequency.

More details of the IBP can be found at: https://www.ncdxf.org/beacon/beaconfaq.html

<h3>6: Display Radio Time Transmitters dialog</h3>

When clicked, opens the Radio Time Transmitters dialog.

* Double clicking in a cell in the table in the Callsign or Location columns, will centre the map on that transmitter.
* Double clicking on the Frequency column will set the Device center frequency.

![Radio Time transmitters dialog](../../../doc/img/Map_plugin_radiotime_dialog.png)

<h3>7: Display Satellite Infrared</h3>

When checked, satellite infrared measurements (10.3um) are downloaded from the internet and are overlaid on the maps. 
This essentially shows cloud cover. The images are updated every 10 minutes.

![Satellite IR](../../../doc/img/Map_plugin_clouds.png)

The data is similar to that which can be received using the [APT Demodulator](../../channelrx/demodapt/readme.md) in the Thermal-infrared (10.3-11.3 um) channel.

This is only supported on 2D raster maps and the 3D map.

<h3>8: Display Weather Radar</h3>

When checked, weather radar measurements are downloaded from the internet and are overlaid on the maps. 
This shows rain and other forms of precipitation.
The images are updated every 10 minutes.

Green, yellow and red are rain, with red being the most intense. 
Light blue through dark blue is snow, with dark blue being the most intense.

![Weather Radar](../../../doc/img/Map_plugin_weather_radar.png)

This is only supported on 2D raster maps and the 3D map.

<h3>9: Display Sea Marks</h3>

When checked, sea marks are overlaid on the maps.

![Sea Marks](../../../doc/img/Map_plugin_seamarks.png)

![Sea Marks Legend](../../../doc/img/Map_plugin_seamarks_legend.png)

This is only supported on 2D raster maps and the 3D map.

<h3>10: Display Railways</h3>

When checked, railway routes are overlaid on the maps.

![Railways](../../../doc/img/Map_plugin_railways.png)

![Railway Legend](../../../doc/img/Map_plugin_railway_legend.png)

This is only supported on 2D raster maps and the 3D map.

<h3>11: Display MUF Contours</h3>

When checked, contours will be downloaded and displayed on the 3D map, showing the MUF (Maximum Usable Frequency) for a 3000km path that reflects off the ionosphere.
The contours will be updated every 15 minutes. MUF contour data is available for the preceding 5 days.

![MUF contours](../../../doc/img/Map_plugin_muf.png)

<h3>12: Display coF2 Contours</h3>

When checked, contours will be downloaded and displayed on the 3D map, showing coF2 (F2 layer critical frequency), the maximum frequency at which radio waves will be reflected vertically from the F2 region of the ionosphere.
The contours will be updated every 15 minutes. coF2 contour data is available for the preceding 5 days.

<h3>13: Display NASA GIBS Data</h3>

When checked, enables overlay of data from NASA GIBS (Global Imagery Browse Services). This includes a vast array of Earth observation satellite data,
such as land and sea temperatures, atmospheric conditions, flux measurements and the like.
Details of available data products can be found [here](https://nasa-gibs.github.io/gibs-api-docs/available-visualizations/#visualization-product-catalog).

For some data sets, GIBS has data spanning many decades. The data period may be hours, days or months. The 3D map will attempt to show data from the closest time set in the 3D map's timescale.
The 2D map will only show data from the default date (which is displayed in the table at the bottom).

![NASA GIBS](../../../doc/img/Map_plugin_GIBS.png)

This is only supported on 2D raster maps and the 3D map.

<h3>14: NASA GIBS Data</h3>

Selects which data from NASA GIBS to overlay on the maps.

<h3>15: NASA GIBS Opacity</h3>

Sets the opacity used for the NASA GIBS overlay image overlay on the 3D map. Lower values make the image more transparent.

<h3>16: Display Names</h3>

When checked, names of objects are displayed in a bubble next to each object.

<h3>17: Display tracks for selected object</h3>

When checked, displays the track (taken or predicted) for the selected object.

<h3>18: Display tracks for all objects</h3>

When checked, displays the track (taken or predicted) for the all objects.

<h3>19: Save to .kml</h3>

When clicked, items and tracks on the map will be saved to a [KML](https://en.wikipedia.org/wiki/Keyhole_Markup_Language) (.kml or .kmz) file, for use in other applications.

Note that the KML format requires 3D models in the Collada (.dae) format. However, SDRangel's models are in glTF (.glb or .gltf) format. 
If you wish to view the models in a KML viewer, you will need to manually convert them. Note that you should still be able to view tracks without the models.
Note that the .glbe files cannot be converted to .dae.

<h3>20: Delete</h3>

When clicked, all items will be deleted from the map.

<h3>21: Display settings</h3>

When clicked, opens the Map Display Settings dialog:

![Map Display Settings Dialog Maps Tab](../../../doc/img/Map_plugin_display_settings.png)

For the 2D map, the settings include:

* Whether the 2D map is displayed.
* Which Map provider will be used to source the map images.
* When OpenStreetMap is used as the provider, a custom map URL can be entered. For example, http://a.tile.openstreetmap.fr/hot/ or http://1.basemaps.cartocdn.com/light_nolabels/
  or file:///opt/install/custom-map if using a map stored locally, such a map created with [QGIS](https://qgis.org/) using the "Generate XYZ tiles (Directory)" tool
  or [gdal2tiles](https://gdal.org/programs/gdal2tiles.html)
  or any other tool that creates map images compatible with Google Maps, OpenLayers and Leaflet.
* When MapboxGL is used as the provider, custom styles can be specified.

For the 3D map, the settings include:

* Whether the 3D map is displayed.
* The terrain provider, which provides elevation data. For a "flat" globe, terrain can be set to Ellipsoid for the WGS-84 ellipsoid.
* The buildings provider, which provides 3D building models. This can be set to None if no buildings are desired.
* Whether the globe and models are lit from the direction of the Sun or the camera.
* The camera reference frame. For ECEF (Earth Centered Earth Fixed), the camera rotates with the globe.
For ECI (Earth Centred Inertial) the camera is fixed in space and the globe will rotate under it.

The "Download 3D Models" button will download the 3D models of aircraft, ships and satellites that are required for the 3D map.
These are not included with the SDRangel distribution, so must be downloaded. It is recommended to restart SDRangel after downloading the models.

![Map Display Settings Dialog Items Tab](../../../doc/img/Map_plugin_display_settings_items.png)

The Map Items tab customization of how objects from different SDRangel plugins and the Internet are displayed on the 2D and 3D maps. This includes:

* Whether images are displayed on the 2D map and whether 3D models are displayed on the 3D map.
* Whether labels are displayed giving the name of the object.
* Whether taken and predicted tracks are displayed and in which colour.
* How the image or 3D model is scaled as the zoom level changes.

![Map Display Settings Dialog API Keys Tab](../../../doc/img/Map_plugin_display_settings_apikeys.png)

API keys are required to access maps from different providers.

Free API keys are available by signing up for an accounts with:

* [Thunderforest](https://www.thunderforest.com/)
* [Maptiler](https://www.maptiler.com/)
* [Mapbox](https://www.mapbox.com/)
* [Cesium ion](https://cesium.com/ion/signup)

If API keys are not specified, a default key will be used, but this may not work if too many users use it.

<h3>Map</h3>

The map feature displays a 2D and a 3D map overlaid with objects reported by other SDRangel channels and features, as well as beacon locations.

* The "Home Station" antenna location is placed according to My Position set under the Preferences > My Position menu.
* To pan around the map, click the left mouse button and drag. To zoom in or out, use the mouse scroll wheel.
* Single clicking on an object in the map will display a text bubble with additional information about the object.
* Right clicking on a object on the 2D map will open a context menu, which allows:
  * To set an object as the target. The target object will have its azimuth and elevation displayed in the text bubble and sent to the Rotator Controller feature.
  * Setting the Device center frequency to the first frequency found in the text bubble for the object.
  * Changing the order in which the objects are drawn, which can help to cycle through multiple objects that are at the same location on the map.
  * Setting the object as the tracking target on the 3D map.
* Left double clicking while holding shift on the 3D map will place a marker showing the position. Right clicking will clear it.

The 2D map will only display the last reported positions for objects.
The 3D map, however, has a timeline that allows replaying how objects have moved over time.
To the right of the timeline is the fullscreen toggle button, which allows the 3D map to be displayed fullscreen.

<h4>SDRs</h4>

The map can display KiwiSDRs and Spy Servers that are publicly accessible via the internet. A URL is displayed in the info box.
Clicking on the URL will open a new KiwiSDR or RemoteTCPInput device which will connect to the corresponding SDR.
Before connecting, you should check the whether the number of users is below the maximum. Server data is updated every 2 minutes.

![SDRs](../../../doc/img/Map_plugin_SDRs.png)

<h4>Ionosonde Stations</h4>

When Ionosonde Stations are displayed, data is downloaded and displayed every 2 minutes. The data includes:

* MUF - Maximum Usable Frequency in MHz for 3000km path.
* M(D) - M-factor (~MUF/foF2) for 3000km path.
* foF2 - F2 region critical frequency in MHz.
* hmF2 - F2 region height in km.
* foE - E region critical frequency in MHz.
* TEC - Total Electron Content.

Each station is labelled on the maps as "MUF/foF2".

MUF and foF2 can be displayed as contours:

![MUF contours](../../../doc/img/Map_plugin_muf.png)

The contours can be clicked on which will display the data for that contour in the info box.

<h4>VLF Transmitters</h4>

The Map contains a built-in list of VLF transmitters. This can be overridden by a user-defined list contained in a file `vlftransmitters.csv` in the application data directory.

The file must have the following columns:

```
Callsign,Frequency,Latitude,Longitude,Power
GQD,19580,54.911643,-3.278456,10
```

<h2>Attribution</h2>

IARU Region 1 beacon list used with permission from: https://iaru-r1-c5-beacons.org/  To add or update a beacon, see: https://iaru-r1-c5-beacons.org/index.php/beacon-update/

Mapping and geolocation services are by Open Street Map: https://www.openstreetmap.org/ esri: https://www.esri.com/
Mapbox: https://www.mapbox.com/ Cesium: https://www.cesium.com Bing: https://www.bing.com/maps/

Ionosonde data and MUF/coF2 contours from [KC2G](https://prop.kc2g.com/) with source data from [GIRO](https://giro.uml.edu/) and [NOAA NCEI](https://www.ngdc.noaa.gov/stp/iono/ionohome.html).


Sea Marks are from OpenSeaMap: https://www.openseamap.org/

Railways are from OpenRailwayMap: https://www.openrailwaymap.org/

Weather radar and satellite data is from RainViewer: https://www.rainviewer.com

Icons made by Google from Flaticon: https://www.flaticon.com
World icons created by turkkub from Flaticon: https://www.flaticon.com
Layers and Boat icons created by Freepik from Flaticon: https://www.flaticon.com
Railway icons created by Prosymbols Premium from Flaticon: https://www.flaticon.com
Satellite icons created by SyafriStudio from Flaticon: https://www.flaticon.com
Pager icons created by xnimrodx from Flaticon: https://www.flaticon.com

3D models are by various artists under a variety of licenses. See: https://github.com/srcejon/sdrangel-3d-models

<h2>Creating 3D Models</h2>

If you wish to contribute a 3D model, see the https://github.com/srcejon/sdrangel-3d-models project.

<h2>API</h2>

Full details of the API can be found in the Swagger documentation. Here is a quick example of how to centre the map on an object from the command line:

    curl -X POST "http://127.0.0.1:8091/sdrangel/featureset/feature/0/actions" -d '{"featureType": "Map",  "MapActions": { "find": "M7RCE" }}'

And to centre the map at a particular latitude and longitude:

    curl -X POST "http://127.0.0.1:8091/sdrangel/featureset/feature/0/actions" -d '{"featureType": "Map", "MapActions": { "find": "51.2 0.0" }}'
