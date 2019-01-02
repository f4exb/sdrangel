# Find Soapy SDR

if (NOT SOAPYSDR_INCLUDE_DIR)
    find_path (SOAPYSDR_INCLUDE_DIR
        NAMES SoapySDR/Version.h
        PATHS ${SOAPYSDR_DIR}/include
              /usr/include
              /usr/local/include
    )
endif()

if (NOT SOAPYSDR_LIBRARY)
    find_library (SOAPYSDR_LIBRARY
        NAMES SoapySDR
        HINTS ${CMAKE_INSTALL_PREFIX}/lib
              ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS ${SOAPYSDR_DIR}/lib
              ${SOAPYSDR_DIR}/lib64
              /usr/local/lib
              /usr/local/lib64
              /usr/lib
              /usr/lib64
    )
endif()

if (SOAPYSDR_INCLUDE_DIR AND SOAPYSDR_LIBRARY)
    set(SOAPYSDR_FOUND TRUE)
endif()

if (SOAPYSDR_FOUND)
    message (STATUS "Found SoapySDR: ${SOAPYSDR_INCLUDE_DIR}, ${SOAPYSDR_LIBRARY}")
else()
    message (STATUS "Could not find SoapySDR")
endif()

mark_as_advanced(SOAPYSDR_INCLUDE_DIR SOAPYSDR_LIBRARY)
