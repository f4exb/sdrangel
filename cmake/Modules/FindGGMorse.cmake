if (NOT GGMORSE_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(PC_GGMorse "libggmorse")

    FIND_PATH(GGMORSE_INCLUDE_DIR
        NAMES ggmorse/ggmorse.h
        HINTS ${GGMORSE_DIR}/include
            ${PC_GGMORSE_INCLUDE_DIR}
            ${CMAKE_INSTALL_PREFIX}/include
        PATHS /usr/local/include
            /usr/include
    )

    FIND_LIBRARY(GGMORSE_LIBRARIES
        NAMES ggmorse libggmorse
        HINTS ${GGMORSE_DIR}/lib
            ${GGMORSE_DIR}/lib64
            ${PC_GGMORSE_LIBDIR}
            ${CMAKE_INSTALL_PREFIX}/lib
            ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS /usr/local/lib
            /usr/local/lib64
            /usr/lib
            /usr/lib64
    )

    if (GGMORSE_INCLUDE_DIR AND GGMORSE_LIBRARIES)
        set(GGMORSE_FOUND TRUE CACHE INTERNAL "GGMorse found")
        message(STATUS "Found GGMorse: ${GGMORSE_INCLUDE_DIR}, ${GGMORSE_LIBRARIES}")
    else (GGMORSE_INCLUDE_DIR AND GGMORSE_LIBRARIES)
        set(GGMORSE_FOUND FALSE CACHE INTERNAL "GGMorse found")
        message(STATUS "GGMorse not found")
    endif (GGMORSE_INCLUDE_DIR AND GGMORSE_LIBRARIES)

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(GGMORSE DEFAULT_MSG GGMORSE_LIBRARIES GGMORSE_INCLUDE_DIR)
    MARK_AS_ADVANCED(GGMORSE_LIBRARIES GGMORSE_INCLUDE_DIR)
endif (NOT GGMORSE_FOUND)
