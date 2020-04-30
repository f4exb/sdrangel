if (NOT CM256CC_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(PC_CM256cc "libcm256cc")

    FIND_PATH(CM256CC_INCLUDE_DIR
        NAMES cm256cc/cm256.h
        HINTS ${CM256CC_DIR}/include
              ${PC_CM256CC_INCLUDE_DIR}
              ${CMAKE_INSTALL_PREFIX}/include
        PATHS /usr/local/include
              /usr/include
    )

    FIND_LIBRARY(CM256CC_LIBRARIES
        NAMES cm256cc libcm256cc
        HINTS ${CM256CC_DIR}/lib
              ${CM256CC_DIR}/lib64
              ${PC_CM256CC_LIBDIR}
              ${CMAKE_INSTALL_PREFIX}/lib
              ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS /usr/local/lib
              /usr/local/lib64
              /usr/lib
              /usr/lib64
    )

    if(CM256CC_INCLUDE_DIR AND CM256CC_LIBRARIES)
        set(CM256CC_FOUND TRUE CACHE INTERNAL "CM256CC found")
        message(STATUS "Found CM256cc: ${CM256CC_INCLUDE_DIR}, ${CM256CC_LIBRARIES}")
    else(CM256CC_INCLUDE_DIR AND CM256CC_LIBRARIES)
        set(CM256CC_FOUND FALSE CACHE INTERNAL "CM256CC found")
        message(STATUS "CM256cc not found")
    endif(CM256CC_INCLUDE_DIR AND CM256CC_LIBRARIES)

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(CM256CC DEFAULT_MSG CM256CC_LIBRARIES CM256CC_INCLUDE_DIR)
    MARK_AS_ADVANCED(CM256CC_LIBRARIES CM256CC_INCLUDE_DIR)
endif (NOT CM256CC_FOUND)