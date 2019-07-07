project(samplesource)

add_subdirectory(fileinput)
add_subdirectory(testsource)
add_subdirectory(localinput)

if(CM256CC_FOUND)
    add_subdirectory(remoteinput)
endif(CM256CC_FOUND)

if(ENABLE_AIRSPY AND LIBAIRSPY_FOUND)
    add_subdirectory(airspy)
endif()

if(ENABLE_AIRSPYHF AND LIBAIRSPYHF_FOUND)
    add_subdirectory(airspyhf)
endif()

if(ENABLE_BLADERF AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf1input)
    add_subdirectory(bladerf2input)
endif()

if(ENABLE_FUNCUBE AND UNIX AND LIBUSB_FOUND)
    add_subdirectory(fcdpro)
    add_subdirectory(fcdproplus)
endif()

if(ENABLE_HACKRF AND LIBHACKRF_FOUND)
    add_subdirectory(hackrfinput)
endif()

if(ENABLE_LIMESUITE AND LIMESUITE_FOUND)
    add_subdirectory(limesdrinput)
endif()

if(ENABLE_MIRISDR AND LIBMIRISDR_FOUND)
    add_subdirectory(sdrplay)
endif()

if(ENABLE_PERSEUS AND LIBPERSEUS_FOUND AND RX_SAMPLE_24BIT)
    add_subdirectory(perseus)
endif()

if(ENABLE_XTRX AND LIBXTRX_FOUND)
    add_subdirectory(xtrxinput)
endif()

if(ENABLE_IIO AND LIBIIO_FOUND)
    add_subdirectory(plutosdrinput)
endif()

if(ENABLE_RTLSDR AND LIBRTLSDR_FOUND)
    add_subdirectory(rtlsdr)
endif()

if(ENABLE_SOAPYSDR AND SOAPYSDR_FOUND)
    add_subdirectory(soapysdrinput)
endif()

add_subdirectory(kiwisdr)