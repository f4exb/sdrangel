==============
Funcube Dongle
==============

Funcube Dongle Pro+ support will need "libasound2-dev" installed. ("libhid" is built from source). Install the rules file "fcdpp.rules" in "/etc/udev/rules.d" to gain the "root access" needed to control the dongle.

Funcube Dongle Pro+ USB drivers are broken on some hardware with recent kernels. It works well for me with an Atom Chipset, which has Full Speed USB ports, or a "Valleyview" Chromebook (kernel 3.10). A Desktop with the "same" chipset needs kernel 3.2, available with Debian 7 "wheezy". YMMV.


==========
For Ubuntu
==========

"sudo apt-get install libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev"

"mkdir out && cd out && cmake ../ && make"

"librtlsdr-dev" is in the "universe" repo. (utopic 14.10 amd64.)

Use "cmake ../ -DV4L-RTL=ON" to build the Linux kernel driver for RTL-SDR (Experimental). Needs a recent kernel and libv4l2. Will need extra work to support SDRPlay. Needs "cp KERNEL_SOURCE/include/linux/compiler.h /usr/include/linux/" and "cp KERNEL_SOURCE/include/uapi/linux/videodev2.h /usr/include/uapi/linux/" and package "libv4l-dev".

The Gnuradio plugin source needs extra packages, including "liblog4cpp-dev libboost-system-dev gnuradio-dev libosmosdr-dev"


============
For Debian 8
============

Debian 7 "wheezy" uses Qt4. Qt5 is available from the "wheezy-backports" repo, but this will remove Qt4.

"sudo apt-get install cmake g++ pkg-config libfftw3-dev libusb-1.0-0-dev libusb-dev qt5-default qtbase5-dev qtchooser libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev librtlsdr-dev"

"mkdir out && cd out && cmake ../ && make"

============
Known Issues
============

FM is mostly untested.

RTL frontend will have bad aliasing in noisy environments.

