if(NOT FDKAAC_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PC_FDKAAC QUIET fdk-aac)
    endif()

    find_path(
        FDKAAC_INCLUDE_DIR
        NAMES fdk-aac/aacdecoder_lib.h aacdecoder_lib.h
        HINTS ${FDKAAC_DIR}/include
              ${FDK_AAC_DIR}/include
              ${PC_FDKAAC_INCLUDE_DIRS}
        PATHS /usr/local/include
              /usr/include
    )

    find_library(
        FDKAAC_LIBRARY
        NAMES fdk-aac fdk_aac
        HINTS ${FDKAAC_DIR}/lib
              ${FDK_AAC_DIR}/lib
              ${PC_FDKAAC_LIBRARY_DIRS}
        PATHS /usr/local/lib
              /usr/lib
              /usr/lib64
    )

    if(FDKAAC_INCLUDE_DIR AND FDKAAC_LIBRARY)
        include(CheckCXXSourceCompiles)
        set(_FDKAAC_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}")
        set(_FDKAAC_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES}")
        set(CMAKE_REQUIRED_INCLUDES "${FDKAAC_INCLUDE_DIR}")
        set(CMAKE_REQUIRED_LIBRARIES "${FDKAAC_LIBRARY}")
        # Recheck when switching packages; a cached header-only success is not
        # evidence that the selected library exports the decoder interface.
        file(SHA256 "${FDKAAC_LIBRARY}" _FDKAAC_LIBRARY_HASH)
        set(_FDKAAC_PACKAGE_KEY "${FDKAAC_INCLUDE_DIR};${FDKAAC_LIBRARY};${_FDKAAC_LIBRARY_HASH};link-v1")
        if(NOT "${FDKAAC_CHECKED_PACKAGE}" STREQUAL "${_FDKAAC_PACKAGE_KEY}")
            unset(FDKAAC_DRM_INTERFACE_AVAILABLE CACHE)
            unset(FDKAAC_RUNTIME_LIBRARY CACHE)
        endif()
        check_cxx_source_compiles("
#if __has_include(<fdk-aac/aacdecoder_lib.h>)
# include <fdk-aac/aacdecoder_lib.h>
#else
# include <aacdecoder_lib.h>
#endif
#if AACDECODER_LIB_VL0 < 3
# error FDK AAC decoder is too old for the validated DRM interface
#endif
int main()
{
    TRANSPORT_TYPE transport = TT_DRM;
    AUDIO_OBJECT_TYPE objectType = AOT_DRM_USAC;
    HANDLE_AACDECODER decoder = aacDecoder_Open(transport, 2);
    UCHAR *bytes = nullptr;
    UINT size = 0, valid = 0;
    INT_PCM pcm[4096] = {};
    AAC_DECODER_ERROR status = aacDecoder_ConfigRaw(decoder, &bytes, &size);
    status = aacDecoder_Fill(decoder, &bytes, &size, &valid);
    status = aacDecoder_DecodeFrame(decoder, pcm, 4096, AACDEC_CONCEAL);
    CStreamInfo *info = aacDecoder_GetStreamInfo(decoder);
    aacDecoder_Close(decoder);
    return info && status == AAC_DEC_OK && objectType == AOT_DRM_USAC ? 0 : 1;
}
" FDKAAC_DRM_INTERFACE_AVAILABLE)
        set(CMAKE_REQUIRED_INCLUDES "${_FDKAAC_REQUIRED_INCLUDES}")
        set(CMAKE_REQUIRED_LIBRARIES "${_FDKAAC_REQUIRED_LIBRARIES}")
        set(FDKAAC_CHECKED_PACKAGE "${_FDKAAC_PACKAGE_KEY}" CACHE INTERNAL "FDK AAC interface probe package")
        unset(_FDKAAC_REQUIRED_INCLUDES)
        unset(_FDKAAC_REQUIRED_LIBRARIES)
        unset(_FDKAAC_LIBRARY_HASH)
        unset(_FDKAAC_PACKAGE_KEY)
    endif()

    if(WIN32)
        get_filename_component(_FDKAAC_LIBRARY_DIR "${FDKAAC_LIBRARY}" DIRECTORY)
        find_file(
            FDKAAC_RUNTIME_LIBRARY
            NAMES fdk-aac.dll libfdk-aac.dll fdk-aac-2.dll libfdk-aac-2.dll
            HINTS ${FDKAAC_DIR}/bin
                  ${FDK_AAC_DIR}/bin
                  ${_FDKAAC_LIBRARY_DIR}
                  ${_FDKAAC_LIBRARY_DIR}/../bin
                  ${PC_FDKAAC_LIBRARY_DIRS}
        )
        unset(_FDKAAC_LIBRARY_DIR)
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
        FDKAAC
        DEFAULT_MSG
        FDKAAC_LIBRARY
        FDKAAC_INCLUDE_DIR
        FDKAAC_DRM_INTERFACE_AVAILABLE
    )

    mark_as_advanced(
        FDKAAC_LIBRARY
        FDKAAC_INCLUDE_DIR
        FDKAAC_RUNTIME_LIBRARY
        FDKAAC_DRM_INTERFACE_AVAILABLE
    )
endif()

if(FDKAAC_FOUND)
    set(FDKAAC_INCLUDE_DIRS ${FDKAAC_INCLUDE_DIR})
    set(FDKAAC_LIBRARIES ${FDKAAC_LIBRARY})
    # Deliberately not FDK_AAC_FOUND: that spelling decides whether the FFmpeg
    # external project builds its own copy, and FFmpeg finds FDK through
    # pkg-config, which this DRM-only discovery does not guarantee.
endif()
