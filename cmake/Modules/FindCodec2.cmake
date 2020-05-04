if (NOT CODEC2_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(PC_CODEC2 "codec2")

    FIND_PATH(CODEC2_INCLUDE_DIR
        NAMES codec2/codec2.h
        HINTS ${CODEC2_DIR}/include
              ${PC_CODEC2_INCLUDE_DIR}
              ${CMAKE_INSTALL_PREFIX}/include
        PATHS /usr/local/include
              /usr/include
    )

    FIND_LIBRARY(CODEC2_LIBRARIES
        NAMES codec2 libcodec2
        HINTS ${CODEC2_DIR}/lib
              ${CODEC2_DIR}/lib64
              ${PC_CODEC2_LIBDIR}
              ${CMAKE_INSTALL_PREFIX}/lib
              ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS /usr/local/lib
              /usr/local/lib64
              /usr/lib
              /usr/lib64
    )

    if(CODEC2_INCLUDE_DIR AND CODEC2_LIBRARIES)
        set(CODEC2_FOUND TRUE CACHE INTERNAL "CODEC2 found")
        message(STATUS "Found Codec2: ${CODEC2_INCLUDE_DIR}, ${CODEC2_LIBRARIES}")
    else()
        set(CODEC2_FOUND FALSE CACHE INTERNAL "CODEC2 found")
        message(STATUS "Codec2 not found")
    endif()

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(CODEC2 DEFAULT_MSG CODEC2_LIBRARIES CODEC2_INCLUDE_DIR)
    MARK_AS_ADVANCED(CODEC2_LIBRARIES CODEC2_INCLUDE_DIR)
endif (NOT CODEC2_FOUND)