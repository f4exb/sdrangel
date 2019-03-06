project(samplesink)

find_package(LibUSB)

find_package(LibBLADERF)
if(LIBUSB_FOUND AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf1output)
    add_subdirectory(bladerf2output)
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

find_package(LibXTRX)
if(LIBXTRX_FOUND)
	add_subdirectory(xtrxoutput)
endif(LIBXTRX_FOUND)

find_package(SoapySDR)
if(LIBUSB_FOUND AND SOAPYSDR_FOUND)
add_subdirectory(soapysdroutput)
endif()

find_package(CM256cc)
if(CM256CC_FOUND)
    add_subdirectory(remoteoutput)
endif(CM256CC_FOUND)

if (BUILD_DEBIAN)
    add_subdirectory(bladerf1output)
    add_subdirectory(bladerf2output)
    add_subdirectory(hackrfoutput)
    add_subdirectory(limesdroutput)
    add_subdirectory(plutosdroutput)
    add_subdirectory(remoteoutput)
endif (BUILD_DEBIAN)

add_subdirectory(filesink)
