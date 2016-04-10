project(dsdplus)

set(dsdplus_SOURCES
    dmr_voice.cpp
    dsd_decoder.cpp
    dsd_filters.cpp
    dsd_mbe.cpp
    dsd_opts.cpp
    dsd_state.cpp
)

set(dsdplus_HEADERS
    dmr_voice.h
    dsd_decoder.h
    dsd_filters.h
    dsd_mbe.h
    dsd_opts.h
    dsd_state.h
)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBMBE_INCLUDE_DIR}
)

add_library(dsdplus SHARED
    ${dsdplus_SOURCES}
)

target_link_libraries(dsdplus ${LIBMBE_LIBRARY})

install(TARGETS dsdplus DESTINATION lib)
