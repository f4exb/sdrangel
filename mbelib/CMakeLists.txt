project(mbelib)

set(mbelib_SOURCES
    ${LIBMBELIBSRC}/ambe3600x2400.c
    ${LIBMBELIBSRC}/ambe3600x2450.c
    ${LIBMBELIBSRC}/ecc.c
    ${LIBMBELIBSRC}/imbe7100x4400.c
    ${LIBMBELIBSRC}/imbe7200x4400.c
    ${LIBMBELIBSRC}/mbelib.c
)

set(mbelib_HEADERS
    ${LIBMBELIBSRC}/ambe3600x2400_const.h
    ${LIBMBELIBSRC}/ambe3600x2450_const.h
    ${LIBMBELIBSRC}/ecc_const.h
    ${LIBMBELIBSRC}/imbe7200x4400_const.h
    ${LIBMBELIBSRC}/mbelib.h
    ${LIBMBELIBSRC}/mbelib_const.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBMBELIBSRC}
)

add_definitions(-DQT_SHARED)

add_library(mbelib SHARED
    ${mbelib_SOURCES}
)

target_link_libraries(mbelib
)

install(TARGETS mbelib DESTINATION lib)
