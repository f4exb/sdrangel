# Find libdsdcc

if(NOT LIBDSDCC_FOUND)

  pkg_check_modules(LIBDSDCC_PKG libdsdcc)

  find_path(LIBDSDCC_INCLUDE_DIR
    NAMES dsdcc/dsd_decoder.h
    HINTS ${DSDCC_DIR}/include
          ${LIBDSDCC_PKG_INCLUDE_DIRS}
    PATHS /usr/include/dsdcc
          /usr/local/include/dsdcc
  )

  find_library(LIBDSDCC_LIBRARIES
    NAMES dsdcc
    HINTS ${DSDCC_DIR}/lib
          ${DSDCC_DIR}/lib64
          ${LIBDSDCC_PKG_LIBRARY_DIRS}
    PATHS /usr/lib
          /usr/lib64
          /usr/local/lib
          /usr/local/lib64
  )

  if(LIBDSDCC_INCLUDE_DIR AND LIBDSDCC_LIBRARIES)
    set(LIBDSDCC_FOUND TRUE CACHE INTERNAL "libdsdcc found")
    message(STATUS "Found libdsdcc: ${LIBDSDCC_INCLUDE_DIR}, ${LIBDSDCC_LIBRARIES}")
  else(LIBDSDCC_INCLUDE_DIR AND LIBDSDCC_LIBRARIES)
    set(LIBDSDCC_FOUND FALSE CACHE INTERNAL "libdsdcc found")
    message(STATUS "libdsdcc not found.")
  endif(LIBDSDCC_INCLUDE_DIR AND LIBDSDCC_LIBRARIES)

  mark_as_advanced(LIBDSDCC_INCLUDE_DIR LIBDSDCC_LIBRARIES)

endif(NOT LIBDSDCC_FOUND)
