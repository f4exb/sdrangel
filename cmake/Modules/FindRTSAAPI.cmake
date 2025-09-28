if (NOT RTSAAPI_FOUND)

    include(FindPackageHandleStandardArgs)

    # Set this paths according to installation location of your RTSA SUITE Pro
    set(AARONIA_RTSA_INSTALL_DIRECTORY "/opt/aaronia-rtsa-suite/Aaronia-RTSA-Suite-PRO")

    message(STATUS "RTSA installation directory: ${AARONIA_SDK_DIRECTORY}")

    find_path(RTSAAPI_INCLUDE_DIR
        NAMES aaroniartsaapi.h
        HINTS ${AARONIA_RTSA_INSTALL_DIRECTORY}/sdk
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH
    )

    find_library(RTSAAPI_LIBRARY
        NAMES AaroniaRTSAAPI
        HINTS ${AARONIA_RTSA_INSTALL_DIRECTORY}
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH
    )

    if(RTSAAPI_INCLUDE_DIR AND RTSAAPI_LIBRARY)
        set(RTSAAPI_FOUND TRUE CACHE INTERNAL "RTSA API found")
        message(STATUS "Found RTSA API: ${RTSAAPI_INCLUDE_DIR}, ${RTSAAPI_LIBRARY}")
    else()
        set(RTSAAPI_FOUND FALSE CACHE INTERNAL "RTSA API found")
        message(STATUS "RTSA API not found.")
    endif()

    mark_as_advanced(RTSAAPI_INCLUDE_DIR RTSAAPI_LIBRARY)

endif()
