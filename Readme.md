For Ubuntu:

	sudo apt-get install libqt5multimedia5-plugins qtmultimedia5-dev qttools5-dev qttools5-dev-tools libqt5opengl5-dev qtbase5-dev libusb-1.0 librtlsdr-dev
	mkdir out && cd out && cmake ../ && make

"librtlsdr-dev" is in the "universe" repo. (utopic 14.10 amd64.)

Use "cmake ../ -DKERNEL=ON" to build the Linux kernel driver. May support Airspy and Hackrf.

