<h1>Main Window interface</h1>

<h2>Multi device support</h2>

Starting with version 2 SDRangel supports running several sampling devices simultaneously. Each concurrent device is associated to a slot with a set of tabbed windows in the UI. These tabs are marked R0, R1, R2...

The slots are arranged in a stacked fashion so that when a new device is added with the Acquisition -> Add device set menu a new slot is allocated in the last position and when a device is removed with the Acquisition -> Remove last device set menu the slot in the last position is deleted. Slot 0 (R0) receiver slot is created at initialization and cannot be deleted with the menu. The letter "R" in the tab names indicates that the slot is for a receiver (source) device while "T" designates a transmitter (sink) device.

The tabbed windows are:

  - Sampling devices (1)
  - Sampling devices control (2)
  - Spectrum display control (3)
  - Channels (4)
  - Spectrum from device (5)

The combination of a sampling device and its associated channels is called a "device set".

![Main Window multi device support](../doc/img/MainWindow_tabs.png)

The sampling devices tab (1) acts as a master and when one of its tabs is selected all other tabs are selected accordingly i.e. all R0s, all R1s, etc... in tabs (2), (3), (4) and (5)

In each slave tab group (2), (3), (4) and (5) an individual tab corresponding to one device can be selected without affecting the selection of the other tabs. This way you can sneak peek into another spectrum or channel group without affecting the display of other tabbed windows.

<h2>Interface details</h2>

![Main Window interface](../doc/img/MainWindow_general.png)

<h3>1. Main menu</h3>

The following items are presented hierarchically from left to right:

  - File:
    - _Exit_ (shortcut Ctl-Q): Exit the program
  - View:
    - _Fullscreen_ (Shortcut F11): Toggle full screen mode
  - Device sets:
    - _Add source device set_: adds a new source (receiver) type device set to the device set stack (last position)
    - _Add sink device set_: adds a new sink (transmitter) type device set to the device set stack (last position)
    - _Remove last device set_: removes the last device set from the device set stack
  - Window: presents the list of dockable windows. Check to make it visible. Uncheck to hide. These windows are:
    - _Sampling devices control_: control of which sampling devices is used and add channels
    - _Sampling devices_: the sampling devices UIs
    - _Spectrum display_: the main spectrum displays (output from the sampling devices)
    - _Presets_: the saved presets
    - _Commands_: the defined commands
    - _Channels_: the channels active for each device
  - Preferences:
    - _Audio_: opens a dialog to choose the audio output device (see 1.1 below for details)
    - _Logging_: opens a dialog to choose logging options (see 1.2 below for details)
    - _AMBE_: Opens a dialog to select AMBE3000 serial devices or AMBE server addresses to use for AMBE digital voice processing. If none is selected AMBE frames decoding will be done with mbelib if available else no audio will be produced for AMBE digital voice (see 1.3 below for details)
    - _My Position_: opens a dialog to enter your station ("My Position") coordinates in decimal degrees with north latitudes positive and east longitudes positive. This is used whenever positional data is to be displayed (APRS, DPRS, ...). For it now only works with D-Star $$CRC frames. See [DSD demod plugin](../plugins/channelrx/demoddsd/readme.md) for details on how to decode Digital Voice modes.
    - _Devices_: section to deal with devices settings
      - _User arguments_: opens a dialog to let the user give arguments specific to a device and its instance (sequence) in the system
  - Help:
    - _Loaded Plugins_: shows details about the loaded plugins (see 1.3 below for details)
    - _About_: current version and blah blah.

<h4>1.1. Preferences - Audio</h4>

See the audio management documentation [here](audio.md).

<h4>1.2. Preferences - Logging</h4>

![Main Window logging preferences](../doc/img/MainWindow_logging.png)

Log message will appear as follows:

```
12.11.2017 15:03:37.864 (D) DownChannelizer::applyConfiguration in= 300000 , req= 375000 , out= 300000 , fc= 0
----------------------- --- ----------------------------------------------------------------------------------
(1)                     (2) (3)
```

  - 1: Timestamp in `yyyy-MM-dd HH:mm:ss.zzz` format
  - 2: Message level: `(D)`: debug, `(I)`: info, `(W)`: warning, `(C)`: critical, `(F)`: fatal
  - 3: Message text

<h5>1.2.1. Console log minimum message level</h5>

This sets the minimum message level for a message do be displayed on the console:

  - Debug: all messages with QtDebug level and above
  - Info: all messages with QtInfo level and above
  - Warning: all messages with QtWarning level and above
  - Error: all messages with QtCritical level and above. Includes QtFatal.

<h5>1.2.2. File log minimum message level</h5>

This sets the minimum message level for a message do be logged to file:

  - Debug: all messages with QtDebug level and above
  - Info: all messages with QtInfo level and above
  - Warning: all messages with QtWarning level and above
  - Error: all messages with QtCritical level and above. Includes QtFatal.

<h5>1.2.3. File log enable</h5>

Use the checkbox to enable (check) or disable (uncheck) the dual logging to file

<h5>1.2.4. Log file selection</h5>

Use this button to open a file dialog to choose or create a new log file. There is a 2s delay before a file change is effective.

<h5>1.2.5. Log file name</h5>

The full path of the log file appears here

<h5>1.2.6. Confirm changes</h5>

Use the "OK" button to validate all changes

<h5>1.2.7. Dismiss changes</h5>

Use the "Cancel" button to dismiss all changes

<h4>1.3 Preferences - AMBE</h4>

When clicking on the AMBE submenu a dialog opens to let you specify physical AMBE devices to decode AMBE frames produced by digital voice signals (using DSD decoder plugin).

![Main Window AMBE](../doc/img/MainWindow_ambe.png)

<h5>1.3.1 AMBE server address and port or direct input</h5>

Use this freeflow text input box to specify either the address and port of an AMBE server in the form: &lt;IPv4 address&gt;:&lt;port&gt; or any directly attached physical device address like a COM port on Windows.

<h5>1.3.2 Import above address or device</h5>

Import the address or device specified in (1) into the list of used devices. The system will try to open the device or contact the server and will add it to the list only if successful.

<h5>1.3.3 Remove in use device or address</h5>

When a device or address is selected in the in use list (6) push this button to remove it from the list. The corresponding resources will be released.

<h5>1.3.4 Refresh in use list</h5>

Checks the list of devices or addresses currently in use and update the in use list (6).

<h5>1.3.5 Empty in use list</h5>

Removes all devices or addresses in use. The in use list (6) is cleared consequently. This removes all AMBE devices related resources attached to the current instance of the SDRangel program. Therefore consecutive AMBE frames decoding will be handled by the mbelib library if available or no audio will be output.

<h5>1.3.6 In use list</h5>

List of devices or addresses currently in use for AMBE frames decoding by this instance of the SDRangel program.

<h5>1.3.7 Import serial device</h5>

Imports a serial device scanned in the list of available AMBE 3000 serial devices (9) in the in use list. If this device is already in the in use list then nothing happens and this is reported in the status text (10)

<h5>1.3.8 Import all serial devices</h5>

Imports all serial devices scanned in the list of available AMBE 3000 serial devices (9) in the in use list. If any device is already in the in use list then it is not added twice.

<h5>1.3.9 List of available AMBE 3000 serial devices</h5>

This is the list of AMBE 3000 currently attached to the system directly. This list gets updated at every opening of the dialog.

<h5>1.3.10 Status text</h5>

A brief text reports the result of the current action

<h5>1.3.11 Close button</h5>

Use this button to dismiss the dialog

<h4>1.4 Prefernces - LimeRFE</h4>

Only in Linux and if LimeSuite library is available this opens a dialog to control a LimeRFE device via USB. The details are provided [here](limerfeusbgui.md).

<h4>1.5 Preferences - Devices - User arguments</h4>

See the devuces user arguments management documentation [here](deviceuserargs.md).

<h4>1.6. Help - Loaded plugins display</h4>

When clicking on Help -> Loaded Plugins from the main menu bar a dialog box appears that shows information about the plugins loaded in SDRangel:

![Main Window loaded plugins](../doc/img/MainWindow_loadedPlugins.png)

<h5>Name</h5>

Plugin display name. Tells briefly what this plugin is about.

<h5>Version</h5>

Starting with SDRangel version 2.0.0 this is the SDRangel version when the plugin was last updated.

<h5>GPL</h5>

Tells if the plugin is under GPL license.

<h5>Expansion</h5>

The plugin entry can be expanded or collapsed using the caret on the left. When expanded it shows more information about the copyright of the author and locations on the web where the plugin can be found. In all cases this is just here.

<h5>OK button</h5>

Click here when done to dismiss the dialog.

<h3>2. Sampling devices</h3>

This is where the plugin GUI specific to the device is displayed. Control of one device is done from here. The common controls are:

![Sampling Device common](../doc/img/SampleDevice_common.png)

<h4>2.1. Start or stop acquisition</h4>

<h5>2.1.1 left click</h5>

Left click to start or stop streaming

  - When a play icon (&#9654;) is displayed with a grey background the device is not operational
  - When a play icon (&#9654;) is displayed with a blue background the device is ready to start
  - When a stop icon (&#9632;) is displayed with a green background the device is currently running
  - When a play icon (&#9654;) is displayed with a red background there is an error and a popup displays the error message. An Error typically occurs when you try to start the same device in more than one tab.

<h5>2.1.2 right click</h5>

Right click to control device reverse API. This dialog opens:

![Basic device settings](../doc/img/BasicDeviceSettings.png)

<h6>2.1.2.1: Toggle reverse API feature</h6>

Use this checkbox to toggle on/off the reverse API feature. With reverse API engaged the changes in the device settings are forwarded to an API endpoint given by address (2.1.2.2), port (2.1.2.3) and device index (2.1.2.4) in the same format as the SDRangel REST API device settings endpoint. With the values of the screenshot the API URL is: `http://127.0.0.1:8888/sdrangel/deviceset/0/device/settings` The JSON payload follows the same format as the SDRangel REST API device settings. For example with HachRF Rx this would be something like:

```
{
  "deviceHwType": "HackRF",
  "hackRFInputSettings": {
    "LOppmTenths": 0,
    "bandwidth": 1750000,
    "biasT": 0,
    "centerFrequency": 435000000,
    "dcBlock": 0,
    "devSampleRate": 2400000,
    "fcPos": 2,
    "iqCorrection": 0,
    "linkTxFrequency": 0,
    "lnaExt": 0,
    "lnaGain": 16,
    "log2Decim": 0,
    "vgaGain": 16
  },
  "tx": 0
}
```
Note that the PATCH method is used. The full set of parameters is sent only when the reverse API is toggled on or a full settings update is done.

The start and stop actions are also forwarded with the `/sdrangel/deviceset/{deviceSetIndex}/device/run` API endpoint using POST (start) or DELETE (stop) methods.

More details on this feature can be found on the corresponding Wiki page.

<h6>2.1.2.2: API address</h6>

This is the IP address of the API endpoint

<h6>2.1.2.3: API port</h6>

This is the IP port of the API endpoint

<h6>2.1.2.4: Device index</h6>

This is the targeted device index

<h6>2.1.2.5: Cancel changes and exit dialog</h6>

Do not make any changes and exit dialog

<h6>2.1.2.6: Validate and exit dialog</h6>

Validates the data (saves it in the channel marker object) and exits the dialog

<h4>2.2. Record I/Q</h4>

This is the I/Q from device record toggle. When a red background is displayed the recording is currently active. The name of the file created is `test_n.sdriq` where `n` is the slot number.

The format is S16LE I/Q samples. Thus there are 4 bytes per sample. I and Q values are 16 bit signed integers. The file starts with a context header containing information about center frequency, sample rate and timestamp of the start of the recording. This header has a length which is a multiple of a sample size (normally 24 bytes thus 6 samples). Thus this file can be used as a raw I/Q file with S16LE samples tolerating a glitch at the start corresponding to the 6 "random" samples.

You can also zap the 24 bytes header with this Linux command: `tail -c +25 myfile.sdriq > myfile.raw`

To convert in another format you may use the sox utility. For example to convert to 32 bit (float) complex samples do:
`sox -r 48k −b 16 −e signed-integer -c 2 myfile.raw -e float -c 2 myfilec.raw`

Note that you have to specify the sampling rate and use `.raw` for the file extensions.

<h4>2.3. Device sampling rate</h4>

This is the sampling rate in kS/s of the I/Q stream extracted from the device after possible decimation. The main spectrum display corresponds to this sampling rate.

<h4>2.4. Center frequency</h4>

This is the current center frequency in kHz with dot separated thousands (MHz, GHz). On devices for which frequency can be directly controlled (i.e. all except File Source and Remote Input) you can use the thumbwheels to set the frequency. Thumbwheels move with the mouse wheel when hovering over a digit.

When left clicking on a digit a cursor is set on it and you can also use the arrows to move the corresponding thumbwheel.

When right clicking on a digit the digits on the right are all set to zero. This effectively does a ceil rounding at the current position.

<h4>Additional inputs</h4>

Most devices will also present an interface to control automatic DC removal and I/Q imbalance and also a control of the LO correction in ppm.

  - Example1: ![Sampling Device corr 1](../doc/img/SampleDevice_corr01.png)
  - Example2: ![Sampling Device corr 2](../doc/img/SampleDevice_corr02.png)

<h3>3. Sampling devices control</h3>

This is where the sampling device for one device set is selected and the channel plugins are instantiated.

![Sampling Devices control](../doc/img/MainWindow_SDControl.png)

<h4>3.1. Currently sampling device name</h4>

This label shows the human readable sampling device name

<h4>3.2. Open sampling device change dialog</h4>

Use this push button to open the device selection dialog to change the sampling device. This dialog will open:

![Main Window sampling devices dialog](../doc/img/MainWindow_SDDialog.png)

<h5>3.2.1. Device selection combo</h5>

Use this combo box to select the device. Only available devices will appear in the list. For devices having more than one channel (ex: LimeSDR) the channel number will appear next to the device sequence number inside the brackets. Ex: `LimeSDR[0:1] 0009060B00473419` designates the second Rx (Rx #1) of the first encountered LimeSDR which serial number is 0009060B00473419.

<h5>3.2.2. Device selection confirmation</h5>

Use the `OK` button to confirm your choice and exit the dialog

<h5>3.2.3. Device selection cancellation</h5>

Use the `Cancel` button to exit the dialog without any change

<h4>3.3. Reload currently selected device</h4>

This button activates a close/open sequence to recycle the device. It may be useful when the device is not streaming anymore or in an attempt to clear possible errors. Make sure the streaming is stopped first.

<h4>3.4. Channel selector</h4>

Use this combo box to select a channel plugin to create a new channel

<h4>3.5. Add a new channel</h4>

Use this push button to add a new channel with the selected plugin

<h3>4. Spectrum display control</h3>

![Spectrum GUI](../doc/img/MainWindow_spectrum_gui.png)

These are the controls of the main spectrum display in (7). The same controls are found in the plugins that feature a spectrum display:
  - Channel Analyzer
  - Broadcast FM demodulator
  - SSB demodulator
  - UDP source
  - UDP sink

<h4>4.1. FFT window selector</h4>

Use this combo box to select which window is applied to the FFT:
  - **Bart**: Bartlett
  - **B-H**: Blackmann-Harris
  - **FT**: Flat top
  - **Ham**: Hamming
  - **Han**: Hanning (default)
  - **Rec**: Rectangular (no window)
  - **Kai**: Kaiser with alpha = 2.15 (beta = 6.76) gives sidelobes &lt; -70dB

<h4>4.2. FFT size</h4>

Select the size of the FFT window among these values:
  - 128
  - 256
  - 512
  - 1k = 1024 (default)
  - 2k = 2048
  - 4k = 4096

<h4>4.3. Reference level</h4>

This is the level in dB at the top of the display range. You can select values between 0 and -110 in 5 dB steps

<h4>4.4. Range</h4>

This is the range of display in dB. You can select values between 5 and 100 in 5 dB steps

<h4>4.5. Averaging mode</h4>

Use this combo to select which averaging mode is applied:
  - **No**: no averaging. Disables averaging regardless of the number of averaged samples (4.6). This is the default option
  - **Mov**: moving average. This is a sliding average over the amount of samples specified next (4.6). There is one complete FFT line produced at every FFT sampling period
  - **Fix**: fixed average. Average is done over the amount of samples specified next (4.6) and a result is produced at the end of the corresponding period then the next block of averaged samples is processed. There is one complete FFT line produced every FFT sampling period multiplied by the number of averaged samples (4.6). The time scale on the waterfall display is updated accordingly.
  - **Max**: this is not an averaging but a max hold. It will retain the maximum value over the amount of samples specified next (4.6). Similarly to the fixed average a result is produced at the end of the corresponding period which results in slowing down the waterfall display. The point of this mode is to make outlying short bursts within the "averaging" period stand out. With averaging they would only cause a modest increase and could be missed out.

<h4>4.6. Number of averaged samples</h4>

Each FFT bin (squared magnitude) is averaged or max'ed over a number of samples. This combo allows selecting the number of samples between these values: 1 (no averaging), 2, 5, 10, 20, 50, 100, 200, 500, 1k (1000) for all modes and in addition 2k, 5k, 10k, 20k, 50k, 1e5 (100000), 2e5, 5e5, 1M (1000000) for "fixed" and "max" modes. The tooltip mentions the resulting averaging period considering the baseband sample rate and FFT size.
Averaging reduces the noise variance and can be used to better detect weak continuous signals. The fixed averaging mode allows long time monitoring on the waterfall. The max mode helps showing short bursts that may appear during the "averaging" period.

&#9758; Note: The spectrum display is refreshed every 50ms (20 FPS). Setting an averaging time above this value will make sure that a short burst is not missed particularly when using the max mode.

<h4>4.7. Phosphor display stroke decay</h4>

This controls the decay rate of the stroke when phosphor display is engaged (4.C). The histogram pixel value is diminished by this value each time a new FFT is produced. A value of zero means no decay and thus phosphor history and max hold (red line) will be kept until the clear button (4.B) is pressed.

<h4>4.8. Phosphor display stroke decay divisor</h4>

When phosphor display is engaged (4.C) and stroke decay is 1 (4.7) this divides the unit decay by this value by diminishing histogram pixel value by one each time a number of FFTs equal to this number have been produced. Thus the actual decay rate is 1 over this value. This allow setting a slower decay rate than one unit for each new FFT.

<h4>4.9. Phosphor display stroke strength</h4>

This controls the stroke strength when phosphor display is engaged (4.C). The histogram value is incremented by this value at each new FFT until the maximum (red) is reached.

<h4>4.A. Trace intensity</h4>

This controls the intensity of the maximum (4.D) and current (4.E) spectrum trace

<h4>4.B. Clear spectrum</h4>

This resets the maximum spectrum trace and phosphor remanence

<h4>4.C. Phosphor display</h4>

Toggles the phosphor display on the spectrum

<h4>4.D. Maximum trace</h4>

Toggles the maximum trace display (red trace) on the spectrum

<h4>4.E. Current trace</h4>

Toggles the current trace display (yellow trace) on the spectrum

<h4>4.F. Waterfall/spectrum placement</h4>

Toggles the spectrum on top or on bottom versus waterfall

<h4>4.G. Waterfall</h4>

Toggles the waterfall display

<h4>4.H.Grid</h4>

Toggles the grid display

<h4>4.I.Grid intensity</h4>

Controls the intensity of the grid display

<h4>4.J. Logarithmic/linear scale</h4>

Use this toggle button to switch between spectrum logarithmic and linear scale display. The face of the button will change to represent either a logaritmic or linear curve.

When in linear mode the range control (4.4) has no effect because the actual range is between 0 and the reference level. The reference level in dB (4.3) still applies but is translated to a linear value e.g -40 dB is 1e-4. In linear mode the scale numbers are formatted using scientific notation so that they always occupy the same space.

<h3>5. Presets and commands</h3>

The presets and commands tree view are by default stacked in tabs. The following sections describe the presets section 5A) and commands (section 5B) views successively

<h3>5A. Presets</h3>

This is a tree view of the saved presets. Presets record the channels setup and a copy of the settings of each sample source that has been used when saving this preset. Thus you can use the same channel arrangement with various devices having their particular setup.

![Main Window presets view](../doc/img/MainWindow_presets_view.png)

<h4>5A.1. Preset selection</h4>

You select a preset or a preset group by clicking on its line in the tree view. All actions (6) will be done relative to this preset or preset group.

<h4>5A.2. Group</h4>

You can organize your presets into groups. Groups can be collapsed or expanded by using the caret icon on the left.

<h4>5A.3. Center frequency</h4>

The center frequency used in this preset is displayed here.

<h4>5A.4. Rx/Tx indicator</h4>

"R" is displayed for a Rx device set and "T" for a Tx device set

<h4>5A.5. Preset name</h4>

You can give a name to your preset. Names need not to be unique.

<h4>5A.6. Preset control or actions</h4>

The controls are located as icons at the bottom of the window:

![Main Window presets](../doc/img/MainWindow_presets.png)

<h5>5A.6.1. New preset</h5>

Click on this icon to create a new preset with the current values in the selected sample device tab (Main window: 2).

<h5>5A.6.2. Update preset</h5>

Click on this icon to create a update the selected preset with the current values in the selected sample device tab (Main window: 2). Please note that this does not save the preset immediately on disk to save presets immediately you need to use the save button (4).

<h5>5A.6.3. Edit preset</h5>

Opens a new window where you can change the group name and description.

  - for group items you can rename the group or merge all group presets into an existing group by selecting this existing group
  - for preset items you can:
    - move the preset to another existing group by selecting this existing group
    - assign this preset to a new group by typing in this new group
    - change the description

<h5>5A.6.4. Save presets</h5>

Presets are saved to disk automatically at exit time you can however request to save them immediately using this icon.

<h5>5A.6.5. Export preset</h5>

Using the previous icon presets are saved globally in a system dependent place. Using this icon you can export a specific preset in a single file that can be imported on another machine possibly with a different O/S. The preset binary data (BLOB) is saved in Base-64 format.

<h5>5A.6.6. Import preset</h5>

This is the opposite of the previous operation. This will create a new preset in the selected group or the same group as the preset being selected.

<h5>5A.6.7. Delete preset</h5>

  - on a preset item: deletes the selected preset.
  - on a preset group: deletes the group and all its presets.

<h5>5A.6.8. Load preset</h5>

Applies the selected preset to the current device set (source and channel plugins).

<h3>5B. Commands</h3>

This is a tree view of the saved commands. Commands describe the path to an executable file, its arguments a possible link to a keystroke event that triggers the execution. Similarly to presets commands can be arranged into groups and have a description short text.

Typically an "executable file" is a script (Python, shell, whatever...) or can be a compiled program (c, c++, java, whatever...) that interacts with SDRangel using its web REST API. When called from within SDRangel they can act as "macros" allowing to perform actions automatically.

Of course any binary that resides in your system can be used that way like `/bin/ls` or `/bin/date` although these two are of anecdotal interest...

![Main Window presets view](../doc/img/MainWindow_commands_view.png)

<h4>5B.1. Command selection</h4>

You select a command or a command group by clicking on its line in the tree view. All actions (6) will be done relative to this command or command group.

<h4>5B.2. Group</h4>

You can organize your commands into groups. Groups can be collapsed or expanded by using the caret icon on the left.

<h4>5B.3. Description</h4>

Short description of a command.

<h4>5B.4. Key binding indicator</h4>

  - `-`: no key binding
  - `P`: key press binding
  - `R`: key release binding

<h4>5B.5. Key binding sequence</h4>

This is a descriptive text of the key sequence that is used for the key binding.

<h4>5B.6. Command control or actions</h4>

The controls are located as icons at the bottom of the window:

![Main Window commands](../doc/img/MainWindow_commands.png)

<h5>5B.6.1. Create new command</h5>

Click on this icon to create a new command. This opens an edit dialog see the edit section (5B.6.3) for the details of the edit dialog.

<h5>5B.6.2. Duplicate command</h5>

Click on this icon to duplicate the currently selected command (inactive on groups). Later you can edit the details of the copy with the edit dialog (see 5B.6.3 next)

<h5>5B.6.3. Edit command or command group</h5>

<b>Command groups</b>

With this dialog you can rename a group using the text box or if you select an existing group with the combo this will merge the contents of the group with the existing group

![Main Window command group edit](../doc/img/MainWindow_command_edit_group.png)

<b>Commands</b>

You can edit the details of the command with this dialog.

![Main Window command group edit](../doc/img/MainWindow_command_edit.png)

<h6>5B.6.3.1. Edit group </h6>

You can select an existing group with the combo or create a new one for this command using the text edit box

<h6>5B.6.3.2. Edit description </h6>

You can edit the description using this text box. The description will appear in the tree view.

<h6>5B.6.3.3. Executable file selection </h6>

Clicking on this button will open a file dialog to select the executable file that will be run with this command. The file selection dialog has predefined file pattern selections:

  - `*` for All files
  - `*.py` for Python files
  - `*.sh` or `*.bat` for shell or batch files
  - `*.bin` or `*.exe` for binary files

<h6>5B.6.3.4. Executable file path </h6>

This is the full path of the selected executable file.

<h6>5B.6.3.5. Command line arguments</h6>

Use the text box to edit the arguments given to the executable file as in `program arguments`.

You can use special codes to insert information specific to the application context:

  - `%1`: the address of the web REST API
  - `%2`: the port of the web REST API
  - `%3`: the currently selected device set index

<h6>5B.6.3.6. Key binding</h6>

Use this checkbox to enable or disable the command execution binding to a key or combination of keys press or release event

<h6>5B.6.3.7. Key binding capture</h6>

Use this button to capture the key or key combination that will be used for the key binding. After pushing this button just type in the key or key combination.

<h6>5B.6.3.8. Key binding display</h6>

This shows the key or combination of keys used for the key binding.

<h6>5B.6.3.9. Release key binding</h6>

Use this checkbox to bind the key or combination of keys to the key release event. If unchecked the binding will be associated to the key press event.

<h6>5B.6.3.10. Confirm changes</h6>

Use the "OK" button to confirm the changes.

<h6>5B.6.3.11. Cancel changes</h6>

Use the "Cancel" button to cancel the changes.

<h5>5B.6.4. Run command or groups of commands</h5>

This will run the currently selected command. If the selection is a group it will run all commands of the group starting them in the displayed order. Please note that commands are run in independent processes and therefore all launched commands in the group will run concurrently.

<h5>5B.6.5. View last command run details</h5>

This dialog will show the results of the last run including the output (merged stdout and stderr).

![Main Window command output](../doc/img/MainWindow_command_output.png)

<h6>5B.6.5.1. Process status</h6>

When the process is not running the stop icon (&#9632;) is displayed. The background color indicate different states:

  - no color (same as background): the process has never run during this session
  - red: the process ended with error
  - green: the process ended successfully. This does not mean that there was no programmatic error.

When the process is running the play icon (&#9654;) is displayed with an orange background.

<h6>5B.6.5.2. Refresh data</h6>

Pushing this button will update the data displayed with the latest status. Please note that the log is displayed only when the process is terminated.

<h6>5B.6.5.3. Start time</h6>

This is the timestamp of process start. It is filled with dots `...` if the process has never started during this session.

<h6>5B.6.5.4. End time</h6>

This is the timestamp of process end. It is filled with dots `...` if the process has never terminated during this session.

<h6>5B.6.5.3. PID</h6>

This is the process PID. It is 0 if the process has never run during this session.

<h6>5B.6.5.6. Process kill</h6>

Use this button to kill (send SIGKILL) the running process. It has no effect if the process is not running.

<h6>5B.6.5.7. Command line</h6>

This shows the actual command line that was used to start the process

<h6>5B.6.5.8. Error status</h6>

This is the translation of `QProcess::ProcessError`. Possible values are:

  - `...`: the process has never run during this session
  - `Failed to start`: the process could not start. For example the executable file has no execution rights actually
  - `Crashed`: the process ended with crash. This is the status when you killed the process
  - `Timed out`: the last waitFor...() function timed out.
  - `Write error`: an error occurred when attempting to write to the process. For example, the process may not be running, or it may have closed its input channel.
  - `Read error`: an error occurred when attempting to read from the process. For example, the process may not be running.
  - `Unknown error`: an unknown error occurred.

<h6>5B.6.5.9. Exit code</h6>

This is the program exit code. When the process crashes this is the signal by which the process end was caused. For example if you kill the process with button (6) it sends the process a SIGKILL (code 9) and therefore the value is 9.

<h6>5B.6.5.10. Exit status</h6>

There are only two possibilities: either the program exits normally but possibly with a non zero exit code or it ends with a crash.

<h6>5B.6.5.11. Process log</h6>

This is the log of the process (merged stdout and stderr). Please note that it is updated only on program termination.

<h6>5B.6.5.12. Exit</h6>

By pushing the "Close" button the process output window is closed.

<h5>5B.6.6. Save commands</h5>

This will save the commands immediately. The commands will be automatically saved when the application exits normally.

<h5>5B.6.7. Delete commands or group of commands</h5>

This will delete the currently selected command or if selection is a group this will delete all commands in the group.

<h5>5B.6.8. Activate keyboard bindings</h5>

Use this button to activate the keyboard bindings. Note that you need to have this button selected (its background should be lit in beige/orange) for the key bindings to be effective.

<h3>6. Channels</h3>

This area shows the control GUIs of the channels currently active for the device. When the preset is saved (as default at exit time or as a saved preset) the GUIs are ordered by increasing frequency. If presets share the same frequency they are ordered by their internal ID name. Thus new channel GUIs will appear ordered only when reloaded.

Details about the GUIs can be found in the channel plugins documentation which consists of a readme.md file in each of the channel plugins folder (done partially).

<h4>6.1. Basic channel settings</h4>

![Channel control 01](../doc/img/MainWindow_channel_01.png)

With most channel types some common basic settings can be set with a popup dialog. This dialog is opened by clicking on the small grey square on the top left of the channel window. The settings are as follows:

![Basic channel settings](../doc/img/BasicChannelSettings.png)

<h5>6.1.1: Window title</h5>

Changes the channel window title

<h5>6.1.2: Channel color</h5>

Changes the color of the window title bar and spectrum overlay. To change the color click on the color square to open a color chooser dialog. The hex rgb value is displayed next to the color square.

<h5>6.1.3: Frequency scale display type</h5>

When the mouse is over the channel window or over the central line in the spectrum a channel parameter is displayed on the frequency scale. This parameter can be:

  - Freq: channel absolute center frequency
  - Title: channel window title
  - AdSnd: UDP address and send port
  - AdRcv: UDP address and receive port

<h5>6.1.4: Toggle reverse API feature</h5>

Use this checkbox to toggle on/off the reverse API feature. With reverse API engaged the changes in the channel settings are forwarded to an API endpoint given by address (6.5), port (6.6), device index (6.7) and channel index (6.8) in the same format as the SDRangel REST API channel settings endpoint. With the values of the screenshot the API URL is: `http://127.0.0.1:8888/sdrangel/deviceset/0/channel/0/settings` The JSON payload follows the same format as the SDRangel REST API channel settings. Using the same example this would be:

```
{
  "SSBDemodSettings": {
    "agc": 0,
    "agcClamping": 0,
    "agcPowerThreshold": -40,
    "agcThresholdGate": 4,
    "agcTimeLog2": 7,
    "audioBinaural": 0,
    "audioDeviceName": "System default device",
    "audioFlipChannels": 0,
    "audioMute": 0,
    "dsb": 0,
    "inputFrequencyOffset": 0,
    "lowCutoff": 300,
    "rfBandwidth": 3000,
    "rgbColor": -16711936,
    "spanLog2": 3,
    "title": "SSB Demodulator",
    "volume": 3
  },
  "channelType": "SSBDemod",
  "tx": 0
}
```
Note that the PATCH method is used. The full set of parameters is sent only when the reverse API is toggled on or a full settings update is done.

More details on this feature can be found on the corresponding Wiki page.

<h5>6.1.5: API address</h5>

This is the IP address of the API endpoint

<h5>6.1.6: API port</h5>

This is the IP port of the API endpoint

<h5>6.1.7: Device index</h5>

This is the targeted device index

<h5>6.1.8: Channel index</h5>

This is the targeted channel index

<h5>6.1.9: Cancel changes and exit dialog</h5>

Do not make any changes and exit dialog

<h5>6.1.10: Validate and exit dialog</h5>

Validates the data (saves it in the channel marker object) and exits the dialog

<h4>6.2 Device stream assignment</h4>

![Channel control 02](../doc/img/MainWindow_channel_02.png)

The bigger square next to the leftmost "c" square is the device stream assignment control. With single Rx (source device set) and single Tx devices (sink device set) this is inactive because the channel is simply connected to the single stream as shown by the "S" letter.

This is in place for future MIMO devices and channels support (v.5).

<h3>7. Spectrum from device</h3>

This shows the spectrum in the passband returned from the sampling device possibly after decimation. The actual sample rate is shown in the device control at the left of the frequency display (2.3)

The spectrum display is controlled by the display control (4).

<h3>8. Status</h3>

![Main Window status](../doc/img/MainWindow_status.png)

<h4>8.1. SDRangel version</h4>

This is the current tag or the latest tag followed by the number of commits since the latest tag followed by the git commit SHA1 (8 hex characters) preceded by 'g'. Ex: `v4.5.3-29-gf5f2349d`

<h4>8.2. Qt version</h4>

Qt version with which this copy of SDRangel was compiled.

<h4>8.3. Architecture</h4>

Codename of the CPU architecture in which SDRangel is running.

<h4>8.4. Operating system</h4>

Pretty print of the operating system in which SDRangel is running.

<h4>8.5. Local date and time</h4>

Local time timestamp according to system clock. Format: `yyyy-mm-dd HH:MM:ss TZ`
