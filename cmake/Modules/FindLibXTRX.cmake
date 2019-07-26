if(NOT LIBXTRX_FOUND)

  pkg_check_modules (LIBXTRX_PKG libxtrx)

  find_path(LIBXTRX_INCLUDE_DIRS
    NAMES xtrx_api.h
    HINTS ${XTRX_DIR}/include
          ${LIBXTRX_PKG_INCLUDE_DIRS}
    PATHS /usr/include
          /usr/local/include
  )

  find_library(LIBXTRX_LIBRARY
    NAMES xtrx
    HINTS ${XTRX_DIR}/lib
          ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
    PATHS /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
  )

  if(LIBXTRX_INCLUDE_DIRS AND LIBXTRX_LIBRARY)
	set(LIBXTRX_FOUND TRUE CACHE INTERNAL "libxtrx found")
	message(STATUS "Found libxtrx: ${LIBXTRX_INCLUDE_DIRS}, ${LIBXTRX_LIBRARY}")
  else()
    set(LIBXTRX_FOUND FALSE CACHE INTERNAL "libxtrx found")
	message(STATUS "libxtrx not found.")
  endif()

  mark_as_advanced(LIBXTRX_INCLUDE_DIRS LIBXTRX_LIBRARY)

endif(NOT LIBXTRX_FOUND)