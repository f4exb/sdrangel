if(NOT LIBAIRSPYHF_FOUND)

  pkg_check_modules (LIBAIRSPYHF_PKG libairspyhf)

  find_path(LIBAIRSPYHF_INCLUDE_DIR
    NAMES libairspyhf/airspyhf.h
    HINTS ${AIRSPYHF_DIR}/include
          ${LIBAIRSPYHF_PKG_INCLUDE_DIRS}
    PATHS /usr/include
          /usr/local/include
  )

  find_library(LIBAIRSPYHF_LIBRARIES
    NAMES airspyhf
    HINTS ${AIRSPYHF_DIR}/lib
          ${LIBAIRSPYHF_PKG_LIBRARY_DIRS}
    PATHS /usr/lib
          /usr/local/lib
  )

  if(LIBAIRSPYHF_INCLUDE_DIR AND LIBAIRSPYHF_LIBRARIES)
    set(LIBAIRSPYHF_FOUND TRUE CACHE INTERNAL "libairspyhf found")
    message(STATUS "Found libairspyhf: ${LIBAIRSPYHF_INCLUDE_DIR}, ${LIBAIRSPYHF_LIBRARIES}")
  else(LIBAIRSPYHF_INCLUDE_DIR AND LIBAIRSPYHF_LIBRARIES)
    set(LIBAIRSPYHF_FOUND FALSE CACHE INTERNAL "libairspyhf found")
    message(STATUS "libairspyhf not found.")
  endif(LIBAIRSPYHF_INCLUDE_DIR AND LIBAIRSPYHF_LIBRARIES)

  mark_as_advanced(LIBAIRSPYHF_INCLUDE_DIR LIBAIRSPYHF_LIBRARIES)

endif(NOT LIBAIRSPYHF_FOUND)
