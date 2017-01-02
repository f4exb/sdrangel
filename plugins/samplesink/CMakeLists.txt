project(samplesink)

find_package(LibUSB)

find_package(LibBLADERF)
if(LIBUSB_FOUND AND LIBBLADERF_FOUND)
    add_subdirectory(bladerfoutput)
endif(LIBUSB_FOUND AND LIBBLADERF_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(bladerfoutput)
endif (BUILD_DEBIAN)

add_subdirectory(filesink)

