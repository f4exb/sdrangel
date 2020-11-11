project(devices)

include_directories(
    # export.h
    ${CMAKE_SOURCE_DIR}/exports
    # util/message.h
    ${CMAKE_SOURCE_DIR}/sdrbase
)

if(ENABLE_BLADERF AND LIBBLADERF_FOUND)
    add_subdirectory(bladerf1)
    add_subdirectory(bladerf2)
endif()

if(ENABLE_HACKRF AND LIBHACKRF_FOUND)
    add_subdirectory(hackrf)
endif()

if(ENABLE_LIMESUITE AND LIMESUITE_FOUND)
    add_subdirectory(limesdr)
endif()

if(ENABLE_XTRX AND LIBXTRX_FOUND)
    add_subdirectory(xtrx)
endif()

if(ENABLE_IIO AND LIBIIO_FOUND)
    add_subdirectory(plutosdr)
endif()

if(ENABLE_PERSEUS AND LIBPERSEUS_FOUND)
    add_subdirectory(perseus)
endif()

if(ENABLE_SOAPYSDR AND SOAPYSDR_FOUND)
    add_subdirectory(soapysdr)
endif()

if(ENABLE_USRP AND UHD_FOUND)
    add_subdirectory(usrp)
endif()

add_subdirectory(metis)
