==============
Funcube Dongle
==============

Funcube Dongle Pro+ support will need "libasound2-dev" installed. ("libhid" is now built from source). Install the rules file "fcdpp.rules" in "/etc/udev/rules.d" to gain the "root access" needed to control the dongle.

Funcube Dongle ProPlus support is broken on recent hardware / kernels. It only works well for me with an Atom Chipset, which has Full Speed USB ports. On my Desktop Intel chipset it needs kernel 3.2, available with Debian 7 "wheezy" or Ubuntu 12.4. Ubuntu 12.10 uses kernel 3.5, which might work with the FCDPP.


==========
For Ubuntu:
==========

"sudo apt-get install libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev"

"mkdir out && cd out && cmake ../ && make"

"librtlsdr-dev" is in the "universe" repo. (utopic 14.10 amd64.)

Use "cmake ../ -DV4L-RTL=ON" to build the Linux kernel driver for RTL-SDR (Experimental). Needs a recent kernel and libv4l2. Will need extra work to support SDRPlay. Needs "cp KERNEL_SOURCE/include/linux/compiler.h /usr/include/linux/" and "cp KERNEL_SOURCE/include/uapi/linux/videodev2.h /usr/include/uapi/linux/" and package "libv4l-dev".

The Gnuradio plugin source needs extra packages, including "liblog4cpp-dev libboost-system-dev gnuradio-dev libosmosdr-dev"


============
For Debian 7:
============

Debian 7 "wheezy" uses Qt4. Qt5 is available from the "wheezy-backports" repo, but this will remove Qt4.

"sudo apt-get install cmake g++ pkg-config libfftw3-dev libusb-1.0-0-dev libusb-dev qt5-default qtbase5-dev qtchooser libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev librtlsdr-dev"

"mkdir out && cd out && cmake ../ && make"

============
Known Issues
============

Possible compiler/linker  bug when building with "make -j2"

FM is mostly untested.

RTL frontend will have bad aliasing in noisy environments.

