project(devices)

find_package(LibUSB)

find_package(LibBLADERF)
if(LIBUSB_FOUND AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf)
endif(LIBUSB_FOUND AND LIBBLADERF_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(bladerf)
endif (BUILD_DEBIAN)

