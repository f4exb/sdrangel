if(NOT LIBXTRX_FOUND)
  pkg_check_modules (LIBXTRX_PKG libxtrx)
  find_path(LIBXTRX_INCLUDE_DIRS NAMES xtrx_api.h
    PATHS
    ${LIBXTRX_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBXTRX_LIBRARIES NAMES xtrx
    PATHS
    ${LIBXTRX_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
  )

if(LIBXTRX_INCLUDE_DIRS AND LIBXTRX_LIBRARIES)
  set(LIBXTRX_FOUND TRUE CACHE INTERNAL "libxtrx found")
  message(STATUS "Found libxtrx: ${LIBXTRX_INCLUDE_DIRS}, ${LIBXTRX_LIBRARIES}")
else(LIBXTRX_INCLUDE_DIRS AND LIBXTRX_LIBRARIES)
  set(LIBXTRX_FOUND FALSE CACHE INTERNAL "libxtrx found")
  message(STATUS "libxtrx not found.")
endif(LIBXTRX_INCLUDE_DIRS AND LIBXTRX_LIBRARIES)

mark_as_advanced(LIBXTRX_LIBRARIES LIBXTRX_INCLUDE_DIRS)

endif(NOT LIBXTRX_FOUND)