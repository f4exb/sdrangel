project(samplesource)

add_subdirectory(fileinput)
add_subdirectory(testsource)
add_subdirectory(localinput)

if (CM256CC_FOUND AND (HAS_SSE3 OR HAS_NEON))
    add_subdirectory(remoteinput)
endif()

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

if(ENABLE_FUNCUBE AND LIBUSB_FOUND AND HIDAPI_FOUND)
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

if(ENABLE_SDRPLAY AND SDRPLAY_FOUND)
    add_subdirectory(sdrplayv3)
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

if(LIBSIGMF_FOUND AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_subdirectory(sigmffileinput)
endif()

if(ENABLE_USRP AND UHD_FOUND)
    add_subdirectory(usrpinput)
endif()

add_subdirectory(audioinput)
add_subdirectory(kiwisdr)
add_subdirectory(remotetcpinput)
add_subdirectory(aaroniartsainput)
