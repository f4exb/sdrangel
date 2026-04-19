IF(NOT CSPICE_FOUND)
    FIND_PATH(
        CSPICE_INCLUDE_DIR
        NAMES SpiceUsr.h
        HINTS ${CSPICE_DIR}/include
        PATHS /usr/local/include
              /usr/include
    )

    FIND_LIBRARY(
        CSPICE_LIBRARIES
        NAMES cspice
        HINTS ${CSPICE_DIR}/lib
        PATHS /usr/local/lib
              /usr/lib
              /usr/lib64
    )

    message(STATUS "CSPICE LIBRARIES " ${CSPICE_LIBRARIES})
    message(STATUS "CSPICE INCLUDE DIRS " ${CSPICE_INCLUDE_DIR})

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(CSPICE DEFAULT_MSG CSPICE_LIBRARIES CSPICE_INCLUDE_DIR)
    MARK_AS_ADVANCED(CSPICE_LIBRARIES CSPICE_INCLUDE_DIR)

ENDIF(NOT CSPICE_FOUND)
