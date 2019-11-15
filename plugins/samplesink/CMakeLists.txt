project(samplesink)

add_subdirectory(testsink)
add_subdirectory(filesink)
add_subdirectory(localoutput)

if(CM256CC_FOUND)
    add_subdirectory(remoteoutput)
endif(CM256CC_FOUND)

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
