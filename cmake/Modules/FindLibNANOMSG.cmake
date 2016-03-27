if(NOT LIBNANOMSG_FOUND)

  pkg_check_modules (LIBNANOMSG_PKG libnanomsg)
  find_path(LIBNANOMSG_INCLUDE_DIR NAMES nanomsg/nn.h
    PATHS
    ${LIBNANOMSG_PKG_INCLUDE_DIRS}
    /usr/include
    /usr/local/include
  )

  find_library(LIBNANOMSG_LIBRARIES NAMES nanomsg
    PATHS
    ${LIBNANOMSG_PKG_LIBRARY_DIRS}
    /usr/lib
    /usr/local/lib
  )

  if(LIBNANOMSG_INCLUDE_DIR AND LIBNANOMSG_LIBRARIES)
    set(LIBNANOMSG_FOUND TRUE CACHE INTERNAL "libnanomsg found")
    message(STATUS "Found libnanomsg: ${LIBNANOMSG_INCLUDE_DIR}, ${LIBNANOMSG_LIBRARIES}")
  else(LIBNANOMSG_INCLUDE_DIR AND LIBNANOMSG_LIBRARIES)
    set(LIBNANOMSG_FOUND FALSE CACHE INTERNAL "libnanomsg found")
    message(STATUS "libnanomsg not found.")
  endif(LIBNANOMSG_INCLUDE_DIR AND LIBNANOMSG_LIBRARIES)

  mark_as_advanced(LIBNANOMSG_INCLUDE_DIR LIBNANOMSG_LIBRARIES)

endif(NOT LIBNANOMSG_FOUND)
