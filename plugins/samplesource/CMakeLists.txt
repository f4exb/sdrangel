project(samplesource)

find_package(LibUSB)
find_package(LibOsmoSDR)
find_package(LibRTLSDR)

add_subdirectory(gnuradio)

if(LIBUSB_FOUND AND LIBOSMOSDR_FOUND)
	add_subdirectory(osmosdr)
endif(LIBUSB_FOUND AND LIBOSMOSDR_FOUND)

if(LIBUSB_FOUND AND LIBRTLSDR_FOUND)
	add_subdirectory(rtlsdr)
endif(LIBUSB_FOUND AND LIBRTLSDR_FOUND)
