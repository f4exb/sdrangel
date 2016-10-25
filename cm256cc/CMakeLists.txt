project(cm256cc)

set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11" )
add_definitions(-DNO_RESTRICT)

EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
message( STATUS "Architecture: ${ARCHITECTURE}" )

if(${ARCHITECTURE} MATCHES "x86_64|AMD64|x86")
    SET(USE_SIMD "SSSE3" CACHE STRING "Use SIMD SSSE3 instructions")
elseif(${ARCHITECTURE} MATCHES "armv7l")
    SET(USE_SIMD "NEON" CACHE STRING "Use SIMD NEON instructions")
endif()

if(USE_SIMD MATCHES SSSE3)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mssse3" )
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mssse3" )
        message(STATUS "g++ SSSE3")
        add_definitions(-DUSE_SIMD)
    elseif(MSVC)
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSSE3" )
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSSE3" )
        set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
        message(STATUS "MSVC SSSE3")
        add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
        add_definitions(-DUSE_SIMD)
    endif()
elseif(USE_SIMD MATCHES NEON)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpu=neon" )
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mfpu=neon" )
        message(STATUS "g++ NEON")
        add_definitions(-DUSE_NEON)
    endif()
else()
    message(STATUS "Unsupported architecture")
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

target_link_libraries(cm256cc
)

install(TARGETS cm256cc DESTINATION lib)
