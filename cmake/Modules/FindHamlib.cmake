# - Try to find hamlib
# Once done, this will define:
#
#  hamlib_FOUND - system has Hamlib-2
#  hamlib_INCLUDE_DIRS - the Hamlib-2 include directories
#  hamlib_LIBRARIES - link these to use Hamlib-2

if (NOT hamlib_FOUND)

    include (LibFindMacros)

    # Use pkg-config to get hints about paths
    libfind_pkg_check_modules (hamlib_PKGCONF hamlib)

    FIND_PATH(hamlib_INCLUDE_DIRS
        NAMES hamlib/rig.h
        HINTS ${HAMLIB_DIR}/include
              ${hamlib_PKGCONF_INCLUDE_DIRS}
              ${CMAKE_INSTALL_PREFIX}/include
        PATHS /usr/local/include
              /usr/include
    )

    FIND_LIBRARY(hamlib_LIBRARIES
        NAMES hamlib
        HINTS ${HAMLIB_DIR}/lib
              ${HAMLIB_DIR}/lib64
              ${hamlib_PKGCONF_LIBRARY_DIRS}
              ${CMAKE_INSTALL_PREFIX}/lib
              ${CMAKE_INSTALL_PREFIX}/lib64
        PATHS /usr/local/lib
              /usr/local/lib64
              /usr/lib
              /usr/lib64
    )

    if(hamlib_INCLUDE_DIRS AND hamlib_LIBRARIES)
        set(hamlib_FOUND TRUE CACHE INTERNAL "Hamlib found")
        message(STATUS "Found Hamlib: ${hamlib_INCLUDE_DIRS}, ${hamlib_LIBRARIES}")
    else()
        set(hamlib_FOUND FALSE CACHE INTERNAL "Hamlib found")
        message(STATUS "Hamlib not found")
    endif()

    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(HAMLIB DEFAULT_MSG hamlib_LIBRARIES hamlib_INCLUDE_DIRS)
    MARK_AS_ADVANCED(hamlib_LIBRARIES hamlib_INCLUDE_DIRS)

endif (NOT hamlib_FOUND)
