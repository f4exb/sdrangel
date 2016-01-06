![SDR Angel banner](/doc/img/sdrangel_banner.png)

**SDRangel** is an Open Source Qt5/OpenGL SDR and signal analyzer frontend to various hardware. 

Although it keeps the same look and feel as its parent application **SDRangelove** it is a major redesign from it hitting more than half the lines of the code. Therefore the code base cannot be kept in sync anymore with its parent. It also contains enhancements and major differences. So it should now fly with its own wings and with its own name: **SDRangel**

<h1>Source code</h1>

<h2>Repository branches</h2>

- master: the production branch
- dev: the development branch
- fix: production fixes that can't wait
- legacy: the modified code from the parent application [hexameron rtl-sdrangelove](https://github.com/hexameron/rtl-sdrangelove) before a major redeisign of the code was carried out and sync was lost.

<h2>Untested plugins</h2>

These plugins come from the parent code base and have been maintained so that they compile but they are not being actively tested:

- Channels:
  - lora
  - tcpsrc (although it has evolved please use the udpsrc plugin instead)

<h2>Unsupported plugins</h2>

These plugins come from the parent code base and are still present in the source tree but are not part of the build:

- Channels:
  - tetra 
- Sample sources:
  - gnuradio
  - osmosdr
  - v4l-msi
  - v4l-rtl
  
<h3>Gnuradio</h3>

The Gnuradio plugin source needs extra packages, including `liblog4cpp-dev libboost-system-dev gnuradio-dev libosmosdr-dev`

If you use your own location for Gnuradio install directory you need to specify library and include locations. Example with `/opt/install/gnuradio-3.7.5.1` with the following defines on `cmake` command line:

`-DGNURADIO_RUNTIME_LIBRARIES=/opt/install/gnuradio-3.7.5.1/lib/libgnuradio-runtime.so -DGNURADIO_RUNTIME_INCLUDE_DIRS=/opt/install/gnuradio-3.7.5.1/include`

<h3>osmosdr</h3>

If you use your own location for gr.osmocom install directory you need to specify library and include locations. Example with `/opt/install/gr-osmosdr` with the following defines on `cmake` command line:

`-DGNURADIO_OSMOSDR_LIBRARIES=/opt/install/gr-osmosdr/lib/libgnuradio-osmosdr.so -DGNURADIO_OSMOSDR_INCLUDE_DIRS=/opt/install/gr-osmosdr/include`

<h3>v4l-*</h3>

Use `cmake ../ -DV4L-RTL=ON` to build the Linux kernel driver for RTL-SDR (Experimental). Needs a recent kernel and libv4l2. Will need extra work to support SDRPlay. Needs `cp KERNEL_SOURCE/include/linux/compiler.h /usr/include/linux/` and `cp KERNEL_SOURCE/include/uapi/linux/videodev2.h /usr/include/uapi/linux/` and package `libv4l-dev`.

<h1>Supported hardware</h1>

<h2>Airspy</h2>

Airspy is supported through the libairspy library that should be installed in your system for proper build of the software and operation support. Add `libairspy-dev` to the list of dependencies to install.

If you use your own location for libairspy install directory you need to specify library and include locations. Example with `/opt/install/libairspy` with the following defines on `cmake` command line:

`-DLIBAIRSPY_LIBRARIES=/opt/install/libairspy/lib/libairspy.so -DLIBAIRSPY_INCLUDE_DIR=/opt/install/libairspy/include`

Please note that if you are using a recent version of libairspy (>= 1.0.6) the dynamic retrieval of sample rates is supported. To benefit from it you should modify the `plugins/samplesource/airspy/CMakeLists.txt` and change line `add_definitions(${QT_DEFINITIONS})` by `add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")`. In fact both lines are present with the last one commented out. 

Be also aware that the lower rates (2.5 MS/s or 5 MS/s with modified firmware) are affected by a noise artifact so 10 MS/s is preferable for weak signal work or instrumentation. A decimation by 64 was implemented to facilitate narrow band work at 10 MS/s input rate.

<h2>BladeRF</h2>

BladeRF is supported through the libbladerf library that should be installed in your system for proper build of the software and operation support. Add `libbladerf-dev` to the list of dependencies to install.

If you use your own location for libbladeRF install directory you need to specify library and include locations. Example with `/opt/install/libbladerf` with the following defines on `cmake` command line:

`-DLIBBLADERF_LIBRARIES=/opt/install/libbladeRF/lib/libbladeRF.so -DLIBBLADERF_INCLUDE_DIR=/opt/install/libbladeRF/include`

<h2>FunCube Dongle</h2>

Both Pro and Pro+ are supported with the plugins in fcdpro and fcdproplus respectively. For the Pro+ the band filter selection is not effective as it is handled by the firmware using the center frequency.

The control interface is based on qthid and has been built in the software in the fcdhid library. You don't need anything else than libusb support. Library fcdlib is used to store the constants for each dongle type.

The Pro+ has trouble starting. The sound card interface is not recognized when you just plug it in and start SDRAngel. The workaround is to start qthid then a recording program like Audacity and start recording in Audacity. Then just quit Audacity without saving and quit qthid. After this operation the Pro+ should be recognized by SDRAngel until you unplug it.

<h2>HackRF</h2>

HackRF is supported through the libhackrf library that should be installed in your system for proper build of the software and operation support. Add `libhackrf-dev` to the list of dependencies to install. Please note that you will need a recent version (2015.07.2 or 2015.07.1 at least) of libhackrf that supports the sequential listing of devices so you might need to build and install the Github version: `https://github.com/mossmann/hackrf.git`. Note also that the firmware must be updated to match the library version as per instructions found in the HackRF wiki.

If you use your own location for libhackrf install directory you need to specify library and include locations. Example with `/opt/install/libhackrf` with the following defines on `cmake` command line:

`-DLIBHACKRF_LIBRARIES=/opt/install/libhackrf/lib/libhackrf.so -DLIBHACKRF_INCLUDE_DIR=/opt/install/libhackrf/include`

HackRF is better used with a sampling frequency over 8 MS/s. You can use the 9.6Ms/s setting that decimates nicely into integer kS/s sample rates. There are quite a few problems with narrowband work with this hardware. You may try various amplifiers settings to limit images and I/Q imbalance with varying success... The cheap RTL-SDR dongles usually do better.

<h2>RTL-SDR</h2>

RTL-SDR based dongles are supported through the librtlsdr library that should be installed in your system for proper build of the software and operation support. Add `librtlsdr-dev` to the list of dependencies to install.

If you use your own location for librtlsdr install directory you need to specify library and include locations. Example with `/opt/install/librtlsdr` with the following defines on `cmake` command line:

`-DLIBRTLSDR_LIBRARIES=/opt/install/librtlsdr/lib/librtlsdr.so -DLIBRTLSDR_INCLUDE_DIR=/opt/install/librtlsdr/include`

<h1>Software build</h1>

<h2>For Ubuntu</h2>

On 14.04 LTS do this first:
`sudo apt-get install libgles2-mesa-dev`

Then:
`sudo apt-get install cmake g++ pkg-config libfftw3-dev libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev libboost-all-dev`

`mkdir build && cd build && cmake ../ && make`

`librtlsdr-dev` is in the `universe` repo. (utopic 14.10 amd64.)

There is no installation procedure the executable is at the root of the build directory

<h2>For Debian</h2>

For any version of Debian you will need Qt5.

Debian 7 "wheezy" uses Qt4. Qt5 is available from the "wheezy-backports" repo, but this will remove Qt4. Debian 8 "jessie" uses Qt5. 

For Debian Jessie or Stretch:

`sudo apt-get install cmake g++ pkg-config libfftw3-dev libusb-1.0-0-dev libusb-dev qt5-default qtbase5-dev qtchooser libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev librtlsdr-dev libboost-all-dev`

`mkdir build && cd build && cmake ../ && make`

<h1>Known Issues</h1>

  - The message queuing model supports a n:1 connection to an object (on its input queue) and a 1:1 connection from an object (on its output queue). Assuming a different model can cause insidious disruptions.
  - As the objects input and output queues can be publicly accessed there is no strict control of which objects post messages on these queues. The correct assumption is that messages can be popped from the input queue only by its holder and that messages can be pushed on the output queue only by its holder.
  - Objects managing more than one message queue (input + output for example) do not work well under stress conditions. Output queue removed from sample sources but this model has to be revised throughout the application.

<h1>Limitations</h1>

  - Tabbed panels showing "X0" refer to the only one selected device it is meant to be populated by more tabs when it will support more than one device possibly Rx + Tx.

<h1>Changes from SDRangelove</h1>

<h2>New features, enhancements and fixes</h2>

  - Added ppm correction for the LO of RTL-SDR. This uses the corresponding function in the librtlsdr interface (range -99..99 ppm)
  - Added a preset update button (the diskette with the yellow corner) to be able to save the current settings on an existing preset
  - Added variable decimation in log2 increments from 2^0=1 to 2^4=16 allowing to see the full 2048 kHz of spectrum if so you wish
  - Better handling of rtlsdr GUI display when settings change (initial load, load of presets)
  - Added display and precise control of the shift frequency from center frequency of the NFM receivers.
  - Removed useless spectrum visualizer in NFM receivers. Created a null sink class to fit corresponding parameter in NFMDemod class constructor.
  - Added display and precise control of the shift frequency from center frequency of the SSB receivers.
  - SSB filter bounds are tunable so that filter can be set off from center frequency allowing aural decoding of CW
  - Make the sidebands appear correctly on SSB channel overlay. Limit to +/- 6 kHz to fit channel spectrum analyzer window
  - SSB bandwidth can now be tuned in steps of 100 Hz
  - NFM and SSB receiver in focus trigger the display of the central frequency line on the spectrum frequency scale thus facilitating its identification
  - Added AM demod so now you can listen to air traffic!
  - Added the possibility to change the brightness and/or color of the grid.
  - Make the low cutoff frequency of the SSB filter variable so it can be used for CW also.
  - NFM demodulation without using atan and smooth squelch with AGC suppressing most clicks on low level signals and hiss on carrier tails. Only useful modulation comes through.
  - Added working WFM demodulation. Optimized for no atan2.
  - OsmoSDR and GNURadio plugins removed from the build as they have too many bugs that are too difficult to correct
  - New plugin for BladeRF interfacing libbladeRF directly
  - Corrected the nasty audio band resampling bug preventing use of sample rates that are not power of 2 multiples of 48kHz. This was because the resampling ratio was calculated with an integer division instead of a float division. 
  - As a consequence of the above added more interesting values for the available sampling rates of the BladeRF plugin
  - Variable span for the SSB demod down to 1.5 kHz
  - Filter out CTCSS tones for audio and full CTCSS support in NFMDemod
  - Enhancement of the NFM squelch mimicking professional analog squelch circuits (based on balance between two AF filters)
  - Added a channel analyzer plugin focusing on measurement (DSA/DSO functionnality). Basic functions.
  - Added a scope widget in the channel analyzer plugin
  - Channel analyzer bandwidth up to half the available RF (IF) bandwidth (was 48k fixed)
  - Enhanced scope display and controls: scale display, better X (time) and Y scales control, grid fit to scale, effectively implementing triggers, trigger on magnitude and phase, properly handling time shift, ...
  - Enhanced spectrum display: Histogram: wider decay range, make stroke and late holdoff adjustable. Added option to show live spectrum (had only max hold before).
  - Enhanced channel analyzer: enhanced scope and spectrum displays as mentioned above, make the spectrum display synchronous to scope (hence triggerable a la E4406A).
  - Sort channel plugins by delta frequency and type before saving to preset
  - Implemented scope pre-trigger delay
  - Implemented variable scope memory depth
  - Implemented trigger delay
  - Trigger on both edges
  - Spectrum histogram clear
  - Trigger line display for all trigger modes
  - Coarse and fine trigger level sliders
  - Minimalist recording (no file choice)
  - File sample source plugin (recording reader)
  - Scope: trace history
  - Scope: trigger countdown
  - Scope: multiple trigger chaining
  - Scope: new mode with linear IQ (two traces) on the primary display and polar IQ on the secondary display
  - New USB source plugin to connect to an external demodulator (ex: GNU radio) via USB ports
  - Binaural option for SSB demod
  - DSB option for SSB
  - Mute option for NFM channel
  - New Broadcast FM demodulator with stereo support
  - RDS support in Broadcast FM demodulator
  
<h2>Major redesign</h2>

  - Make the DSP engine global static simplifying access to it. Have a distinct object to handle the thread
  - Fixed startup initialization sequence. New initialization phase in DSP engine and new ready state
  - Synchronous messaging class to push message to thread and wait for completion relieving the message queuing mechanism from this role
  - Message queuing and handling redesign. Still not completely satisfactory
  - Objects have their own input and output message queues
  - Dedicated message queue to communicate to the GUI for objects coupled with a GUI
  - Optimizations with Valgrind cachegrind
  - Corrected decimators bit shifting so that the number of effective output bits is constant and fill the internal sample width
  - Separate library to support all flavours of FunCube dongles. Rewritten FCD plugins.
  - Allow several sample source plugins to share the same presets for what is common
  - Prepare grounds for future Tx operations with Tx spectrum display in the main window
  - Merged include-gpl into include and removed GPL dribble in About dialog
  - Many other little things...
    
<h1>To Do</h1>
  - Allow the handling of more than one device at the same time. For Rx/Tx devices like the BladeRF Rx and Tx appear as two logical devices with two plugin instances and a common handler for the physical device services both plugins. This effectively opens Tx support.
  - Tx channels
  - Possibility to connect channels for example Rx to Tx or single Rx channel to dual Rx channel supporting MI(MO) features like 360 degree polarization detection.
  - Specialize plugins into channel and sample source plugins since both have almost complete different requirements and only little in common
  - 32 bit samples for the Channel Analyzer
  - Enhance presets management (Edit, Move, Import/Export from/to human readable format like JSON).
  - Headless mode based on a saved configuration in above human readable form
  - Allow arbitrary sample rate for channelizers and demodulators (not multiple of 48 kHz). Prerequisite for polyphase channelizer
  - Implement polyphase channelizer
  - Level calibration  
  - Even more demods ...
  
<h1>Developper's notes</h1>

<h2>Build options</h2>

The release type can be specified with the `-DBUILD_TYPE` cmake option. It takes the following values:
  - `RELEASE` (default): produces production release code i.e.optimized and no debug symbols
  - `RELEASEWITHDBGINFO`: optimized with debug info
  - `DEBUG`: unoptimized with debug info

You can specify whether or not you want to see debug messages printed out to the console with the `-DDEBUG_OUTPUT` cmake option:
  - `OFF` (default): no debug output
  - `ON`: debug output
