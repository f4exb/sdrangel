if(NOT LIBHACKRF_FOUND)

  pkg_check_modules (LIBHACKRF_PKG libhackrf)
  find_path(LIBHACKRF_INCLUDE_DIR NAMES libhackrf/hackrf.h
    PATHS
    ${LIBHACKRF_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBHACKRF_LIBRARIES NAMES hackrf
    PATHS
    ${LIBHACKRF_PKG_LIBRARY_DIRS}
    /usr/lib
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
