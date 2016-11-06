project(cm256cc)

if (HAS_SSSE3)
    message(STATUS "SDRdaemonFEC: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "SDRdaemonFEC: use Neon SIMD" )
else()
    message(STATUS "SDRdaemonFEC: Unsupported architecture")
    return()
endif()

set(cm256cc_SOURCES
    ${LIBCM256CCSRC}/gf256.cpp
    ${LIBCM256CCSRC}/cm256.cpp
)

set(cm256cc_HEADERS
    ${LIBCM256CCSRC}/gf256.h
    ${LIBCM256CCSRC}/cm256.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBCM256CCSRC}
)

add_definitions(-DQT_SHARED)

add_library(cm256cc SHARED
    ${cm256cc_SOURCES}
)

install(TARGETS cm256cc DESTINATION lib)
