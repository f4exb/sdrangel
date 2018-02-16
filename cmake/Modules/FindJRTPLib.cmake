if(NOT JRTPLIB_FOUND)

INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_JRTPLIB "jrtplib")

FIND_PATH(JRTPLIB_INCLUDE_DIR
    NAMES rtpsession.h
    HINTS ${PC_JRTPLIB_INCLUDE_DIR}
    ${CMAKE_INSTALL_PREFIX}/include/jrtplib3
    ${JRTPLIB_INSTALL_PREFIX}/include/jrtplib3
    PATHS
    /usr/local/include/jrtplib3
    /usr/include/jrtplib3
)

FIND_LIBRARY(JRTPLIB_LIBRARIES
    NAMES libjrtp
    HINTS ${PC_JRTPLIB_LIBDIR}
    ${CMAKE_INSTALL_PREFIX}/lib
    ${CMAKE_INSTALL_PREFIX}/lib64
    PATHS
    ${JRTPLIB_INCLUDE_DIR}/../lib
    /usr/local/lib
    /usr/local/lib64
    /usr/lib
    /usr/lib64
)

if(JRTPLIB_INCLUDE_DIR AND JRTPLIB_LIBRARIES)
    set(JRTPLIB_FOUND TRUE CACHE INTERNAL "JRTPLib found")
    message(STATUS "Found JRTPLib: ${JRTPLIB_INCLUDE_DIR}, ${JRTPLIB_LIBRARIES}")
else()
    set(JRTPLIB_FOUND FALSE CACHE INTERNAL "JRTPLib found")
    message(STATUS "JRTPLib not found")
endif()

MARK_AS_ADVANCED(JRTPLIB_LIBRARIES JRTPLIB_INCLUDE_DIR)

endif(NOT JRTPLIB_FOUND)
