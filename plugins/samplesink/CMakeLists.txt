project(samplesink)

find_package(LibUSB)

find_package(LibBLADERF)
if(LIBUSB_FOUND AND LIBBLADERF_FOUND)
    add_subdirectory(bladerfoutput)
endif(LIBUSB_FOUND AND LIBBLADERF_FOUND)

find_package(LibHACKRF)
if(LIBUSB_FOUND AND LIBHACKRF_FOUND)
    add_subdirectory(hackrfoutput)
endif(LIBUSB_FOUND AND LIBHACKRF_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(bladerfoutput)
    add_subdirectory(hackrfoutput)
endif (BUILD_DEBIAN)

add_subdirectory(filesink)

