project(devices)

find_package(LibUSB)

if (BUILD_DEBIAN)
    add_subdirectory(bladerf)
    add_subdirectory(hackrf)
    add_subdirectory(limesdr)
else(BUILD_DEBIAN)
    find_package(LibBLADERF)
    if(LIBUSB_FOUND AND LIBBLADERF_FOUND)
        add_subdirectory(bladerf)
    endif(LIBUSB_FOUND AND LIBBLADERF_FOUND)

    find_package(LibHACKRF)
    if(LIBUSB_FOUND AND LIBHACKRF_FOUND)
        add_subdirectory(hackrf)
    endif(LIBUSB_FOUND AND LIBHACKRF_FOUND)

    find_package(LimeSuite)
    if(LIBUSB_FOUND AND LIMESUITE_FOUND)
        add_subdirectory(limesdr)
    endif(LIBUSB_FOUND AND LIMESUITE_FOUND)
endif (BUILD_DEBIAN)
