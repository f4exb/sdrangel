project(samplesource)

find_package(LibUSB)

if(V4L-RTL)
    FIND_LIBRARY (LIBV4L2 v4l2)
    FIND_PATH (LIBV4L2H libv4l2.h)
#   add_subdirectory(v4l-rtl)
endif()
if(V4L-MSI)
        FIND_LIBRARY (LIBV4L2 v4l2)
    FIND_PATH (LIBV4L2H libv4l2.h)
#   add_subdirectory(v4l-msi)
endif()

find_package(LibHACKRF)
if(LIBUSB_FOUND AND LIBHACKRF_FOUND)
    add_subdirectory(hackrfinput)
endif(LIBUSB_FOUND AND LIBHACKRF_FOUND)

find_package(LimeSuite)
if(LIBUSB_FOUND AND LIMESUITE_FOUND)
    add_subdirectory(limesdrinput)
endif(LIBUSB_FOUND AND LIMESUITE_FOUND)

find_package(LibRTLSDR)
if(LIBUSB_FOUND AND LIBRTLSDR_FOUND)
    add_subdirectory(rtlsdr)
endif(LIBUSB_FOUND AND LIBRTLSDR_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(hackrfinput)
    add_subdirectory(limesdrinput)
    add_subdirectory(rtlsdr)
endif (BUILD_DEBIAN)

add_subdirectory(filesource)
