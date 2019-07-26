# Find serialDV

find_path(LIBSERIALDV_INCLUDE_DIR
  NAMES dvcontroller.h
  HINTS ${SERIALDV_DIR}/include/serialdv
  PATHS /usr/include/serialdv
        /usr/local/include/serialdv
)

set(LIBSERIAL_NAMES ${LIBSERIAL_NAMES} serialdv libserialdv)

find_library(LIBSERIALDV_LIBRARY
  NAMES serialdv
  HINTS ${SERIALDV_DIR}/lib
  PATHS /usr/lib
        /usr/local/lib
)

if (LIBSERIALDV_INCLUDE_DIR AND LIBSERIALDV_LIBRARY)
    set(LIBSERIALDV_FOUND TRUE)
endif (LIBSERIALDV_INCLUDE_DIR AND LIBSERIALDV_LIBRARY)

if (LIBSERIALDV_FOUND)
    if (NOT SerialDV_FIND_QUIETLY)
        message (STATUS "Found libserialdv: ${LIBSERIALDV_INCLUDE_DIR}, ${LIBSERIALDV_LIBRARY}")
    endif (NOT SerialDV_FIND_QUIETLY)
else (LIBSERIALDV_FOUND)
    if (SerialDV_FIND_REQUIRED)
        message (FATAL_ERROR "Could not find SerialDV")
    endif (SerialDV_FIND_REQUIRED)
endif (LIBSERIALDV_FOUND)

mark_as_advanced(LIBSERIALDV_INCLUDE_DIR LIBSERIALDV_LIBRARY)
