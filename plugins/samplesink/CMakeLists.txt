project(samplesink)

add_subdirectory(testsink)
add_subdirectory(fileoutput)
add_subdirectory(localoutput)
add_subdirectory(audiooutput)

if (CM256CC_FOUND AND (HAS_SSE3 OR HAS_NEON))
    add_subdirectory(remoteoutput)
endif()

if(ENABLE_BLADERF AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf1output)
    add_subdirectory(bladerf2output)
endif()

if(ENABLE_HACKRF AND LIBHACKRF_FOUND)
    add_subdirectory(hackrfoutput)
endif()

if(ENABLE_LIMESUITE AND LIMESUITE_FOUND)
    add_subdirectory(limesdroutput)
endif()

if(ENABLE_XTRX AND LIBXTRX_FOUND)
	add_subdirectory(xtrxoutput)
endif()

if(ENABLE_IIO AND LIBIIO_FOUND)
    add_subdirectory(plutosdroutput)
endif()

if(ENABLE_SOAPYSDR AND SOAPYSDR_FOUND)
    add_subdirectory(soapysdroutput)
endif()

if(ENABLE_USRP AND UHD_FOUND)
    add_subdirectory(usrpoutput)
endif()
