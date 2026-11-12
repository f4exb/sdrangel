# Find vocoscodec (https://github.com/srcejon/vocoscodec, EnCodec + Vocos): Angel61's V1 voice codec.
# SDRangel loads its shared library at run time rather than links it (sdrbase/dsp/dv/vocoscodec.cpp),
# so nothing is compiled against it: this only finds whether it is installed, and where, so that
# external/CMakeLists.txt builds it when it is not.
#
# In order:
#   - the installed package (VocoscodecConfig.cmake), under VOCOSCODEC_DIR or Vocoscodec_DIR if given;
#   - the library and the models, under VOCOSCODEC_DIR if given, then the system's paths.
# Once external/CMakeLists.txt builds it (VOCOSCODEC_EXTERNAL), it is not looked for any more.
#
# Sets:
#   VOCOSCODEC_FOUND     - the library and its models were found
#   VOCOSCODEC_LIBRARY   - the shared library (vocoscodec.dll, libvocoscodec.so or libvocoscodec.dylib)
#   VOCOSCODEC_MODEL_DIR - the directory of its models (encodec_encoder.bin, encodec_codebooks.bin and
#                          vocos_decoder.bin)
#
# At run time SDRangel looks for the library beside its executable, in <bin>/../lib/sdrangel,
# <bin>/../lib64/sdrangel and <bin>/../lib, then where the system's loader looks; and for the models in
# $VOCOSCODEC_MODEL_DIR, <bin>/vocoscodec, <bin>/../share/sdrangel/vocoscodec, then the directory the
# library was built with (VOCOSCODEC_RUNTIME_MODEL_DIR when building vocoscodec).

if(VOCOSCODEC_EXTERNAL OR VOCOSCODEC_RUNTIME_LIBRARY)
    # Built by external/CMakeLists.txt, which builds it again
    set(VOCOSCODEC_FOUND FALSE)
    set(LibVocosCodec_FOUND FALSE)
    return()
endif()

find_package(Vocoscodec CONFIG QUIET
    HINTS ${VOCOSCODEC_DIR}
)

if(Vocoscodec_FOUND AND TARGET vocoscodec::runtime)
    get_target_property(VOCOSCODEC_PACKAGE_LIBRARY vocoscodec::runtime LOCATION)
    set(VOCOSCODEC_LIBRARY "${VOCOSCODEC_PACKAGE_LIBRARY}" CACHE FILEPATH "vocoscodec shared library" FORCE)
endif()

if(WIN32)
    # find_library finds a DLL's import library, not the DLL
    find_file(VOCOSCODEC_LIBRARY
        NAMES vocoscodec.dll
        HINTS ${VOCOSCODEC_DIR}/bin
              ${VOCOSCODEC_DIR}/lib
    )
else()
    find_library(VOCOSCODEC_LIBRARY
        NAMES vocoscodec
        HINTS ${VOCOSCODEC_DIR}/lib
              ${VOCOSCODEC_DIR}/lib64
              ${VOCOSCODEC_DIR}/lib/sdrangel
              ${VOCOSCODEC_DIR}/lib64/sdrangel
        PATHS /usr/lib
              /usr/lib64
              /usr/local/lib
              /usr/local/lib64
    )
endif()

# vocoscodec installs them in share/vocoscodec/model of its prefix: that of the library found
# (lib, or lib/<multiarch triplet>) when no VOCOSCODEC_DIR was given
set(VOCOSCODEC_MODEL_HINTS ${Vocoscodec_MODEL_DIR})
if(VOCOSCODEC_DIR)
    list(APPEND VOCOSCODEC_MODEL_HINTS
        "${VOCOSCODEC_DIR}/share/vocoscodec/model"
        "${VOCOSCODEC_DIR}/share/sdrangel/vocoscodec")
endif()
if(VOCOSCODEC_LIBRARY)
    get_filename_component(VOCOSCODEC_LIBRARY_DIR "${VOCOSCODEC_LIBRARY}" DIRECTORY)
    list(APPEND VOCOSCODEC_MODEL_HINTS
        "${VOCOSCODEC_LIBRARY_DIR}/../share/vocoscodec/model"
        "${VOCOSCODEC_LIBRARY_DIR}/../../share/vocoscodec/model")
endif()

find_path(VOCOSCODEC_MODEL_DIR
    NAMES vocos_decoder.bin
    HINTS ${VOCOSCODEC_MODEL_HINTS}
    PATHS /usr/share/vocoscodec/model
          /usr/local/share/vocoscodec/model
    NO_DEFAULT_PATH
)

if(VOCOSCODEC_LIBRARY
        AND EXISTS "${VOCOSCODEC_MODEL_DIR}/encodec_encoder.bin"
        AND EXISTS "${VOCOSCODEC_MODEL_DIR}/encodec_codebooks.bin"
        AND EXISTS "${VOCOSCODEC_MODEL_DIR}/vocos_decoder.bin")
    set(VOCOSCODEC_FOUND TRUE)
    if(NOT LibVocosCodec_FIND_QUIETLY)
        message(STATUS "Found vocoscodec: ${VOCOSCODEC_LIBRARY}, models in ${VOCOSCODEC_MODEL_DIR}")
    endif()
else()
    set(VOCOSCODEC_FOUND FALSE)
endif()

# Not cached: external/CMakeLists.txt builds it when it is not found
set(LibVocosCodec_FOUND ${VOCOSCODEC_FOUND})

mark_as_advanced(VOCOSCODEC_LIBRARY VOCOSCODEC_MODEL_DIR)

if(NOT VOCOSCODEC_FOUND AND LibVocosCodec_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find vocoscodec")
endif()
