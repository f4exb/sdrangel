IF (NOT DAB_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(LIBDAB_PKG libdab)

    FIND_PATH(
        DAB_INCLUDE_DIR
        NAMES dab-api.h
        HINTS ${DAB_DIR}/include/dab_lib
              ${LIBDAB_PKG_INCLUDE_DIRS}
        PATHS /usr/include/dab_lib
              /usr/local/include/dab_lib
    )

    FIND_LIBRARY(
        DAB_LIBRARIES
        NAMES dab_lib
        HINTS ${DAB_DIR}/lib
              ${DAB_DIR}/lib64
              ${LIBDAB_PKG_LIBRARY_DIRS}
        PATHS /usr/lib
              /usr/lib64
              /usr/local/lib
              /usr/local/lib64
    )

    if (DAB_INCLUDE_DIR AND DAB_LIBRARIES)
        set(DAB_FOUND TRUE CACHE INTERNAL "libdab found")
        message(STATUS "Found libdab: ${DAB_INCLUDE_DIR}, ${DAB_LIBRARIES}")
    else (DAB_INCLUDE_DIR AND DAB_LIBRARIES)
        set(DAB_FOUND FALSE CACHE INTERNAL "libdab found")
        message(STATUS "libdab not found.")
    endif (DAB_INCLUDE_DIR AND DAB_LIBRARIES)

    mark_as_advanced(DAB_INCLUDE_DIR DAB_LIBRARIES)

ENDIF (NOT DAB_FOUND)
