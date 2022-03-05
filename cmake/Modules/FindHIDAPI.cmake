#.rst:
# FindHIDAPI
# ----------
#
# Try to find HIDAPI library, from http://www.signal11.us/oss/hidapi/
#
# Cache Variables: (probably not for direct use in your scripts)
#  HIDAPI_INCLUDE_DIR
#  HIDAPI_LIBRARY
#
# Non-cache variables you might use in your CMakeLists.txt:
#  HIDAPI_FOUND
#  HIDAPI_INCLUDE_DIRS
#  HIDAPI_LIBRARIES
#
# COMPONENTS
# ^^^^^^^^^^
#
# This module respects several COMPONENTS specifying the backend you prefer:
# ``any`` (the default), ``libusb``, and ``hidraw``.
# The availablility of the latter two depends on your platform.
#
#
# IMPORTED Targets
# ^^^^^^^^^^^^^^^^

# This module defines :prop_tgt:`IMPORTED` target ``HIDAPI::hidapi`` (in all cases or
# if no components specified), ``HIDAPI::hidapi-libusb`` (if you requested the libusb component),
# and ``HIDAPI::hidapi-hidraw`` (if you requested the hidraw component),
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# ``HIDAPI_FOUND``
#   True if HIDAPI or the requested components (if any) were found.
#
# We recommend using the imported targets instead of the following.
#
# ``HIDAPI_INCLUDE_DIRS``
# ``HIDAPI_LIBRARIES``
#
# Original Author:
# 2009-2010, 2019 Ryan Pavlik <ryan.pavlik@collabora.com> <abiryan@ryand.net>
# http://academic.cleardefinition.com
#
# Copyright Iowa State University 2009-2010.
# Copyright Collabora, Ltd. 2019.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_policy(SET CMP0045 NEW)
cmake_policy(SET CMP0053 NEW)
cmake_policy(SET CMP0054 NEW)

set(HIDAPI_ROOT_DIR
    "${HIDAPI_ROOT_DIR}"
    CACHE PATH "Root to search for HIDAPI")

# Clean up components
if("${HIDAPI_FIND_COMPONENTS}")
    if(WIN32 OR APPLE)
        # This makes no sense on Windows or Mac, which have native APIs
        list(REMOVE "${HIDAPI_FIND_COMPONENTS}" libusb)
    endif()

    if(NOT ${CMAKE_SYSTEM} MATCHES "Linux")
        # hidraw is only on linux
        list(REMOVE "${HIDAPI_FIND_COMPONENTS}" hidraw)
    endif()
endif()
if(NOT "${HIDAPI_FIND_COMPONENTS}")
    # Default to any
    set("${HIDAPI_FIND_COMPONENTS}" any)
endif()

# Ask pkg-config for hints
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    set(_old_prefix_path "${CMAKE_PREFIX_PATH}")
    # So pkg-config uses HIDAPI_ROOT_DIR too.
    if(HIDAPI_ROOT_DIR)
        list(APPEND CMAKE_PREFIX_PATH ${HIDAPI_ROOT_DIR})
    endif()
    pkg_check_modules(PC_HIDAPI_LIBUSB QUIET hidapi-libusb)
    pkg_check_modules(PC_HIDAPI_HIDRAW QUIET hidapi-hidraw)
    # Restore
    set(CMAKE_PREFIX_PATH "${_old_prefix_path}")
endif()

# Actually search
find_library(
    HIDAPI_UNDECORATED_LIBRARY
    NAMES hidapi
    PATHS "${HIDAPI_ROOT_DIR}"
    PATH_SUFFIXES lib)

find_library(
    HIDAPI_LIBUSB_LIBRARY
    NAMES hidapi hidapi-libusb
    PATHS "${HIDAPI_ROOT_DIR}"
    PATH_SUFFIXES lib
    HINTS ${PC_HIDAPI_LIBUSB_LIBRARY_DIRS})

if(CMAKE_SYSTEM MATCHES "Linux")
    find_library(
        HIDAPI_HIDRAW_LIBRARY
        NAMES hidapi-hidraw
        HINTS ${PC_HIDAPI_HIDRAW_LIBRARY_DIRS})
endif()

find_path(
    HIDAPI_INCLUDE_DIR
    NAMES hidapi.h
    PATHS "${HIDAPI_ROOT_DIR}"
    PATH_SUFFIXES hidapi include include/hidapi
    HINTS ${PC_HIDAPI_HIDRAW_INCLUDE_DIRS} ${PC_HIDAPI_LIBUSB_INCLUDE_DIRS})

find_package(Threads QUIET)

###
# Compute the "I don't care which backend" library
###
set(HIDAPI_LIBRARY)

# First, try to use a preferred backend if supplied
if("${HIDAPI_FIND_COMPONENTS}" MATCHES "libusb"
   AND HIDAPI_LIBUSB_LIBRARY
   AND NOT HIDAPI_LIBRARY)
    set(HIDAPI_LIBRARY ${HIDAPI_LIBUSB_LIBRARY})
endif()
if("${HIDAPI_FIND_COMPONENTS}" MATCHES "hidraw"
   AND HIDAPI_HIDRAW_LIBRARY
   AND NOT HIDAPI_LIBRARY)
    set(HIDAPI_LIBRARY ${HIDAPI_HIDRAW_LIBRARY})
endif()

# Then, if we don't have a preferred one, settle for anything.
if(NOT HIDAPI_LIBRARY)
    if(HIDAPI_LIBUSB_LIBRARY)
        set(HIDAPI_LIBRARY ${HIDAPI_LIBUSB_LIBRARY})
    elseif(HIDAPI_HIDRAW_LIBRARY)
        set(HIDAPI_LIBRARY ${HIDAPI_HIDRAW_LIBRARY})
    elseif(HIDAPI_UNDECORATED_LIBRARY)
        set(HIDAPI_LIBRARY ${HIDAPI_UNDECORATED_LIBRARY})
    endif()
endif()

###
# Determine if the various requested components are found.
###
set(_hidapi_component_required_vars)

foreach(_comp IN LISTS HIDAPI_FIND_COMPONENTS)
    if("${_comp}" STREQUAL "any")
        list(APPEND _hidapi_component_required_vars HIDAPI_INCLUDE_DIR
                    HIDAPI_LIBRARY)
        if(HIDAPI_INCLUDE_DIR AND EXISTS "${HIDAPI_LIBRARY}")
            set(HIDAPI_any_FOUND TRUE)
            mark_as_advanced(HIDAPI_INCLUDE_DIR)
        else()
            set(HIDAPI_any_FOUND FALSE)
        endif()

    elseif("${_comp}" STREQUAL "libusb")
        list(APPEND _hidapi_component_required_vars HIDAPI_INCLUDE_DIR
                    HIDAPI_LIBUSB_LIBRARY)
        if(HIDAPI_INCLUDE_DIR AND EXISTS "${HIDAPI_LIBUSB_LIBRARY}")
            set(HIDAPI_libusb_FOUND TRUE)
            mark_as_advanced(HIDAPI_INCLUDE_DIR HIDAPI_LIBUSB_LIBRARY)
        else()
            set(HIDAPI_libusb_FOUND FALSE)
        endif()

    elseif("${_comp}" STREQUAL "hidraw")
        list(APPEND _hidapi_component_required_vars HIDAPI_INCLUDE_DIR
                    HIDAPI_HIDRAW_LIBRARY)
        if(HIDAPI_INCLUDE_DIR AND EXISTS "${HIDAPI_HIDRAW_LIBRARY}")
            set(HIDAPI_hidraw_FOUND TRUE)
            mark_as_advanced(HIDAPI_INCLUDE_DIR HIDAPI_HIDRAW_LIBRARY)
        else()
            set(HIDAPI_hidraw_FOUND FALSE)
        endif()

    else()
        message(WARNING "${_comp} is not a recognized HIDAPI component")
        set(HIDAPI_${_comp}_FOUND FALSE)
    endif()
endforeach()
unset(_comp)

###
# FPHSA call
###
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    HIDAPI REQUIRED_VARS ${_hidapi_component_required_vars} THREADS_FOUND
    HANDLE_COMPONENTS)

if(HIDAPI_FOUND)
    set(HIDAPI_LIBRARIES "${HIDAPI_LIBRARY}")
    set(HIDAPI_INCLUDE_DIRS "${HIDAPI_INCLUDE_DIR}")
    if(NOT TARGET HIDAPI::hidapi)
        add_library(HIDAPI::hidapi UNKNOWN IMPORTED)
        set_target_properties(
            HIDAPI::hidapi
	    PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION ${HIDAPI_LIBRARY})
        set_property(
            TARGET HIDAPI::hidapi PROPERTY IMPORTED_LINK_INTERFACE_LIBRARIES
                                           Threads::Threads)
    endif()
endif()

if(HIDAPI_libusb_FOUND AND NOT TARGET HIDAPI::hidapi-libusb)
    add_library(HIDAPI::hidapi-libusb UNKNOWN IMPORTED)
    set_target_properties(
        HIDAPI::hidapi-libusb
        PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C" IMPORTED_LOCATION
                   ${HIDAPI_LIBUSB_LIBRARY})
    set_property(TARGET HIDAPI::hidapi-libusb
                 PROPERTY IMPORTED_LINK_INTERFACE_LIBRARIES Threads::Threads)
endif()

if(HIDAPI_hidraw_FOUND AND NOT TARGET HIDAPI::hidapi-hidraw)
    add_library(HIDAPI::hidapi-hidraw UNKNOWN IMPORTED)
    set_target_properties(
        HIDAPI::hidapi-hidraw
        PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C" IMPORTED_LOCATION
                   ${HIDAPI_HIDRAW_LIBRARY})
    set_property(TARGET HIDAPI::hidapi-hidraw
                 PROPERTY IMPORTED_LINK_INTERFACE_LIBRARIES Threads::Threads)
endif()
