# Find Soapy SDR

if (NOT SOAPYSDR_INCLUDE_DIR)
    FIND_PATH (SOAPYSDR_INCLUDE_DIR
        NAMES SoapySDR/Version.h
        PATHS
        /usr/include
        /usr/local/include
    )
endif()

if (NOT SOAPYSDR_LIBRARY)
    FIND_LIBRARY (SOAPYSDR_LIBRARY
        NAMES SoapySDR
        HINTS ${CMAKE_INSTALL_PREFIX}/lib
              ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS /usr/local/lib
              /usr/local/lib64
              /usr/lib
              /usr/lib64
    )
endif()

if (SOAPYSDR_INCLUDE_DIR AND SOAPYSDR_LIBRARY)
    SET(SOAPYSDR_FOUND TRUE)
endif()

if (SOAPYSDR_FOUND)
    MESSAGE (STATUS "Found SoapySDR: ${SOAPYSDR_INCLUDE_DIR}, ${SOAPYSDR_LIBRARY}")
else()
    MESSAGE (STATUS "Could not find SoapySDR")
endif()

mark_as_advanced(SOAPYSDR_INCLUDE_DIR SOAPYSDR_LIBRARY)
