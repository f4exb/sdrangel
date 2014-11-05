project(samplesource)

find_package(LibUSB)
find_package(LibOsmoSDR)

add_subdirectory(gnuradio)

if(LIBUSB_FOUND AND LIBOSMOSDR_FOUND)
	add_subdirectory(osmosdr)
endif(LIBUSB_FOUND AND LIBOSMOSDR_FOUND)

if(KERNEL AND UNIX)
	add_subdirectory(v4l)
else()
	find_package(LibRTLSDR)
endif()

if(LIBUSB_FOUND AND LIBRTLSDR_FOUND)
	add_subdirectory(rtlsdr)
endif(LIBUSB_FOUND AND LIBRTLSDR_FOUND)
