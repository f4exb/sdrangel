# If CODEC2_DIR is specified, treat it as an explicit user override and
# search that installation directly. Otherwise use pkg-config to verify
# the minimum supported codec2 version before locating the headers and
# library.

if (NOT CODEC2_FOUND)
    if (CODEC2_DIR)
        message(STATUS "Using CODEC2_DIR: ${CODEC2_DIR}")
    else()
        find_package(PkgConfig QUIET)
        if(NOT PKG_CONFIG_FOUND)
            message(STATUS "codec2 support disabled: pkg-config is required to verify codec2 version.")
            return()
        endif()

        pkg_check_modules(PC_CODEC2 QUIET codec2>=1.1.1)
        if(NOT PC_CODEC2_FOUND)
            message(STATUS "codec2 support disabled: codec2 >= 1.1.1 was not found.")
            return()
        endif()
    endif()

    FIND_PATH(CODEC2_INCLUDE_DIR
        NAMES codec2/codec2.h
        HINTS ${CODEC2_DIR}/include
              ${PC_CODEC2_INCLUDE_DIRS}
              ${CMAKE_INSTALL_PREFIX}/include
        PATHS /usr/local/include
              /usr/include
    )

    FIND_LIBRARY(CODEC2_LIBRARIES
        NAMES codec2 libcodec2
        HINTS ${CODEC2_DIR}/lib
              ${CODEC2_DIR}/lib64
              ${PC_CODEC2_LIBRARY_DIRS}
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
