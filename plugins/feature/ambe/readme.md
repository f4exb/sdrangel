<h1>AMBE controller</h1>

<h2>Introduction</h2>

Control AMBE3000 serial devices or AMBE server addresses to use for AMBE digital voice processing.

<h2>Interface</h2>

![AMBE controller GUI](../../../doc/img/AMBE_plugin.png)


<h3>1: AMBE server address and port or direct input</h3>

Use this freeflow text input box to specify either the address and port of an AMBE server in the form: &lt;IPv4 address>:&lt;port> or any directly attached physical device address like a COM port on Windows.

<h3>2: Import above address or device</h3>

Import the address or device specified in (1) into the list of used devices. The system will try to open the device or contact the server and will add it to the list only if successful.

<h3>3: Remove in use device or address</h3>

When a device or address is selected in the in use list (6) push this button to remove it from the list. The corresponding resources will be released.

<h3>4: Refresh in use list</h3>

Checks the list of devices or addresses currently in use and update the in use list (6).

<h3>5: Empty in use list</h3>

Removes all devices or addresses in use. The in use list (6) is cleared consequently. This removes all AMBE devices related resources attached to the current instance of the SDRangel program. Therefore consecutive AMBE frames decoding will be handled by the mbelib library if available or no audio will be output.

<h3>6: In use list</h3>

List of devices or addresses currently in use for AMBE frames decoding by this feature.

Format is the device path or URL followed by a dash then the number of frames successfully decoded then a vertical bar then the number of frames that failed decoding. i.e:

`<Device path> - <successful count>|<failed count>`

<h3>7: Import serial device</h3>

Imports a serial device scanned in the list of available AMBE 3000 serial devices (10) in the in use list. If this device is already in the in use list then nothing happens and this is reported in the status text (11)

<h3>8: Import all serial devices</h3>

Imports all serial devices scanned in the list of available AMBE 3000 serial devices (9) in the in use list. If any device is already in the in use list then it is not added twice.

<h3>9: Refresh list of AMBE serial devices</h3>

Scans available AMBE 3000 serial devices and updates the list.

<h3>10: List of AMBE 3000 serial devices</h3>

List of AMBE 3000 connected to the system. Use button (9) to update the list.

<h3>11: Status text</h3>

A brief text reports the result of the current action.
