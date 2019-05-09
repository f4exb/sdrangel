if(NOT LIBIIO_FOUND)

  pkg_check_modules (LIBIIO_PKG libiio>=0.7)

  if(LIBIIO_PKG_FOUND)
    find_path(LIBIIO_INCLUDE_DIR
      NAMES iio.h
      PATHS ${IIO_DIR}/include
      ${LIBIIO_PKG_INCLUDE_DIRS}
      /usr/include
      /usr/local/include
      )

    find_library(LIBIIO_LIBRARIES
      NAMES iio
      PATHS ${IIO_DIR}/lib
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

endif(LIBIIO_PKG_FOUND)

endif(NOT LIBIIO_FOUND)
