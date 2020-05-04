# Find libmbe
if (NOT LIBMBE_FOUND)
    find_path(LIBMBE_INCLUDE_DIR
            NAMES mbelib.h
            HINTS ${MBE_DIR}/include
            PATHS /usr/include
            /usr/local/include
            )

    set(LIBMBE_NAMES ${LIBMBE_NAMES} mbe libmbe)

    find_library(LIBMBE_LIBRARIES
            NAMES ${LIBMBE_NAMES}
            HINTS ${MBE_DIR}/lib
            PATHS /usr/lib
            /usr/local/lib
            )

    if (LIBMBE_INCLUDE_DIR AND LIBMBE_LIBRARIES)
        set(LIBMBE_FOUND TRUE CACHE INTERNAL "libmbe found")

        if (NOT LibMbe_FIND_QUIETLY)
            message(STATUS "Found LibMbe: ${LIBMBE_INCLUDE_DIR}, ${LIBMBE_LIBRARY}")
        endif (NOT LibMbe_FIND_QUIETLY)
    else (LIBMBE_INCLUDE_DIR AND LIBMBE_LIBRARIES)
        set(LIBMBE_FOUND FALSE CACHE INTERNAL "libmbe found")

        if (LibMbe_FIND_REQUIRED)
            message(FATAL_ERROR "Could not find LibMbe")
        endif (LibMbe_FIND_REQUIRED)
    endif (LIBMBE_INCLUDE_DIR AND LIBMBE_LIBRARIES)

    mark_as_advanced(LIBMBE_INCLUDE_DIR LIBMBE_LIBRARIES)
endif (NOT LIBMBE_FOUND)