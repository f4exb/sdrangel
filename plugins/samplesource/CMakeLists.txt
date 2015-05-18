project(samplesource)

find_package(LibUSB)
#find_package(LibOsmoSDR)

add_subdirectory(gnuradio)
#add_subdirectory(remote)

#if(LIBUSB_FOUND AND LIBOSMOSDR_FOUND)
#	add_subdirectory(osmosdr)
#endif(LIBUSB_FOUND AND LIBOSMOSDR_FOUND)

if(V4L-RTL)
	FIND_LIBRARY (LIBV4L2 v4l2)
	FIND_PATH (LIBV4L2H libv4l2.h)
	add_subdirectory(v4l-rtl)
endif()
if(V4L-MSI)
        FIND_LIBRARY (LIBV4L2 v4l2)
	FIND_PATH (LIBV4L2H libv4l2.h)
	add_subdirectory(v4l-msi)
endif()

if(LIBUSB_FOUND AND UNIX)
	FIND_PATH (ASOUNDH alsa/asoundlib.h)
	FIND_LIBRARY (LIBASOUND asound)
endif()
if(LIBASOUND AND ASOUNDH)
	add_subdirectory(fcd)
endif()

find_package(LibRTLSDR)
if(LIBUSB_FOUND AND LIBRTLSDR_FOUND)
	add_subdirectory(rtlsdr)
endif(LIBUSB_FOUND AND LIBRTLSDR_FOUND)

