## Examples of client scripts ##

These are all Python scripts using python-requests so you have to install this package as a prerequiste either with your package manager or pip. They are designed for Python 2.7 but may work with Python 3 possibly with minimal changes.

<h2>add_channel.py</h2>

Adds a channel to a device set specifying device set index and channel type. 

  - Operation ID: `devicesetChannelPost`
  - URI: `/sdrangel/deviceset/{deviceSetIndex}/channel`
  - HTTP method: `POST`

<h2>devicesets_config.py</h2>

Example of building an entire configuration with 3 device sets using presets to configure each one of the device sets then start streaming on all of them.

It uses the following APIs:

  - To select a device in a device set:
    - Operation ID: `devicesetDevicePut`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device`
    - HTTP method: `PUT`
  - To load a preset in a device set:
    - Operation ID: `instancePresetPatch`
    - URI: `/sdrangel/preset`
    - HTTP method: `PATCH`
  - To create a new device set:
    - OperationID: `instanceDeviceSetPost`
    - URI: `/sdrangel/deviceset`
    - HTTP method: `POST`
  - Activate the DV serial dongle support for digital voice modes
    - OperationID: `instanceDVSerialPatch`
    - URI: `/sdrangel/dvserial`
    - HTTP method: `PATCH`
  - Start a device streaming
    - OperationID: `devicesetDeviceRunPost`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device/run`
    - HTTP method: `POST`
  
<h2>limesdr_tx.py</h2>

Create a Tx device set with a LimeSDR Tx device and a NFM modulator channel configured to send some beacon Morse code. Then starts the Tx.
 
It uses the following APIs:

  - To create a new device set:
    - OperationID: `instanceDeviceSetPost`
    - URI: `/sdrangel/deviceset`
    - HTTP method: `POST`
  - To select a device in a device set:
    - Operation ID: `devicesetDevicePut`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device`
    - HTTP method: `PUT`
  - To create a new channel:
    - Operation ID: `devicesetChannelPost`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/channel`
    - HTTP method: `POST`
  - To change the settings of a channel:
    - OperationID: `devicesetChannelSettingsPatch`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/settings`
    - HTTP method: `PATCH`
  - Start a device streaming
    - OperationID: `devicesetDeviceRunPost`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device/run`
    - HTTP method: `POST`
  
<h2>nfm_test.py</h2>

Example of creating NFM channels (demodulator and modulator) and changing the settings

It uses the following APIs:
  
  - To create a new channel:
    - Operation ID: `devicesetChannelPost`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/channel`
    - HTTP method: `POST`
  - To create a new device set:
    - OperationID: `instanceDeviceSetsPost`
    - URI: `/sdrangel/devicesets`
    - HTTP method: `POST`
  - To change the settings of a channel:
    - OperationID: `devicesetChannelSettingsPatch`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/channel/{channelIndex}/settings`
    - HTTP method: `PATCH`
    
<h2>ptt.py</h2>

Implements a basic push to talk (PTT) feature. Verifies that devise set #0 is a Rx and that #1 is a Tx. Stops streaming on one device and start streaming on the other depending on the PTT move (Rx to Tx or Tx to Rx).

It uses the following APIs:
  
  - Get information on device sets:
    - Operation ID: `instanceDeviceSetsGet`
    - URI: `/sdrangel/devicesets`
    - HTTP method: `GET`
  - Start a device streaming
    - OperationID: `devicesetDeviceRunPost`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device/run`
    - HTTP method: `POST`
  - Stop a device streaming
    - OperationID: `devicesetDeviceRunDelete`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device/run`
    - HTTP method: `DELETE`

<h2>rtlsdr_settings.py</h2>

Make sure a RTLSDR device is selected on device set #0. Get and change the settings of this RTLSDR device.

It uses the following APIs:

  - Get information on a device set:
    - Operation ID: `devicesetGet`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}`
    - HTTP method: `GET`
  - To select a device in a device set:
    - Operation ID: `devicesetDevicePut`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device`
    - HTTP method: `PUT`
  - To get the settings of a device:
    - OperationID: `devicesetDeviceSettingsGet`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device/settings`
    - HTTP method: `GET`
  - To change the settings of a device:
    - OperationID: `devicesetDeviceSettingsPatch`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device/settings`
    - HTTP method: `PATCH`

<h2>start_stop.py</h2>

Starts or stops a device in the specified device set

It uses the following APIs:
  
  - Get information on device sets:
    - Operation ID: `instanceDeviceSetsGet`
    - URI: `/sdrangel/devicesets`
    - HTTP method: `GET`
  - Start a device streaming
    - OperationID: `devicesetDeviceRunPost`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device/run`
    - HTTP method: `POST`
  - Stop a device streaming
    - OperationID: `devicesetDeviceRunDelete`
    - URI: `/sdrangel/deviceset/{deviceSetIndex}/device/run`
    - HTTP method: `DELETE`
    
<h2>stop_server.py</h2>

This works with a server instance only i.e. `sdrangelsrv`. It will shutdown the instance nicely as you would do with the exit menu or Ctl-Q in the GUI application.

It uses this API:

  - Stop a server instance
    - OperationID: `instanceDelete`
    - URI: `/sdrangel`
    - HTTP method: `DELETE`

  