if(NOT LIBBLADERF_FOUND)

  pkg_check_modules (LIBBLADERF_PKG libbladeRF>=2.0)

  if(LIBBLADERF_PKG_FOUND OR (DEFINED BLADERF_DIR))
    find_path(LIBBLADERF_INCLUDE_DIRS
      NAMES libbladeRF.h
      HINTS ${BLADERF_DIR}/include
            ${LIBBLADERF_PKG_INCLUDE_DIRS}
      PATHS /usr/include
            /usr/local/include
      )

    find_library(LIBBLADERF_LIBRARIES
      NAMES bladeRF
      HINTS ${BLADERF_DIR}/lib
            ${LIBBLADERF_PKG_LIBRARY_DIRS}
      PATHS /usr/lib
            /usr/lib64
            /usr/local/lib
            /usr/local/lib64
      )

    if(LIBBLADERF_INCLUDE_DIRS AND LIBBLADERF_LIBRARIES)
      set(LIBBLADERF_FOUND TRUE CACHE INTERNAL "libbladerf found")
      message(STATUS "Found libbladerf: ${LIBBLADERF_INCLUDE_DIRS}, ${LIBBLADERF_LIBRARIES}")
    else()
      set(LIBBLADERF_FOUND FALSE CACHE INTERNAL "libbladerf not found")
      message(STATUS "libbladerf not found.")
    endif()

    mark_as_advanced(LIBBLADERF_INCLUDE_DIRS LIBBLADERF_LIBRARIES)

  endif(LIBBLADERF_PKG_FOUND OR (DEFINED BLADERF_DIR))

endif(NOT LIBBLADERF_FOUND)
