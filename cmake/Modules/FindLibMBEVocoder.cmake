# Find mbevocoder (https://github.com/srcejon/mbevocoder): the AMBE, AMBE+2 and IMBE
# vocoder of the DSD demodulator and the DSE modulator, a static library.
#
# In order:
#   - MBEVOCODER_INCLUDE_DIRS and MBEVOCODER_LIBRARY given by hand (or by external/CMakeLists.txt, which
#     builds it, on an earlier configure);
#   - the installed package (mbevocoderConfig.cmake), under MBEVOCODER_DIR or mbevocoder_DIR if given;
#   - the header and library, under MBEVOCODER_DIR if given, then where pkg-config says, then the
#     system's paths.
#
# Sets:
#   MBEVOCODER_FOUND        - mbevocoder was found
#   MBEVOCODER_INCLUDE_DIRS - its include directory (empty when MBEVOCODER_LIBRARY is the imported target)
#   MBEVOCODER_LIBRARY      - the library to link: mbevocoder::mbevocoder, or the library file
#
# Users of it compile with MBE_STATIC defined (the package's target defines it itself).

set(MBEVOCODER_INCLUDE_DIRS "" CACHE STRING "mbevocoder include directories (semicolon separated, must contain mbevocoder/mbevocoder.h and the generated mbevocoder/version.h)")
set(MBEVOCODER_LIBRARY "" CACHE FILEPATH "mbevocoder static library")

if(MBEVOCODER_INCLUDE_DIRS AND MBEVOCODER_LIBRARY)
    set(MBEVOCODER_FOUND TRUE)
    if(NOT LibMBEVocoder_FIND_QUIETLY)
        message(STATUS "Found mbevocoder: ${MBEVOCODER_LIBRARY}")
    endif()
else()
    find_package(mbevocoder CONFIG QUIET
        HINTS ${MBEVOCODER_DIR}
    )

    if(mbevocoder_FOUND AND TARGET mbevocoder::mbevocoder)
        set(MBEVOCODER_FOUND TRUE)
        set(MBEVOCODER_INCLUDE_DIRS "")
        set(MBEVOCODER_LIBRARY mbevocoder::mbevocoder)
        if(NOT LibMBEVocoder_FIND_QUIETLY)
            message(STATUS "Found mbevocoder package: ${mbevocoder_DIR} (${mbevocoder_VERSION})")
        endif()
    else()
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(MBEVOCODER_PKG QUIET mbevocoder)
        endif()

        find_path(MBEVOCODER_INCLUDE_DIR
            NAMES mbevocoder/mbevocoder.h
            HINTS ${MBEVOCODER_DIR}/include
                  ${MBEVOCODER_PKG_INCLUDE_DIRS}
            PATHS /usr/include
                  /usr/local/include
        )

        # The debug build of Windows has a 'd' postfix
        find_library(MBEVOCODER_LIBRARY_FILE
            NAMES mbevocoder mbevocoderd
            HINTS ${MBEVOCODER_DIR}/lib
                  ${MBEVOCODER_DIR}/lib64
                  ${MBEVOCODER_PKG_LIBRARY_DIRS}
            PATHS /usr/lib
                  /usr/lib64
                  /usr/local/lib
                  /usr/local/lib64
        )

        if(MBEVOCODER_INCLUDE_DIR AND MBEVOCODER_LIBRARY_FILE)
            set(MBEVOCODER_FOUND TRUE)
            set(MBEVOCODER_INCLUDE_DIRS "${MBEVOCODER_INCLUDE_DIR}")
            set(MBEVOCODER_LIBRARY "${MBEVOCODER_LIBRARY_FILE}")
            if(NOT LibMBEVocoder_FIND_QUIETLY)
                message(STATUS "Found mbevocoder: ${MBEVOCODER_INCLUDE_DIR}, ${MBEVOCODER_LIBRARY_FILE}")
            endif()
        else()
            set(MBEVOCODER_FOUND FALSE)
        endif()

        mark_as_advanced(MBEVOCODER_INCLUDE_DIR MBEVOCODER_LIBRARY_FILE)
    endif()
endif()

# Not cached: external/CMakeLists.txt builds it when it is not found
set(LibMBEVocoder_FOUND ${MBEVOCODER_FOUND})

if(NOT MBEVOCODER_FOUND AND LibMBEVocoder_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find mbevocoder")
endif()
