project(samplesink)

find_package(LibUSB)

find_package(LibHACKRF)
if(LIBUSB_FOUND AND LIBHACKRF_FOUND)
    add_subdirectory(hackrfoutput)
endif(LIBUSB_FOUND AND LIBHACKRF_FOUND)

find_package(LimeSuite)
if(LIBUSB_FOUND AND LIMESUITE_FOUND)
    add_subdirectory(limesdroutput)
endif(LIBUSB_FOUND AND LIMESUITE_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(limesdroutput)
    add_subdirectory(hackrfoutput)
endif (BUILD_DEBIAN)

add_subdirectory(filesink)
