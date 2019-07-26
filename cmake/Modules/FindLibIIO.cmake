if(NOT LIBIIO_FOUND)

  pkg_check_modules (LIBIIO_PKG libiio>=0.7)

  if(LIBIIO_PKG_FOUND OR (DEFINED IIO_DIR))
    find_path(LIBIIO_INCLUDE_DIR
      NAMES iio.h
      HINTS ${IIO_DIR}/include
            ${LIBIIO_PKG_INCLUDE_DIRS}
      PATHS /usr/include
             /usr/local/include
      )

    find_library(LIBIIO_LIBRARIES
      NAMES iio
      HINTS ${IIO_DIR}/lib
            ${LIBIIO_PKG_LIBRARY_DIRS}
      PATHS /usr/lib
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

  endif(LIBIIO_PKG_FOUND OR (DEFINED IIO_DIR))

endif(NOT LIBIIO_FOUND)
