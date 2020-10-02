IF(NOT UHD_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(PC_UHD uhd)
    
    FIND_PATH(
        UHD_INCLUDE_DIRS
        NAMES uhd.h
        HINTS $ENV{UHD_DIR}/include
        PATHS /usr/local/include
              /usr/include
    )
    
    FIND_LIBRARY(
        UHD_LIBRARIES
        NAMES uhd
        HINTS $ENV{UHD_DIR}/lib
        PATHS /usr/local/lib
              /usr/lib
              /usr/lib64
    )
    
    message(STATUS "UHD LIBRARIES " ${UHD_LIBRARIES})
    message(STATUS "UHD INCLUDE DIRS " ${UHD_INCLUDE_DIRS})
    
    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(UHD DEFAULT_MSG UHD_LIBRARIES UHD_INCLUDE_DIRS)
    MARK_AS_ADVANCED(UHD_LIBRARIES UHD_INCLUDE_DIRS)

ENDIF(NOT UHD_FOUND)
