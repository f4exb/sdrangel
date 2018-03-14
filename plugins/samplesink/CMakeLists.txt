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

find_package(LimeSuite)
if(LIBUSB_FOUND AND LIMESUITE_FOUND)
    add_subdirectory(limesdroutput)
endif(LIBUSB_FOUND AND LIMESUITE_FOUND)

find_package(LibIIO)
if(LIBUSB_FOUND AND LIBIIO_FOUND)
    add_subdirectory(plutosdroutput)
endif(LIBUSB_FOUND AND LIBIIO_FOUND)

find_package(CM256cc)
find_package(LibNANOMSG)
if(CM256CC_FOUND AND LIBNANOMSG_FOUND)
    add_subdirectory(sdrdaemonsink)
endif(CM256CC_FOUND AND LIBNANOMSG_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(bladerfoutput)
    add_subdirectory(hackrfoutput)
    add_subdirectory(limesdroutput)
    if (LIBNANOMSG_FOUND)
        add_subdirectory(sdrdaemonsink)
    endif (LIBNANOMSG_FOUND)
    add_subdirectory(plutosdroutput)
endif (BUILD_DEBIAN)

add_subdirectory(filesink)

