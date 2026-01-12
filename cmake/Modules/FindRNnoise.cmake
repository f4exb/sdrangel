if (NOT RNNOISE_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(PC_RNNoise "librnnoise")

    FIND_PATH(RNNOISE_INCLUDE_DIR
        NAMES rnnoise.h
        HINTS ${RNNOISE_DIR}/include
            ${PC_RNNOISE_INCLUDE_DIR}
            ${CMAKE_INSTALL_PREFIX}/include
        PATHS /usr/local/include
            /usr/include
    )

    FIND_LIBRARY(RNNOISE_LIBRARIES
        NAMES rnnoise librnnoise
        HINTS ${RNNOISE_DIR}/lib
            ${RNNOISE_DIR}/lib64
            ${PC_RNNOISE_LIBDIR}
            ${CMAKE_INSTALL_PREFIX}/lib
            ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS /usr/local/lib
            /usr/local/lib64
            /usr/lib
            /usr/lib64
    )

    if (RNNOISE_INCLUDE_DIR AND RNNOISE_LIBRARIES)
        set(RNNOISE_FOUND TRUE CACHE INTERNAL "RNNoise found")
        message(STATUS "Found RNNoise: ${RNNOISE_INCLUDE_DIR}, ${RNNOISE_LIBRARIES}")
    else (RNNOISE_INCLUDE_DIR AND RNNOISE_LIBRARIES)
        set(RNNOISE_FOUND FALSE CACHE INTERNAL "RNNoise found")
        message(STATUS "RNNoise not found")
    endif (RNNOISE_INCLUDE_DIR AND RNNOISE_LIBRARIES)

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(RNNoise DEFAULT_MSG RNNOISE_LIBRARIES RNNOISE_INCLUDE_DIR)
    MARK_AS_ADVANCED(RNNOISE_LIBRARIES RNNOISE_INCLUDE_DIR)
endif (NOT RNNOISE_FOUND)
