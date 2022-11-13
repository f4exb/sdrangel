IF(NOT SGP4_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(PC_SGP4 sgp4)

    FIND_PATH(
        SGP4_INCLUDE_DIR
        NAMES SGP4.h
        HINTS ${SGP4_DIR}/include/libsgp4
        PATHS /usr/local/include/libsgp4
              /usr/include/libsgp4
    )

    FIND_LIBRARY(
        SGP4_LIBRARIES
        NAMES sgp4s
        HINTS ${SGP4_DIR}/lib
        PATHS /usr/local/lib
              /usr/lib
              /usr/lib64
    )

    message(STATUS "SGP4 LIBRARIES " ${SGP4_LIBRARIES})
    message(STATUS "SGP4 INCLUDE DIRS " ${SGP4_INCLUDE_DIR})

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(SGP4 DEFAULT_MSG SGP4_LIBRARIES SGP4_INCLUDE_DIR)
    MARK_AS_ADVANCED(SGP4_LIBRARIES SGP4_INCLUDE_DIR)

ENDIF(NOT SGP4_FOUND)
