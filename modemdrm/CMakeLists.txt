project(modemdrm)

set(modemdrm_SOURCES
    drmacquisition.cpp
    drmaudiodecoder.cpp
    drmaudioframing.cpp
    drmbitutils.cpp
    drmcellmap.cpp
    drmchannelestimator.cpp
    drmconvolutional.cpp
    drmdata.cpp
    drmfac.cpp
    drmfacacquisition.cpp
    drmjournaline.cpp
    drminterleaver.cpp
    drmmsc.cpp
    drmofdm.cpp
    drmparameters.cpp
    drmreceiver.cpp
    drmresampler.cpp
    drmsdc.cpp
    drmspi.cpp
    drmsyncstatemachine.cpp
)

set(modemdrm_HEADERS
    drmacquisition.h
    drmaudiodecoder.h
    drmaudioframing.h
    drmbitutils.h
    drmcellmap.h
    drmchannelestimator.h
    drmconvolutional.h
    drmdata.h
    drmfac.h
    drmfacacquisition.h
    drmjournaline.h
    drminterleaver.h
    drmmsc.h
    drmofdm.h
    drmparameters.h
    drmreceiver.h
    drmresampler.h
    drmsdc.h
    drmspi.h
    drmsyncstatemachine.h
)

add_library(modemdrm ${modemdrm_SOURCES})

target_include_directories(modemdrm
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/exports
)

# MOT, Journaline, SPI and the Viterbi decoder are shared with DAB
target_link_libraries(modemdrm PUBLIC modemdr)

# DRM AAC/xHE-AAC audio uses the FDK AAC decoder
if(FDKAAC_FOUND)
    message(STATUS "modemdrm: DRM AAC/xHE-AAC audio via FDK AAC")
    target_compile_definitions(modemdrm PRIVATE MODEMDRM_HAS_FDK_AAC=1)
    target_include_directories(modemdrm PRIVATE ${FDKAAC_INCLUDE_DIR})
    target_link_libraries(modemdrm PRIVATE ${FDKAAC_LIBRARY})
    if(DEFINED FDKAAC_DEPENDS)
        add_dependencies(modemdrm ${FDKAAC_DEPENDS})
    endif()
    # A static FDK (the external one, or one built for a package) is private to
    # modemdrm. Do not re-export its aacDecoder_* symbols from a shared
    # modemdrm, so they cannot interpose on (or be interposed by) the
    # libfdk-aac that FFmpeg loads in the same process.
    if((FDKAAC_EXTERNAL OR FDKAAC_LIBRARY MATCHES "\\.a$") AND BUILD_SHARED_LIBS AND UNIX AND NOT APPLE)
        target_link_options(modemdrm PRIVATE "LINKER:--exclude-libs,libfdk-aac.a")
    endif()
endif()

# DRM AAC can be decoded by FAAD2 built for DRM (faad_drm), but it does not support xHE-AAC.
if(FAAD_DRM_LIBRARY AND NOT FDKAAC_FOUND)
    message(STATUS "modemdrm: DRM AAC audio via FAAD2 - xHE-AAC not supported")
    target_compile_definitions(modemdrm PRIVATE MODEMDRM_HAS_FAAD2=1)
    target_include_directories(modemdrm PRIVATE ${FAAD_INCLUDE_DIR})
    target_link_libraries(modemdrm PRIVATE ${FAAD_DRM_LIBRARY})
    if(DEFINED FAAD_DEPENDS)
        add_dependencies(modemdrm ${FAAD_DEPENDS})
    endif()
endif()

# Without either library DRM still demodulates and lists services, and reports the codec as unavailable.
if(NOT FDKAAC_FOUND AND NOT FAAD_DRM_LIBRARY)
    message(STATUS "modemdrm: no DRM audio codec")
endif()

install(TARGETS modemdrm DESTINATION ${INSTALL_LIB_DIR})

if(FDKAAC_FOUND AND NOT FDKAAC_EXTERNAL AND WIN32 AND FDKAAC_RUNTIME_LIBRARY)
    # Shipping a prebuilt library requires shipping its licence (the external
    # build installs its NOTICE itself).
    get_filename_component(_fdk_library_dir "${FDKAAC_LIBRARY}" DIRECTORY)
    find_file(FDKAAC_NOTICE NAMES NOTICE NOTICE.txt
        HINTS ${FDKAAC_DIR} ${FDK_AAC_DIR} "${_fdk_library_dir}/.." "${FDKAAC_INCLUDE_DIR}/.."
        PATH_SUFFIXES . share/doc/fdk-aac share/licenses/fdk-aac)
    unset(_fdk_library_dir)
    if(NOT FDKAAC_NOTICE)
        message(FATAL_ERROR "Installing ${FDKAAC_RUNTIME_LIBRARY} requires its licence: set FDKAAC_NOTICE to the FDK AAC NOTICE file")
    endif()
    install(FILES "${FDKAAC_RUNTIME_LIBRARY}" DESTINATION ${INSTALL_BIN_DIR})
    install(FILES "${FDKAAC_NOTICE}" DESTINATION "${INSTALL_DOC_DIR}/fdk-aac" RENAME NOTICE)
endif()
