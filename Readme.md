======================
Cloning the repository
======================

- Clone as usual
- Checkout the f4exb branch: `git checkout f4exb`

=======================
GNURadio and libosmosdr
=======================

These plugins have a lot of bugs and they have been removed trom the build. Original sources still exist in the tree. So current version only supports the following hardware:
  - RTL-SDR
  - Funcube
  - BladeRF (standard and with XB-200 transverter) 

==============
Funcube Dongle
==============

Funcube Dongle Pro+ support will need "libasound2-dev" installed. ("libhid" is built from source). Install the rules file "fcdpp.rules" in "/etc/udev/rules.d" to gain the "root access" needed to control the dongle.

Funcube Dongle Pro+ USB drivers are broken on some hardware with recent kernels. It works well for me with an Atom Chipset, which has Full Speed USB ports, or a "Valleyview" Chromebook (kernel 3.10). A Desktop with the "same" chipset needs kernel 3.2, available with Debian 7 "wheezy". YMMV.

=======
BladeRF
=======

A complete new plugin has been written for the BladeRF that interfaces libbladeRF directly. Osmosdr/GnuRadio interface is not implemented correctly and has a lot of bugs as mentioned previously and was not reasonably usable with the BladeRF.

If you use your own location for libbladeRF install directory you need to specify library and include locations. Example with `opt/install/libbladerf` with the following defines on `cmake` command line:

`-DLIBBLADERF_LIBRARIES=/opt/install/libbladeRF/lib/libbladeRF.so -DLIBBLADERF_INCLUDE_DIR=/opt/install/libbladeRF/include`

==========
For Ubuntu
==========

`sudo apt-get install libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev`

`mkdir build && cd build && cmake ../ && make`

`librtlsdr-dev` is in the `universe` repo. (utopic 14.10 amd64.)

Use `cmake ../ -DV4L-RTL=ON` to build the Linux kernel driver for RTL-SDR (Experimental). Needs a recent kernel and libv4l2. Will need extra work to support SDRPlay. Needs `cp KERNEL_SOURCE/include/linux/compiler.h /usr/include/linux/` and `cp KERNEL_SOURCE/include/uapi/linux/videodev2.h /usr/include/uapi/linux/` and package `libv4l-dev`.

The Gnuradio plugin source needs extra packages, including `liblog4cpp-dev libboost-system-dev gnuradio-dev libosmosdr-dev`

For non standard installations of RTL-SDR library, the GNU Radio runtime and gr.osmocom drivers use the following variables in the cmake command line. The paths specified are just examples:

  - For GNU Radio runtime:
    - Includes: `-DGNURADIO_RUNTIME_INCLUDE_DIRS=/opt/install/gnuradio-3.7.5.1/include`
    - Library: `-DGNURADIO_RUNTIME_LIBRARIES=/opt/install/gnuradio-3.7.5.1/lib/libgnuradio-runtime.so`
  - For gr.osmocom:
    - Includes: `-DGNURADIO_OSMOSDR_INCLUDE_DIRS=/opt/install/gr-osmosdr/include`
    - Library: `-DGNURADIO_OSMOSDR_LIBRARIES=/opt/install/gr-osmosdr/lib/libgnuradio-osmosdr.so`
  - For RTL-SDR library:
    - Includes: `-DLIBRTLSDR_INCLUDE_DIR=/opt/install/rtlsdr/include`
    - Library: `-DLIBRTLSDR_LIBRARIES=/opt/install/rtlsdr/lib/librtlsdr.so`
    
There is no installation procedure the executable is at the root of the build directory

============
For Debian 8
============

Debian 7 "wheezy" uses Qt4. Qt5 is available from the "wheezy-backports" repo, but this will remove Qt4. Debian 8 "jessie" uses Qt5.

`sudo apt-get install cmake g++ pkg-config libfftw3-dev libusb-1.0-0-dev libusb-dev qt5-default qtbase5-dev qtchooser libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev librtlsdr-dev`

`mkdir build && cd build && cmake ../ && make`

The same remarks as for Ubuntu apply...

============
Known Issues
============

  - Actually NFM seems to be working pretty well
  - RTL frontend will have bad aliasing in noisy environments. Considering the size of the hardware there is no place for proper filters. With good filtering and a good antenna up front these devices work remarkably well for the price! 
  - Aliasing can be annoying for broadcast FM. In this case try to shift the signal until you find a clear background for your station. This is a limitation of the RTL hardware so just use this workaround.
  - GNU Radio plugin is not fully functional and has serious bugs (frequent segfaults). Trying to repair it is abandonned. 

===================
Done since the fork
===================

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
  - OsmoSDR and GNURAdio plugins removed from the build as they have too many bugs that are too difficult to correct
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
  - Enhanced spectrum display: Histogram: define NO_AVX, wider decay range, make stroke and late holdoff adjustable. Added option to show live spectrum (had only max hold before).
  - Enhanced channel analyzer: enhanced scope and spectrum displays as mentioned above, make the spectrum display synchronous to scope (hence triggerable a la E4406A).
  - Sort channel plugins by delta frequency and type before saving to preset
  - Implemented scope pre-trigger delay
    
=====
To Do
=====

  - Variable scope memory depth
  - Trigger delay
  - Enhance presets management (Edit, Move, Import/Export from/to human readable format like JSON)  
  - Level calibration
  - Enhance WFM (stereo, RDS?)
  - Even more demods ... 
  - recording capability
  - Tx channels for Rx/Tx boards like BladeRF
