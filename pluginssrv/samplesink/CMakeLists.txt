project(samplesink)

find_package(LibUSB)

find_package(LimeSuite)
if(LIBUSB_FOUND AND LIMESUITE_FOUND)
    add_subdirectory(limesdroutput)
endif(LIBUSB_FOUND AND LIMESUITE_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(limesdroutput)
endif (BUILD_DEBIAN)

add_subdirectory(filesink)
