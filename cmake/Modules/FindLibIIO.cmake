# Finds libiio for PlutoSDR support.
#
# The libiio 0.x API is used by default. Set ENABLE_LIBIIO_V1=ON to build
# against libiio 1.x instead. The header and the library always come from the
# same install: IIO_DIR if set, else the one pkg-config reports, else the
# system paths. An install with the other API is skipped.
#
# Sets LIBIIO_FOUND, LIBIIO_INCLUDE_DIR, LIBIIO_LIBRARIES and LIBIIO_V1.

if(ENABLE_LIBIIO_V1)
  set(_LIBIIO_WANT_V1 TRUE)
else()
  set(_LIBIIO_WANT_V1 FALSE)
endif()

# Detect again when ENABLE_LIBIIO_V1 changes in an existing build directory
if(LIBIIO_FOUND AND NOT LIBIIO_EXTERNAL AND NOT (LIBIIO_V1 STREQUAL _LIBIIO_WANT_V1))
  unset(LIBIIO_FOUND CACHE)
  unset(LIBIIO_INCLUDE_DIR CACHE)
  unset(LIBIIO_LIBRARIES CACHE)
endif()

# Checks one install for the requested API: header and library must both be found there.
# Sets _LIBIIO_INSTALL_FOUND in the caller's scope.
function(libiio_check_install label include_dirs lib_dirs)
  unset(_LIBIIO_INC_V1 CACHE)
  unset(_LIBIIO_INC_V0 CACHE)
  unset(_LIBIIO_LIB CACHE)
  find_path(_LIBIIO_INC_V1 NAMES iio/iio.h PATHS ${include_dirs} NO_DEFAULT_PATH)
  find_path(_LIBIIO_INC_V0 NAMES iio.h PATHS ${include_dirs} NO_DEFAULT_PATH)
  find_library(_LIBIIO_LIB NAMES iio PATHS ${lib_dirs}
    PATH_SUFFIXES ${CMAKE_LIBRARY_ARCHITECTURE} NO_DEFAULT_PATH)

  if(_LIBIIO_WANT_V1)
    set(include_dir ${_LIBIIO_INC_V1})
    set(other_include_dir ${_LIBIIO_INC_V0})
    set(other_api "0.x")
    set(option_state "ON")
  else()
    set(include_dir ${_LIBIIO_INC_V0})
    set(other_include_dir ${_LIBIIO_INC_V1})
    set(other_api "1.x")
    set(option_state "OFF")
  endif()

  set(found FALSE)

  if(include_dir AND _LIBIIO_LIB)
    set(LIBIIO_INCLUDE_DIR ${include_dir} CACHE PATH "libiio include directory" FORCE)
    set(LIBIIO_LIBRARIES ${_LIBIIO_LIB} CACHE FILEPATH "libiio library" FORCE)
    set(found TRUE)
  elseif(other_include_dir AND _LIBIIO_LIB)
    message(STATUS "Ignoring libiio ${other_api} in ${label}: ENABLE_LIBIIO_V1 is ${option_state}")
  endif()

  unset(_LIBIIO_INC_V1 CACHE)
  unset(_LIBIIO_INC_V0 CACHE)
  unset(_LIBIIO_LIB CACHE)
  set(_LIBIIO_INSTALL_FOUND ${found} PARENT_SCOPE)
endfunction()

if(NOT LIBIIO_FOUND)

  pkg_check_modules (LIBIIO_PKG libiio>=0.7)
  set(LIBIIO_V1 FALSE CACHE INTERNAL "libiio 1.x API")

  if(LIBIIO_PKG_FOUND OR (DEFINED IIO_DIR))
    set(_LIBIIO_INSTALL_FOUND FALSE)

    if(DEFINED IIO_DIR)
      libiio_check_install("IIO_DIR (${IIO_DIR})"
        "${IIO_DIR}/include"
        "${IIO_DIR}/lib;${IIO_DIR}/lib64")
    endif()

    if(NOT _LIBIIO_INSTALL_FOUND AND LIBIIO_PKG_FOUND)
      libiio_check_install("pkg-config (libiio ${LIBIIO_PKG_VERSION})"
        "${LIBIIO_PKG_INCLUDEDIR};${LIBIIO_PKG_INCLUDE_DIRS}"
        "${LIBIIO_PKG_LIBDIR};${LIBIIO_PKG_LIBRARY_DIRS}")
    endif()

    if(NOT _LIBIIO_INSTALL_FOUND)
      libiio_check_install("system paths"
        "/usr/include;/usr/local/include"
        "/usr/lib;/usr/local/lib")
    endif()

    if(_LIBIIO_INSTALL_FOUND)
      set(LIBIIO_FOUND TRUE CACHE INTERNAL "libiio found")
      set(LIBIIO_V1 ${_LIBIIO_WANT_V1} CACHE INTERNAL "libiio 1.x API")
      if(LIBIIO_V1)
        message(STATUS "Found libiio (1.x API): ${LIBIIO_INCLUDE_DIR}, ${LIBIIO_LIBRARIES}")
      else()
        message(STATUS "Found libiio (0.x API): ${LIBIIO_INCLUDE_DIR}, ${LIBIIO_LIBRARIES}")
      endif()
    else()
      set(LIBIIO_FOUND FALSE CACHE INTERNAL "libiio found")
      message(STATUS "libiio not found.")
    endif()

    mark_as_advanced(LIBIIO_INCLUDE_DIR LIBIIO_LIBRARIES)

  endif(LIBIIO_PKG_FOUND OR (DEFINED IIO_DIR))

endif(NOT LIBIIO_FOUND)
