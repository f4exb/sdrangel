======================
Cloning the repository
======================

- Clone as usual
- Checkout the f4exb branch: `git checkout f4exb`

==============
Funcube Dongle
==============

Funcube Dongle Pro+ support will need "libasound2-dev" installed. ("libhid" is built from source). Install the rules file "fcdpp.rules" in "/etc/udev/rules.d" to gain the "root access" needed to control the dongle.

Funcube Dongle Pro+ USB drivers are broken on some hardware with recent kernels. It works well for me with an Atom Chipset, which has Full Speed USB ports, or a "Valleyview" Chromebook (kernel 3.10). A Desktop with the "same" chipset needs kernel 3.2, available with Debian 7 "wheezy". YMMV.


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
  - WFM does not work for broadcast
  - NFM audio seems wrong (too slow) for RTL-SDR rates other than 1536 kHz. Maybe in SSB too.
  - RTL frontend will have bad aliasing in noisy environments. Considering the size of the hardware there is no place for proper filters. With good filtering and a good antenna up front these devices work remarkably well for the price! 

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
  - Make the sidebands appear correctly on SSB channel overlay. Limit to +/- 6 kHz to fit channel spectrum analyzer window
  - SSB bandwidth can now be tuned in steps of 100 Hz
  - NFM and SSB receiver in focus trigger the display of the central frequency line on the spectrum frequency scale thus facilitating its identification
  
  
=====
To Do
=====


  - AM demod. What about the air band?!
  - Add the possibility to change the brightness and/or color of the grid. Sometimes it is barely visible yet useful
  - Possibility to completely undock the receiver in a separate window. Useful when there are many receivers
  - Larger decimation capability for narrowband and very narrowband work (32, 64, ...)
  - Even more demods ... 
  - Triggering capability like on expensive spectrum analyzers to trap burst signals
  - recording capability
