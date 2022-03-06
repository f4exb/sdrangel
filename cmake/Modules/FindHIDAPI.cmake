# From https://github.com/rpavlik/cmake-modules/blob/main/FindHIDAPI.cmake

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
#
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
# Copyright 2009-2010, Iowa State University
# Copyright 2019, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

set(HIDAPI_ROOT_DIR
    "${HIDAPI_ROOT_DIR}"
    CACHE PATH "Root to search for HIDAPI")

option(hidapi_USE_STATIC_LIBS "hidapi: link static libs" OFF)

# Clean up components
if(HIDAPI_FIND_COMPONENTS)
    if(WIN32 OR APPLE)
        # This makes no sense on Windows or Mac, which have native APIs
        list(REMOVE HIDAPI_FIND_COMPONENTS libusb)
    endif()

    if(NOT ${CMAKE_SYSTEM} MATCHES "Linux")
        # hidraw is only on linux
        list(REMOVE HIDAPI_FIND_COMPONENTS hidraw)
    endif()
endif()
if(NOT HIDAPI_FIND_COMPONENTS)
    # Default to any
    set(HIDAPI_FIND_COMPONENTS any)
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

set(HIDAPI_NAME hidapi)
set(HIDAPI_USB_NAME hidapi-libusb)
set(HIDAPI_RAW_NAME hidapi-hidraw)
if ((UNIX OR APPLE) AND hidapi_USE_STATIC_LIBS)
  set(HIDAPI_NAME libhidapi.a)
  set(HIDAPI_USB_NAME libhidapi-libusb.a)
  set(HIDAPI_RAW_NAME libhidapi-hidraw.a)
endif()

# Actually search
find_library(
    HIDAPI_UNDECORATED_LIBRARY
    NAMES ${HIDAPI_NAME}
    PATHS "${HIDAPI_ROOT_DIR}"
    PATH_SUFFIXES lib)

find_library(
    HIDAPI_LIBUSB_LIBRARY
    NAMES ${HIDAPI_NAME} ${HIDAPI_USB_NAME}
    PATHS "${HIDAPI_ROOT_DIR}"
    PATH_SUFFIXES lib
    HINTS ${PC_HIDAPI_LIBUSB_LIBRARY_DIRS})

if(CMAKE_SYSTEM MATCHES "Linux")
    find_library(
        HIDAPI_HIDRAW_LIBRARY
        NAMES ${HIDAPI_RAW_NAME}
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

# If we have static linking and are under Linux, we need to link libusb for
# the USB backend, and udev for both backends.
if (hidapi_USE_STATIC_LIBS AND (UNIX AND NOT APPLE))
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LIBUDEV libudev)
  endif()
  find_library(UDEV_LIBRARY
    NAMES
    libudev.a
    udev
    PATHS
    ${PC_LIBUDEV_LIBRARY_DIRS}
    ${PC_LIBUDEV_LIBDIR}
    HINTS
    "${UDEV_ROOT_DIR}"
    )
  add_library(hidapi_udev STATIC IMPORTED)
  set_target_properties(
    hidapi_udev
    PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${UDEV_LIBRARY}")
  list(APPEND _hidapi_component_required_vars UDEV_LIBRARY)
  set(HIDAPI_EXTRA_LINK_LIBS hidapi_udev)
  if(HIDAPI_LIBRARY STREQUAL ${HIDAPI_LIBUSB_LIBRARY})
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_LIBUSB1 libusb-1.0)
    endif()
    find_library(LIBUSB1_LIBRARY
      NAMES
      libusb-1.0.a
      PATHS
      ${PC_LIBUSB1_LIBRARY_DIRS}
      ${PC_LIBUSB1_LIBDIR}
      HINTS
      "${LIBUSB1_ROOT_DIR}")
    add_library(hidapi_libusb STATIC IMPORTED)
    set_target_properties(
      hidapi_libusb
      PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LINK_INTERFACE_LIBRARIES hidapi_udev
      IMPORTED_LOCATION "${LIBUSB1_LIBRARY}")
    list(APPEND _hidapi_usb_required_vars LIBUSB1_LIBRARY)
    list(APPEND HIDAPI_USB_LINK_LIBS hidapi_libusb)
    list(APPEND HIDAPI_EXTRA_LINK_LIBS ${HIDAPI_USB_LINK_LIBS})
  endif()
endif()

# If we have static linking and are under Windows, we need to link with
# setupapi.
if (hidapi_USE_STATIC_LIBS AND WIN32)
  list(APPEND HIDAPI_EXTRA_LINK_LIBS Setupapi)
endif()

# If we have static linking and are under OSX, we need to link IOKit,
# CoreFoundation and AppKit.
if (hidapi_USE_STATIC_LIBS AND APPLE)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(APPKIT_LIBRARY AppKit)
  list(APPEND _hidapi_component_required_vars IOKIT_LIBRARY COREFOUNDATION_LIBRARY APPKIT_LIBRARY)
  list(APPEND HIDAPI_EXTRA_LINK_LIBS ${IOKIT_LIBRARY} ${COREFOUNDATION_LIBRARY} ${APPKIT_LIBRARY})
endif()

###
# FPHSA call
###
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    HIDAPI
    REQUIRED_VARS ${_hidapi_component_required_vars} THREADS_FOUND
    HANDLE_COMPONENTS)
if(HIDAPI_FOUND)
    set(HIDAPI_LIBRARIES "${HIDAPI_LIBRARY}")
    set(HIDAPI_INCLUDE_DIRS "${HIDAPI_INCLUDE_DIR}")
    if(NOT TARGET HIDAPI::hidapi)
        add_library(HIDAPI::hidapi UNKNOWN IMPORTED)
        set_target_properties(
            HIDAPI::hidapi
            PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                       IMPORTED_LOCATION "${HIDAPI_LIBRARY}"
                       INTERFACE_INCLUDE_DIRECTORIES "${HIDAPI_INCLUDE_DIR}"
                       IMPORTED_LINK_INTERFACE_LIBRARIES "Threads::Threads;${HIDAPI_EXTRA_LINK_LIBS}")
    endif()
endif()

if(HIDAPI_libusb_FOUND AND NOT TARGET HIDAPI::hidapi-libusb)
    add_library(HIDAPI::hidapi-libusb UNKNOWN IMPORTED)
    set_target_properties(
        HIDAPI::hidapi-libusb
        PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                   IMPORTED_LOCATION "${HIDAPI_LIBUSB_LIBRARY}"
                   INTERFACE_INCLUDE_DIRECTORIES "${HIDAPI_INCLUDE_DIR}"
                   IMPORTED_LINK_INTERFACE_LIBRARIES "Threads::Threads;${HIDAPI_EXTRA_LINK_LIBS};${HIDAPI_USB_LINK_LIBS}")
endif()

if(HIDAPI_hidraw_FOUND AND NOT TARGET HIDAPI::hidapi-hidraw)
    add_library(HIDAPI::hidapi-hidraw UNKNOWN IMPORTED)
    set_target_properties(
        HIDAPI::hidapi-hidraw
        PROPERTIES IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                   IMPORTED_LOCATION "${HIDAPI_HIDRAW_LIBRARY}"
                   INTERFACE_INCLUDE_DIRECTORIES "${HIDAPI_INCLUDE_DIR}"
                   IMPORTED_LINK_INTERFACE_LIBRARIES "Threads::Threads;${HIDAPI_EXTRA_LINK_LIBS}")
endif()
