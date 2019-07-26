if(NOT LIBHACKRF_FOUND)

  pkg_check_modules (LIBHACKRF_PKG libhackrf)

  find_path(LIBHACKRF_INCLUDE_DIR
    NAMES libhackrf/hackrf.h
    HINTS ${HACKRF_DIR}/include
          ${LIBHACKRF_PKG_INCLUDE_DIRS}
    PATHS /usr/include
          /usr/local/include
  )

  find_library(LIBHACKRF_LIBRARIES
    NAMES hackrf
    HINTS ${HACKRF_DIR}/lib
          ${LIBHACKRF_PKG_LIBRARY_DIRS}
    PATHS /usr/lib
          /usr/local/lib
  )

  if(LIBHACKRF_INCLUDE_DIR AND LIBHACKRF_LIBRARIES)
    set(LIBHACKRF_FOUND TRUE CACHE INTERNAL "libhackrf found")
    message(STATUS "Found libhackrf: ${LIBHACKRF_INCLUDE_DIR}, ${LIBHACKRF_LIBRARIES}")
  else(LIBHACKRF_INCLUDE_DIR AND LIBHACKRF_LIBRARIES)
    set(LIBHACKRF_FOUND FALSE CACHE INTERNAL "libhackrf found")
    message(STATUS "libhackrf not found.")
  endif(LIBHACKRF_INCLUDE_DIR AND LIBHACKRF_LIBRARIES)

  mark_as_advanced(LIBHACKRF_INCLUDE_DIR LIBHACKRF_LIBRARIES)

endif(NOT LIBHACKRF_FOUND)
