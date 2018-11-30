project(dsdcc)

set(dsdcc_SOURCES
    ${LIBDSDCCSRC}/descramble.cpp
    ${LIBDSDCCSRC}/dmr.cpp
    ${LIBDSDCCSRC}/dsd_decoder.cpp
    ${LIBDSDCCSRC}/dsd_filters.cpp
    ${LIBDSDCCSRC}/dsd_logger.cpp
    ${LIBDSDCCSRC}/dsd_mbe.cpp
    ${LIBDSDCCSRC}/dsd_opts.cpp
    ${LIBDSDCCSRC}/dsd_state.cpp
    ${LIBDSDCCSRC}/dsd_symbol.cpp
    ${LIBDSDCCSRC}/dstar.cpp
    ${LIBDSDCCSRC}/ysf.cpp
    ${LIBDSDCCSRC}/dpmr.cpp
    ${LIBDSDCCSRC}/nxdn.cpp
    ${LIBDSDCCSRC}/nxdnconvolution.cpp
    ${LIBDSDCCSRC}/nxdncrc.cpp
    ${LIBDSDCCSRC}/nxdnmessage.cpp
    ${LIBDSDCCSRC}/p25p1_heuristics.cpp
    ${LIBDSDCCSRC}/fec.cpp
    ${LIBDSDCCSRC}/crc.cpp
    ${LIBDSDCCSRC}/viterbi.cpp
    ${LIBDSDCCSRC}/viterbi3.cpp
    ${LIBDSDCCSRC}/viterbi5.cpp
    ${LIBDSDCCSRC}/pn.cpp
    ${LIBDSDCCSRC}/mbefec.cpp
    ${LIBDSDCCSRC}/locator.cpp
    ${LIBDSDCCSRC}/phaselock.cpp
)

set(dsdcc_HEADERS
    ${LIBDSDCCSRC}/descramble.h
    ${LIBDSDCCSRC}/dmr.h
    ${LIBDSDCCSRC}/dsd_decoder.h
    ${LIBDSDCCSRC}/dsd_filters.h
    ${LIBDSDCCSRC}/dsd_logger.h
    ${LIBDSDCCSRC}/dsd_mbe.h
    ${LIBDSDCCSRC}/dsd_mbelib.h
    ${LIBDSDCCSRC}/dsd_opts.h
    ${LIBDSDCCSRC}/dsd_state.h
    ${LIBDSDCCSRC}/dsd_symbol.h
    ${LIBDSDCCSRC}/dstar.h
    ${LIBDSDCCSRC}/ysf.h
    ${LIBDSDCCSRC}/dpmr.h
    ${LIBDSDCCSRC}/nxdn.h
    ${LIBDSDCCSRC}/nxdnconvolution.h
    ${LIBDSDCCSRC}/nxdncrc.h
    ${LIBDSDCCSRC}/nxdnmessage.h
    ${LIBDSDCCSRC}/p25p1_heuristics.h
    ${LIBDSDCCSRC}/runningmaxmin.h
    ${LIBDSDCCSRC}/doublebuffer.h
    ${LIBDSDCCSRC}/fec.h
    ${LIBDSDCCSRC}/crc.h
    ${LIBDSDCCSRC}/viterbi.h
    ${LIBDSDCCSRC}/viterbi3.h
    ${LIBDSDCCSRC}/viterbi5.h
    ${LIBDSDCCSRC}/pn.h
    ${LIBDSDCCSRC}/mbefec.h
    ${LIBDSDCCSRC}/locator.h
    ${LIBDSDCCSRC}/phaselock.h
)

add_definitions(-DDSD_USE_MBELIB)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBDSDCCSRC}
    ${LIBMBELIBSRC}
)

add_definitions(-DQT_SHARED)

add_library(dsdcc SHARED
    ${dsdcc_SOURCES}
)

target_link_libraries(dsdcc
    mbelib
)

install(TARGETS dsdcc DESTINATION lib)
