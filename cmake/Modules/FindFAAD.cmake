IF(NOT FAAD_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(PC_FAAD faad2)

    FIND_PATH(
        FAAD_INCLUDE_DIR
        NAMES faad.h
        HINTS ${FAAD_DIR}/include
              ${PC_FAAD_INCLUDE_DIRS}
        PATHS /usr/local/include
              /usr/include
    )

    FIND_LIBRARY(
        FAAD_LIBRARY
        NAMES faad
        HINTS ${FAAD_DIR}/lib
              ${PC_FAAD_LIBRARY_DIRS}
        PATHS /usr/local/lib
              /usr/lib
              /usr/lib64
    )

    message(STATUS "FAAD LIBRARY " ${FAAD_LIBRARY})
    message(STATUS "FAAD INCLUDE DIRS " ${FAAD_INCLUDE_DIR})

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(FAAD DEFAULT_MSG FAAD_LIBRARY FAAD_INCLUDE_DIR)
    MARK_AS_ADVANCED(FAAD_LIBRARY FAAD_INCLUDE_DIR)

ENDIF(NOT FAAD_FOUND)
