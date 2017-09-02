if(NOT LIBIIO_FOUND)

  pkg_check_modules (LIBIIO_PKG libiio)
  find_path(LIBIIO_INCLUDE_DIR NAMES iio.h
    PATHS
    ${LIBIIO_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBIIO_LIBRARIES NAMES iio
    PATHS
    ${LIBIIO_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
  )

  if(LIBIIO_INCLUDE_DIR AND LIBIIO_LIBRARIES)
    set(LIBIIO_FOUND TRUE CACHE INTERNAL "libiio found")
    message(STATUS "Found libiio: ${LIBIIO_INCLUDE_DIR}, ${LIBIIO_LIBRARIES}")
  else(LIBIIO_INCLUDE_DIR AND LIBIIO_LIBRARIES)
    set(LIBIIO_FOUND FALSE CACHE INTERNAL "libiio found")
    message(STATUS "libiio not found.")
  endif(LIBIIO_INCLUDE_DIR AND LIBIIO_LIBRARIES)

  mark_as_advanced(LIBIIO_INCLUDE_DIR LIBIIO_LIBRARIES)

endif(NOT LIBIIO_FOUND)
