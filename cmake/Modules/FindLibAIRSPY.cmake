if(NOT LIBAIRSPY_FOUND)

  pkg_check_modules (LIBAIRSPY_PKG libairspy)
  find_path(LIBAIRSPY_INCLUDE_DIR NAMES libairspy/airspy.h
    PATHS
    ${LIBAIRSPY_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBAIRSPY_LIBRARIES NAMES airspy
    PATHS
    ${LIBAIRSPY_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
  )

  if(LIBAIRSPY_INCLUDE_DIR AND LIBAIRSPY_LIBRARIES)
    set(LIBAIRSPY_FOUND TRUE CACHE INTERNAL "libairspy found")
    message(STATUS "Found libairspy: ${LIBAIRSPY_INCLUDE_DIR}, ${LIBAIRSPY_LIBRARIES}")
  else(LIBAIRSPY_INCLUDE_DIR AND LIBAIRSPY_LIBRARIES)
    set(LIBAIRSPY_FOUND FALSE CACHE INTERNAL "libairspy found")
    message(STATUS "libairspy not found.")
  endif(LIBAIRSPY_INCLUDE_DIR AND LIBAIRSPY_LIBRARIES)

  mark_as_advanced(LIBAIRSPY_INCLUDE_DIR LIBAIRSPY_LIBRARIES)

endif(NOT LIBAIRSPY_FOUND)
