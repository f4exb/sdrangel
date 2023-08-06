<h1>Rotator Controller Feature Plugin</h1>

<h2>Introduction</h2>

The Rotator Controller feature plugin allows SDRangel to send commands to GS-232 and SPID rotators as well as hamlib's rotctld, via a serial or TCP connection.
This allows SDRangel to point antennas mounted on a rotator to a specified azimuth and elevation.

Azimuth and elevation can be set manually by a user in the GUI, via the REST API, via another plugin, such as the Map Feature, the ADS-B Demodulator, or the Star Tracker, or by controller/gamepads (such as an XBox Wireless Controller).

<h2>Interface</h2>

![Rotator Controller feature plugin GUI](../../../doc/img/GS232Controller_plugin.png)

<h3>1: Start/Stop plugin</h3>

This button starts or stops the plugin. When the plugin is stopped, azimuth and elevation commands will not be sent to the rotator.

When started, the background will be green if the rotator is pointing at target azimuth and elevation within the specified tolerance. When off target (i.e. while rotating) the background will be yellow.

<h3>2: Azimuth</h3>

Specifies the target azimuth (angle in degrees, clockwise from North) to point the antenna towards. Valid values range from 0 to 450 degrees.
The value to the right of the target azimuth, is the current azimuth read from the rotator.

<h3>3: Elevation</h3>

Specifies the target elevation (angle in degrees) to point the antenna towards. Valid values range from 0 to 180 degrees, where 0 and 180 point towards the horizon and 90 degrees to zenith.
The value to the right of the target elevation, is the current elevation read from the rotator.

<h3>4: Track</h3>

When checked, the target azimuth and elevation will be controlled by the Channel or Feature Source (5).
For example, this allows an aircraft to be tracked, by setting the Source to the ADS-B Demodulator plugin, or the Moon to be tracked by setting Source to the Star Tracker plugin.

<h3>5: Source</h3>

Specify the SDRangel Channel or Feature that will control the target azimuth and elevation values, when Track (4) is checked.

<h3>6: Target</h3>

When tracking is enabled, this field will display a name for the target being tracked, as indicated by the selected Source plugin (5).
For example, the ADS-B plugin will display the flight number of the target aircraft. The Star Tracker plugin will display Sun, Moon or Star.

<h3>7: Protocol</h3>

Selects which protocol to use. This can be GS-232, SPID (rot2prog) or rotctld.

<h3>8: Connection</h3>

Selects whether to use a serial connection or TCP.

<h3>9: Serial Port</h3>

Specifies the serial port (E.g. COM3 on Windows or /dev/ttyS0 on Linux) that will be used to send commands to the rotator.

<h3>10: Baud rate</h3>

Specifies the baud rate that will be used to send commands to the rotator. Typically this is 9600 for GS-232.

<h3>11: Host</h3>

Specifies the hostname / IP address of the computer to connect to.

<h3>12: Port</h3>

Specifies the TCP port number to connect to.

<h3>13: Azimuth Offset</h3>

The azimuth offset specifies an angle in degrees that is added to the target azimuth before sending to the controller. This allows for a misalignment of the rotator to be corrected.

<h3>14: Elevation Offset</h3>

The elevation offset specifies an angle in degrees that is added to the target elevation before sending to the controller. This allows for a misalignment of the rotator to be corrected.

<h3>15 and 16: Azimuth Min and Max</h3>

The azimuth min and max values specify the minimum and maximum azimuth values (after offset has been applied), that will be sent to the rotator.
These values can be used to prevent the rotator from rotating an antenna in to an obstacle.

<h3>17 and 18: Elevation Min and Max</h3>

The elevation min and max values specify the minimum and maximum elevation values (after offset has been applied), that will be sent to the rotator.
These values can be used to prevent the rotator from rotating an antenna in to an obstacle.
If the maximum elevation is set to 0, the controller will only use the M GS-232 command, rather than M and W.

<h3>19: Tolerance</h3>

Specifies a tolerance in degrees, below which, changes in target azimuth or elevation will not be sent to the rotator.
This can prevent some rotators that have a limited accuracy from making unbeneficial movements.

If this set to 0, every target azimuth and elevation received by the controller will be send to the rotator.
If it is set to 2, then a change in azimuth of +-1 degree from the previous azimuth, would not be sent to the rotator.

<h3>20: Precision</h3>

Specifies the number of decimal places used for coordinates and the tolerance setting.
For GS-232 this is fixed to 0. For SPID it is fixed to 1.

<h3>21: Coordinates</h3>

Specifies the coordinate system used by the GUI for entry and display of the position of the rotator. This can be:

* Az/El - For azimuth and elevation in degrees.
* X/Y 85' - For X/Y coordinates in degrees. 0,0 is zenith. X is positive Southward. Y is positive Eastward.
* X/Y 30' - For X/Y coordinates in degrees. 0,0 is zenith. X is positive Eastward. Y is positive Northward.

Equations for translating between these coordinate systems can be found [here](https://ntrs.nasa.gov/citations/19670030005).

<h3>22: Input Control</h3>

Specifies a controller/gamepad that can be used to specify target coordinates or azimuth and elevation offset.

When a gamepad with 2 sticks (4 axes) such as the XBox Wireless Controller is used, the right stick is used for controlling target coordinates,
while the left stick is for controlling azimuth and elevation offset.
If a controller only has 2 axes, target coordinates will be controlled when not tracking (6) and offset will be controlled when tracking.

Buttons on a controller are assigned to the following functions:

| Button  | XBox Controller  | Function                                 |
|---------|------------------|------------------------------------------|
| 0       | Y                | Start/stop plugin                        |
| 1       | A                | Enable/disabling tracking                |
| 3       | B                | Enable/disable target control            |
| 2       | X                | Enable/disable offset control            |
| 8       | R1               | Switch between low and high sensitivity  |

The [Qt Gamepad](https://doc.qt.io/qt-5/qtgamepad-index.html) library is used to implement gamepad support.
On Linux, using Qt Gamepad with the evdev backend, all joysticks & gamepads appear as though they have 4 axes (a limitation of Qt Gamepad).
If using a joystick which only has 2 axes, whether it corresponds to the left or right stick can be configured by pressing the 'Config...' button.
On Linux, the [xone driver](https://github.com/medusalix/xone) has support for the Xbox Wireless Controller, that isn't supported by the older xpad driver that is included with Ubuntu.

<h3>23: High or Low Sensitivity</h3>

Specifies High (H) or Low (L) Sensitivity mode. This is how fast coordinates will change for a given control stick movement.
High Sensitivity is used for coarse target/offset adjustment, whereas Low sensitivity is used for fine target/offset adjustment.
The sensitivity in each mode can be set in the Input Configuration Dialog (22).

<h3>24: (T) Enable Target Control</h3>

When checked, the target coordinates can be set with the input controller. When unchecked, the axes controlling the target will be ignored.

<h3>25: (O) Enable Offset Control</h3>

When checked, the offset coordinates can be set with the input controller. When unchecked, the axes controlling the offset will be ignored.

<h3>26: Input Control Configuration</h3>

Pressing the Config... button will display the Input Configuration Dialog:

<h4>1: Configuration</h4>

The Configure buttons allow setting which axis is assigned to target or offset control. To assign an axis, press the corresponding Configure button, then move the controller axis.

<h4>2: Deadzones</h3>

Deadzones can be set individually for each for the 4 axes. A deadzone specifies the amount a controller axis can be moved without a response.
This can be used to prevent very small movements around the center position on a stick from adjusting the target, or eliminate target adjustments
when a stick is centered, but is reporting a non-zero position on the axis.

<h4>3: Sensitivity</h3>

Specifies the sensitivity of the input in Low and High Sensitivity mode (23). The higher the value, the faster coordinates will change for a given control stick movement.

<h2>Protocol Implementations</h2>

<h3>GS-232 Protocol Implementation Notes</h3>

The controller uses the Waaa eee command when elevation needs to be set.
When only azimuth needs to be set, the Maaa command is used.
The C2 command is used to read current azimuth and elevation. A response of AZ=aaaEL=eee is expected.

<h3>SPID rot2prog Protocol Implementation</h3>

The controller uses the 0x2f set command with PH/PV=2 to set azimuth and elevation.
The 0x1f status command is used to read current azimuth and elevation.
A 12 byte response is expected for set and status commands.
All frames start with 0x57 and end with 0x20.

<h3>rotctld Protocol Implementation</h3>

The controller uses the 'P' and 'p' commands to set and get azimuth and elevation.

<h2>API</h2>

Full details of the API can be found in the Swagger documentation. Here is a quick example of how to set the azimuth and elevation from the command line:

    curl -X PATCH "http://127.0.0.1:8091/sdrangel/featureset/0/feature/0/settings" -d '{"featureType": "GS232Controller",  "GS232ControllerSettings": { "azimuth": 180, "elevation": 45 }}'

To start sending commands to the rotator:

    curl -X POST "http://127.0.0.1:8091/sdrangel/featureset/0/feature/0/run"
