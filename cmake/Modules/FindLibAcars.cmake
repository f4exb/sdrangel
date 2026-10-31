IF (NOT LIBACARS_FOUND)
    FIND_PATH(
        LIBACARS_INCLUDE_DIR
        NAMES libacars/libacars.h
        HINTS ${LIBACARS_DIR}/include/libacars-2
        PATHS /usr/include/libacars-2
              /usr/local/include/libacars-2
    )

    FIND_LIBRARY(
        LIBACARS_LIBRARY
        NAMES acars-2
        HINTS ${LIBACARS_DIR}/lib
              ${LIBACARS_DIR}/lib64
        PATHS /usr/lib
              /usr/lib64
              /usr/local/lib
              /usr/local/lib64
    )

    if (LIBACARS_INCLUDE_DIR AND LIBACARS_LIBRARY)
        set(LIBACARS_LIBRARIES ${LIBACARS_LIBRARY} CACHE INTERNAL "libacars libraries")
        set(LIBACARS_FOUND TRUE CACHE INTERNAL "libacars found")
        message(STATUS "Found libacars: ${LIBACARS_INCLUDE_DIR}, ${LIBACARS_LIBRARIES}")
    else ()
        set(LIBACARS_FOUND FALSE CACHE INTERNAL "libacars found")
        message(STATUS "libacars not found.")
    endif ()

    mark_as_advanced(LIBACARS_INCLUDE_DIR LIBACARS_LIBRARIES)

ENDIF (NOT LIBACARS_FOUND)
