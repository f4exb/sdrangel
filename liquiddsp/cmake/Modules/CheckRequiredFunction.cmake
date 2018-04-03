include(CheckFunctionExists)
include(CheckLibraryExists)

macro(CHECK_REQUIRED_FUNCTION FUNCTION LIBRARY VARIABLE)
    # First try without any library.
    CHECK_FUNCTION_EXISTS("${FUNCTION}" ${VARIABLE})
    if (NOT ${VARIABLE})
        unset(${VARIABLE} CACHE)
        # Retry with the library specified
        CHECK_LIBRARY_EXISTS("${LIBRARY}" "${FUNCTION}" "" ${VARIABLE})
    endif ()
    if (NOT ${VARIABLE})
        message(FATAL_ERROR "Required function '${FUNCTION}' not found")
    endif ()
endmacro ()

