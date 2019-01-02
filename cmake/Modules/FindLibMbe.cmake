# Find libmbe

find_path(LIBMBE_INCLUDE_DIR 
  NAMES mbelib.h
  PATHS ${MBE_DIR}/include
        /usr/include
        /usr/local/include
)

set(LIBMBE_NAMES ${LIBMBE_NAMES} mbe libmbe)

find_library(LIBMBE_LIBRARY 
  NAMES ${LIBMBE_NAMES} 
  PATHS ${MBE_DIR}/lib
        /usr/include
        /usr/local/include
)

if (LIBMBE_INCLUDE_DIR AND LIBMBE_LIBRARY)
    set(LIBMBE_FOUND TRUE)
endif (LIBMBE_INCLUDE_DIR AND LIBMBE_LIBRARY)

if (LIBMBE_FOUND)
    if (NOT LibMbe_FIND_QUIETLY)
        message (STATUS "Found LibMbe: ${LIBMBE_INCLUDE_DIR}, ${LIBMBE_LIBRARY}")
    endif (NOT LibMbe_FIND_QUIETLY)
else (LIBMBE_FOUND)
    if (LibMbe_FIND_REQUIRED)
        message (FATAL_ERROR "Could not find mbe")
    endif (LibMbe_FIND_REQUIRED)
endif (LIBMBE_FOUND)

mark_as_advanced(LIBMBE_INCLUDE_DIR LIBMBE_LIBRARY)
