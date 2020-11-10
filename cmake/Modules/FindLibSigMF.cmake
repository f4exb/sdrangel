# Find libdsdcc

if (NOT LIBSIGMF_FOUND)

    pkg_check_modules(LIBSIGMF_PKG libsigmf)

    find_path (LIBSIGMF_INCLUDE_DIR
        NAMES libsigmf/sigmf_sdrangel_generated.h
        HINTS ${LIBSIGMF_DIR}/include
              ${LIBSIGMF_PKG_INCLUDE_DIRS}
        PATHS /usr/include/libsigmf
              /usr/local/include/libsigmf
    )

    find_library (LIBSIGMF_LIBRARIES
        NAMES libsigmf
        HINTS ${LIBSIGMF_DIR}/lib
              ${LIBSIGMF_DIR}/lib64
              ${LIBSIGMF_PKG_LIBRARY_DIRS}
        PATHS /usr/lib
              /usr/lib64
              /usr/local/lib
              /usr/local/lib64
    )

    if (LIBSIGMF_INCLUDE_DIR AND LIBSIGMF_LIBRARIES)
        set(LIBSIGMF_FOUND TRUE CACHE INTERNAL "libsigmf found")
        message(STATUS "Found libsigmf: ${LIBSIGMF_INCLUDE_DIR}, ${LIBSIGMF_LIBRARIES}")
    else (LIBSIGMF_INCLUDE_DIR AND LIBSIGMF_LIBRARIES)
        set(LIBSIGMF_FOUND FALSE CACHE INTERNAL "libsigmf found")
        message(STATUS "libsigmf not found.")
    endif (LIBSIGMF_INCLUDE_DIR AND LIBSIGMF_LIBRARIES)

    mark_as_advanced(LIBSIGMF_INCLUDE_DIR LIBSIGMF_LIBRARIES)

endif (NOT LIBSIGMF_FOUND)
