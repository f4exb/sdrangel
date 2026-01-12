IF (NOT INMARSATC_FOUND)
    FIND_PATH(
        INMARSATC_INCLUDE_DIR
        NAMES inmarsatc_decoder.h inmarsatc_parser.h
        HINTS ${INMARSATC_DIR}/include
        PATHS /usr/include
              /usr/local/include
    )

    # We don't currently use inmarsatc_demodulator

    FIND_LIBRARY(
        INMARSATC_DECODER_LIBRARY
        NAMES inmarsatc_decoder
        HINTS ${INMARSATC_DIR}/lib
              ${INMARSATC_DIR}/lib64
        PATHS /usr/lib
              /usr/lib64
              /usr/local/lib
              /usr/local/lib64
    )

    FIND_LIBRARY(
        INMARSATC_PARSER_LIBRARY
        NAMES inmarsatc_parser
        HINTS ${INMARSATC_DIR}/lib
              ${INMARSATC_DIR}/lib64
        PATHS /usr/lib
              /usr/lib64
              /usr/local/lib
              /usr/local/lib64
    )

    if (INMARSATC_INCLUDE_DIR AND INMARSATC_DECODER_LIBRARY AND INMARSATC_PARSER_LIBRARY)
        set(INMARSATC_LIBRARIES ${INMARSATC_DECODER_LIBRARY} ${INMARSATC_PARSER_LIBRARY} CACHE INTERNAL "inmarsatc libraries")
        set(INMARSATC_FOUND TRUE CACHE INTERNAL "inmarsatc found")
        message(STATUS "Found inmarsatc: ${INMARSATC_INCLUDE_DIR}, ${INMARSATC_LIBRARIES}")
    else ()
        set(INMARSATC_FOUND FALSE CACHE INTERNAL "inmarsatc found")
        message(STATUS "inmarsatc not found.")
    endif ()

    mark_as_advanced(INMARSATC_INCLUDE_DIR INMARSATC_LIBRARIES)

ENDIF (NOT INMARSATC_FOUND)
