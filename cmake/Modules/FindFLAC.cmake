IF(NOT FLAC_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(PC_FLAC flac)

    FIND_PATH(
        FLAC_INCLUDE_DIR
        NAMES FLAC/stream_encoder.h
        HINTS ${PC_FLAC_INCLUDE_DIRS}
        PATHS /usr/local/include
              /usr/include
    )

    FIND_LIBRARY(
        FLAC_LIBRARY
        NAMES FLAC
              libFLAC
        HINTS ${FLAC_DIR}/lib
              ${PC_FLAC_LIBRARY_DIRS}
        PATHS /usr/local/lib
              /usr/lib
              /usr/lib64
    )

    message(STATUS "FLAC LIBRARY " ${FLAC_LIBRARY})
    message(STATUS "FLAC INCLUDE DIR " ${FLAC_INCLUDE_DIR})

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(FLAC DEFAULT_MSG FLAC_LIBRARY FLAC_INCLUDE_DIR)
    MARK_AS_ADVANCED(FLAC_LIBRARY FLAC_INCLUDE_DIR)

ENDIF(NOT FLAC_FOUND)
