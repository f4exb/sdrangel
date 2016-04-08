project(dsd)

set(dsd_SOURCES
    dmr_const.c
    dmr_data.c
    dmr_voice.c
    dsd_audio.c
    dsd_cleanupexit.c
    dsd_comp.c
    dsd_dibit.c
#   dsd_file.c
    dsd_filters.c
    dsd_frame_sync.c
    dsd_frame.c
    dsd_livescanner.c
    dsd_mbe.c
    dsd_nocarrier.c
    dsd_opts.c
    dsd_state.c
    dsd_symbol.c
    dsd_upsample.c
    dstar_const.c
    dstar_header.c
    dstar.c
    nxdn_const.c
    nxdn_data.c
    nxdn_voice.c
    nxdn96.c
    nxdn96_const.c
    p25_lcw.c
    p25p1_const.c
    p25p1_heuristics.c
    p25p1_hdu.c
    p25p1_ldu1.c
    p25p1_ldu2.c
    p25p1_tdulc.c
    provoice.c
    provoice_const.c
    x2tdma_const.c
    x2tdma_data.c
    x2tdma_voice.c
)

set(dsd_HEADERS
    config.h
    descramble.h
    dmr_const.h
    dsd_cleanupexit.h
    dsd_comp.h
    dsd_livescanner.h
    dsd_nocarrier.h
    dsd_opts.h
    dsd_state.h
    dsd.h
    dstar_const.h
    dstar_header.h
    fcs.h
    nxdn_const.h
    nxdn96_const.h
    p25p1_const.h
    p25p1_heuristics.h
    provoice_const.h
    x2tdma_const.h
)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBMBE_INCLUDE_DIR}
)

SET(LIBS ${LIBS} ${LIBMBE_LIBRARY})

add_definitions(-DQT_SHARED)

add_library(dsd SHARED
    ${dsd_SOURCES}
)

target_link_libraries(dsd ${LIBS})

install(TARGETS dsd DESTINATION lib)


