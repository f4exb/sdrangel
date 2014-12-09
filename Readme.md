============
For Debian 6:
============

Debian 6 uses Qt4. Qt5 is available from the "wheezy-backports" repo, but will remove Qt4.

Required Packages: "sudo apt-get install cmake g++ pkg-config libfftw3-dev libusb-1.0-0-dev libusb-dev qt5-default qtbase5-dev qtchooser libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev librtlsdr-dev"

Funcube Dongle Pro+ support will need "libasound2-dev" installed. "libhid" is now built from source.


==========
For Ubuntu:
==========
	sudo apt-get install libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev
	mkdir out && cd out && cmake ../ && make

"librtlsdr-dev" is in the "universe" repo. (utopic 14.10 amd64.)

Funcube Dongle Pro+ support needs package "libasound2-dev"

Use "cmake ../ -DKERNEL=ON" to build the Linux kernel driver (Experimental). Needs a recent kernel and libv4l2. Will need extra work to support Airspy and Hackrf. Needs "cp KERNEL_SOURCE/include/linux/compiler.h /usr/include/linux/" and "cp KERNEL_SOURCE/include/uapi/linux/videodev2.h /usr/include/uapi/linux/" and package "libv4l-dev".

The Gnuradio plugin source needs extra packages, including "liblog4cpp-dev libboost-system-dev gnuradio-dev libosmosdr-dev"

