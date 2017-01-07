project(devices)

find_package(LibUSB)

find_package(LibBLADERF)
if(LIBUSB_FOUND AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf)
endif(LIBUSB_FOUND AND LIBBLADERF_FOUND)

find_package(LibHACKRF)
if(LIBUSB_FOUND AND LIBHACKRF_FOUND)
    add_subdirectory(hackrf)
endif(LIBUSB_FOUND AND LIBHACKRF_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(bladerf)
    add_subdirectory(hackrf)
endif (BUILD_DEBIAN)

