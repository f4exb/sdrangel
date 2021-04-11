IF(NOT SDRPLAY_FOUND)

    FIND_PATH(
        SDRPLAY_INCLUDE_DIR
        NAMES sdrplay_api.h
        HINTS ${SDRPLAY_DIR}/inc
        PATHS /usr/local/include
              /usr/include
    )

    FIND_LIBRARY(
        SDRPLAY_LIBRARIES
        NAMES sdrplay_api
        HINTS ${SDRPLAY_DIR}/x86_64
        PATHS /usr/local/lib
              /usr/lib
              /usr/lib64
    )

    message(STATUS "SDRPLAY LIBRARIES " ${SDRPLAY_LIBRARIES})
    message(STATUS "SDRPLAY INCLUDE DIRS " ${SDRPLAY_INCLUDE_DIR})

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDRPLAY DEFAULT_MSG SDRPLAY_LIBRARIES SDRPLAY_INCLUDE_DIR)
    MARK_AS_ADVANCED(SDRPLAY_LIBRARIES SDRPLAY_INCLUDE_DIR)

ENDIF(NOT SDRPLAY_FOUND)
