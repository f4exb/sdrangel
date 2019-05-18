cmake_minimum_required(VERSION 3.1.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
    message(FATAL_ERROR "SDRangel requires GCC version 4.9 or higher!")
endif()

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_definitions(-DLINUX) # for compatibility with Android and Windows builds with QtCretino

option(V4L-RTL "Use Linux Kernel RTL-SDR Source." OFF)
option(V4L-MSI "Use Linux Kernel MSI2500 Source." OFF)
option(BUILD_TYPE "Build type (RELEASE, RELEASEWITHDBGINFO, DEBUG" RELEASE)
option(DEBUG_OUTPUT "Print debug messages" OFF)
option(SANITIZE_ADDRESS "Activate memory address sanitization" OFF)
option(HOST_RPI "Compiling on RPi" OFF)
option(RX_SAMPLE_24BIT "Internal 24 bit Rx DSP" OFF)
option(NO_DSP_SIMD "Do not use SIMD instructions for DSP even if available" OFF)
option(BUILD_SERVER "Build Server" ON)
option(BUILD_GUI "Build GUI" ON)
option(ENABLE_DISTRIBUTION "Compile for package distribution" OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

project(sdrangel)

if (BUILD_TYPE MATCHES RELEASE)
    set(CMAKE_BUILD_TYPE "Release")
elseif (BUILD_TYPE MATCHES RELEASEWITHDBGINFO)
    set(CMAKE_BUILD_TYPE "ReleaseWithDebugInfo")
elseif (BUILD_TYPE MATCHES DEBUG)
    set(CMAKE_BUILD_TYPE "Debug")
elseif (BUILD_TYPE MATCHES DEBIAN)
    set(CMAKE_BUILD_TYPE "Release")
    set(BUILD_DEBIAN TRUE)
else()
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(QT_USE_QTOPENGL TRUE)
set(CMAKE_AUTOMOC ON)

if(APPLE AND EXISTS /usr/local/opt/qt5)
       # Homebrew installs Qt5 (up to at least 5.9.1) in
       # /usr/local/qt5, ensure it can be found by CMake since
       # it is not in the default /usr/local prefix.
       list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/qt5")
endif()

#find_package(Qt4 REQUIRED)
find_package(Qt5Core 5.0 REQUIRED)
find_package(Qt5Widgets 5.0 REQUIRED)
find_package(Qt5Multimedia 5.0 REQUIRED)
find_package(Qt5OpenGL 5.0 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig)
find_package(Boost REQUIRED)
find_package(FFTW3F)

if (NOT BUILD_DEBIAN)
    find_package(LibDSDcc)
    find_package(LibMbe)
    find_package(SerialDV)
endif()

# MacOS Compatibility
if(APPLE)
        find_package(ICONV)
endif(APPLE)
##############################################################################

#include(${QT_USE_FILE})

if (DEBUG_OUTPUT)
    set( QT_DEFINITIONS "${QT_DEFINITIONS}" )
else()
    set( QT_DEFINITIONS "${QT_DEFINITIONS} -DQT_NO_DEBUG_OUTPUT" )
endif()

add_definitions(${QT_DEFINITIONS})

if(MSVC)
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin_${OUTPUTCONFIG})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin_${OUTPUTCONFIG})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin_${OUTPUTCONFIG})
    endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)
endif()

##############################################################################

if (HOST_RPI)
    message( STATUS "Compiling on RPi" )
endif()


set(TEST_DIR ${PROJECT_SOURCE_DIR}/cmake/test)

# Clang or AppleClang (see CMP0025)
if(NOT DEFINED C_CLANG AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(C_CLANG 1)
endif()
if(NOT DEFINED C_GCC AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(C_GCC 1)
endif()

# Detect current compilation architecture and create standard definitions
# =======================================================================
include(CheckSymbolExists)
function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE)
            add_definitions(-DARCHITECTURE_${arch}=1)
        endif()
    endif()
endfunction()

if (NOT ENABLE_GENERIC)
    if (MSVC)
        detect_architecture("_M_AMD64" x86_64)
        detect_architecture("_M_IX86" x86)
        detect_architecture("_M_ARM" ARM)
        detect_architecture("_M_ARM64" ARM64)
    else()
        detect_architecture("__x86_64__" x86_64)
        detect_architecture("__i386__" x86)
        detect_architecture("__arm__" ARM)
        detect_architecture("__aarch64__" ARM64)
    endif()
endif()
if (NOT DEFINED ARCHITECTURE)
    set(ARCHITECTURE "GENERIC")
    set(ARCHITECTURE_GENERIC 1)
    add_definitions(-DARCHITECTURE_GENERIC=1)
endif()
message(STATUS "Target architecture: ${ARCHITECTURE}")

# flag that set the minimum cpu flag requirements
# used to create re-distribuitable binary
if (ENABLE_DISTRIBUTION)
    if (${ARCHITECTURE} MATCHES "x86_64|x86")
        set(HAS_SSSE3 ON CACHE BOOL "SSSE3 SIMD enabled")
        if(C_GCC OR C_CLANG)
            set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3" )
            message(STATUS "Use SSSE3 SIMD instructions")
            add_definitions(-DUSE_SSSE3)
        elseif(MSVC)
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSSE3" )
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSSE3" )
            set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
            message(STATUS "Use MSVC SSSE3 SIMD instructions")
            add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
            add_definitions(-DUSE_SSSE3)
        endif()
    elseif (${ARCHITECTURE} MATCHES "ARM|ARM64")
        set(HAS_NEON ON CACHE BOOL "NEON SIMD enabled")
        message(STATUS "Use NEON SIMD instructions")
        add_definitions(-DUSE_NEON)
    endif()
else ()
if (${ARCHITECTURE} MATCHES "x86_64|x86")
    try_run(RUN_SSE2 COMPILE_SSE2 ${CMAKE_BINARY_DIR}/tmp ${TEST_DIR}/test_x86_sse2.cxx COMPILE_DEFINITIONS -msse2 -O0)
    if(COMPILE_SSE2 AND RUN_SSE2 EQUAL 0)
       set(HAS_SSE2 ON CACHE BOOL "Architecture has SSSE2 SIMD enabled")
       if(C_GCC OR C_CLANG)
           set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2" )
           message(STATUS "Use SSE2 SIMD instructions")
           add_definitions(-DUSE_SSE2)
       elseif(MSVC)
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSE2" )
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSE2" )
           set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
           add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
           add_definitions(-DUSE_SSE2)
       endif()
    else()
       set(HAS_SSE2 OFF CACHE BOOL "Architecture does not have SSSE2 SIMD enabled")
    endif()
    try_run(RUN_SSSE3 COMPILE_SSSE3 ${CMAKE_BINARY_DIR}/tmp ${TEST_DIR}/test_x86_ssse3.cxx COMPILE_DEFINITIONS -mssse3 -O0)
    if(COMPILE_SSSE3 AND RUN_SSSE3 EQUAL 0)
       set(HAS_SSSE3 ON CACHE BOOL "Architecture has SSSE3 SIMD enabled")
       if(C_GCC OR C_CLANG)
           set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3" )
           message(STATUS "Use SSSE3 SIMD instructions")
           add_definitions(-DUSE_SSSE3)
       elseif(MSVC)
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSSE3" )
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSSE3" )
           set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
           message(STATUS "Use MSVC SSSE3 SIMD instructions")
           add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
           add_definitions(-DUSE_SSSE3)
       endif()
    else()
       set(HAS_SSSE3 OFF CACHE BOOL "Architecture does not have SSSE3 SIMD enabled")
    endif()
    try_run(RUN_SSE4_1 COMPILE_SSE4_1 ${CMAKE_BINARY_DIR}/tmp ${TEST_DIR}/test_x86_sse41.cxx COMPILE_DEFINITIONS -msse4.1 -O0)
    if(COMPILE_SSE4_1 AND RUN_SSE4_1 EQUAL 0)
       set(HAS_SSE4_1 ON CACHE BOOL "Architecture has SSE 4.1 SIMD enabled")
       if(C_GCC OR C_CLANG)
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse4.1" )
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -msse4.1" )
           message(STATUS "Use SSE 4.1 SIMD instructions")
           add_definitions(-DUSE_SSE4_1)
       elseif(MSVC)
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSE4_1" )
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSE4_1" )
           set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
           add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
           add_definitions(-DUSE_SSE4_1)
       endif()
    else()
       set(HAS_SSE4_1 OFF CACHE BOOL "Architecture does not have SSE 4.1 SIMD enabled")
    endif()
    try_run(RUN_SSE4_2 COMPILE_SSE4_2 ${CMAKE_BINARY_DIR}/tmp ${TEST_DIR}/test_x86_sse42.cxx COMPILE_DEFINITIONS -msse4.2 -O0)
    if(COMPILE_SSE4_2 AND RUN_SSE4_2 EQUAL 0)
       set(HAS_SSE4_2 ON CACHE BOOL "Architecture has SSE 4.2 SIMD enabled")
       if(C_GCC OR C_CLANG)
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse4.2" )
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -msse4.2" )
           message(STATUS "Use SSE 4.2 SIMD instructions")
           add_definitions(-DUSE_SSE4_2)
       elseif(MSVC)
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSE4_2" )
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSE4_2" )
           set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
           add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
           add_definitions(-DUSE_SSE4_2)
       endif()
    else()
       set(HAS_SSE4_2 OFF CACHE BOOL "Architecture does not have SSE 4.2 SIMD enabled")
    endif()
    try_run(RUN_AVX COMPILE_AVX ${CMAKE_BINARY_DIR}/tmp ${TEST_DIR}/test_x86_avx.cxx COMPILE_DEFINITIONS -mavx -O0)
    if(COMPILE_AVX AND RUN_AVX EQUAL 0)
       set(HAS_AVX ON CACHE BOOL "Architecture has AVX SIMD enabled")
       if(C_GCC OR C_CLANG)
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx" )
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mavx" )
           message(STATUS "Use AVX SIMD instructions")
           add_definitions(-DUSE_AVX)
       endif()
    else()
       set(HAS_AVX OFF CACHE BOOL "Architecture does not have AVX SIMD enabled")
    endif()
    try_run(RUN_AVX2 COMPILE_AVX2 ${CMAKE_BINARY_DIR}/tmp ${TEST_DIR}/test_x86_avx2.cxx COMPILE_DEFINITIONS -mavx2 -O0)
    if(COMPILE_AVX2 AND RUN_AVX2 EQUAL 0)
       set(HAS_AVX2 ON CACHE BOOL "Architecture has AVX2 SIMD enabled")
       if(C_GCC OR C_CLANG)
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx2" )
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mavx2" )
           message(STATUS "Use AVX2 SIMD instructions")
           add_definitions(-DUSE_AVX2)
       endif()
    else()
       set(HAS_AVX2 OFF CACHE BOOL "Architecture does not have AVX2 SIMD enabled")
    endif()
    try_run(RUN_AVX512 COMPILE_AVX512 ${CMAKE_BINARY_DIR}/tmp ${TEST_DIR}/test_x86_avx512.cxx COMPILE_DEFINITIONS -mavx512f -O0)
    if(COMPILE_AVX512 AND RUN_AVX512 EQUAL 0)
       set(HAS_AVX512 ON CACHE BOOL "Architecture has AVX512 SIMD enabled")
       if(C_GCC OR C_CLANG)
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx512f" )
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mavx512f" )
           message(STATUS "Use AVX512 SIMD instructions")
           add_definitions(-DUSE_AVX512)
       endif()
    else()
       set(HAS_AVX512 OFF CACHE BOOL "Architecture does not have AVX512 SIMD enabled")
    endif()
elseif(ARCHITECTURE_ARM)
    try_run(RUN_NEON COMPILE_NEON ${CMAKE_BINARY_DIR}/tmp ${TEST_DIR}/test_arm_neon.cxx COMPILE_DEFINITIONS -mfpu=neon -O0)
    if(COMPILE_NEON AND RUN_NEON EQUAL 0)
       set(HAS_NEON ON CACHE BOOL "Architecture has NEON SIMD enabled")
       if(C_GCC OR C_CLANG)
           set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpu=neon" )
           set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mfpu=neon" )
           message(STATUS "Use NEON SIMD instructions")
           add_definitions(-DUSE_NEON)
       endif()
    else()
       set(HAS_NEON OFF CACHE BOOL "Architecture does not have NEON SIMD enabled")
    endif()
elseif(ARCHITECTURE_ARM64)
    # Advanced SIMD (aka NEON) is mandatory for AArch64
    set(HAS_NEON ON CACHE BOOL "Architecture has NEON SIMD enabled")
    message(STATUS "Use NEON SIMD instructions")
    add_definitions(-DUSE_NEON)
endif()
endif()

# clear binary test folder
FILE(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/tmp)
##############################################################################

# Compiler flags.
if (RX_SAMPLE_24BIT)
	message(STATUS "Compiling for 24 bit Rx DSP chain")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSDR_RX_SAMPLE_24BIT")
else()
	message(STATUS "Compiling for 16 bit Rx DSP chain")
endif()

if (SANITIZE_ADDRESS)
    message(STATUS "Activate address sanitization")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -fsanitize=address")
endif()

if (NO_DSP_SIMD)
    message(STATUS "Not compiling with SIMD instructions for DSP even if available")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNO_DSP_SIMD")
else()
    message(STATUS "Compiling with SIMD instructions for DSP if available")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wvla -Woverloaded-virtual -ffast-math -ftree-vectorize ${EXTRA_FLAGS}")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W3 -MP ${EXTRA_FLAGS}")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (BUILD_DEBIAN)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmax-errors=1")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmax-errors=10")
    endif()
endif()

##############################################################################

execute_process(
    COMMAND git describe --abbrev=8 --always --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions("-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}")

configure_file(
    ${CMAKE_SOURCE_DIR}/version.h.in
    ${CMAKE_BINARY_DIR}/generated/sdrangel_version.h
)

##############################################################################
# base libraries
add_subdirectory(sdrbase)
if (BUILD_GUI)
add_subdirectory(sdrgui)
endif()
if (BUILD_SERVER)
add_subdirectory(sdrsrv)
endif()
add_subdirectory(sdrbench)
add_subdirectory(httpserver)
add_subdirectory(logging)
add_subdirectory(qrtplib)
add_subdirectory(swagger)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/generated
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/sdrgui
    ${CMAKE_SOURCE_DIR}/logging
    ${OPENGL_INCLUDE_DIR}
)

##############################################################################
# External binary resources
include(${CMAKE_SOURCE_DIR}/cmake/include/Qt5ExternalResources.cmake)

qt5_add_external_resources(sdrbase.rcc sdrbase/resources/res.qrc)


##############################################################################
# main GUI application
if (BUILD_GUI)

set(sdrangel_SOURCES
    app/main.cpp
)

if(WIN32)
    SET(sdrangel_SOURCES ${sdrangel_SOURCES} sdrgui/resources/sdrangel.rc)
endif(WIN32)

add_executable(sdrangel
    ${sdrangel_SOURCES}
)

target_link_libraries(sdrangel
    sdrbase
    sdrgui
    logging
    ${QT_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

if(WIN32)
    set_target_properties(sdrangel PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
    set_target_properties(sdrangel PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
    set_target_properties(sdrangel PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
    set_target_properties(sdrangel PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
endif(WIN32)

target_link_libraries(sdrangel Qt5::Widgets Qt5::Multimedia)
endif()

##############################################################################
# main server application
if (BUILD_SERVER)

set(sdrangelsrv_SOURCES
    appsrv/main.cpp
)

add_executable(sdrangelsrv
    ${sdrangelsrv_SOURCES}
)

target_include_directories(sdrangelsrv
    PUBLIC ${CMAKE_SOURCE_DIR}/sdrsrv
)

target_link_libraries(sdrangelsrv
    sdrbase
    sdrsrv
    logging
    ${QT_LIBRARIES}
)

target_link_libraries(sdrangelsrv Qt5::Multimedia)
endif()

##############################################################################
# main benchmark application

set(sdrangelbench_SOURCES
    appbench/main.cpp
)

add_executable(sdrangelbench
    ${sdrangelbench_SOURCES}
)

target_include_directories(sdrangelbench
    PUBLIC ${CMAKE_SOURCE_DIR}/sdrbench
)

target_link_libraries(sdrangelbench
    sdrbench
    logging
    ${QT_LIBRARIES}
)

target_compile_features(sdrangelbench PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0
target_link_libraries(sdrangelbench Qt5::Multimedia)

##############################################################################

if (BUILD_DEBIAN)
    add_subdirectory(cm256cc)
    add_subdirectory(mbelib)
    add_subdirectory(serialdv)
    add_subdirectory(dsdcc)
    add_subdirectory(libairspy)
    add_subdirectory(libairspyhf)
    add_subdirectory(libhackrf)
    add_subdirectory(librtlsdr)
    add_subdirectory(libbladerf)
    add_subdirectory(liblimesuite)
    add_subdirectory(libmirisdr)
    add_subdirectory(libperseus)
    add_subdirectory(libiio)
	add_subdirectory(libsoapysdr)
endif (BUILD_DEBIAN)

find_package(Codec2)
if (CODEC2_FOUND)
    add_subdirectory(libfreedv)
endif(CODEC2_FOUND)

add_subdirectory(devices)
if (BUILD_GUI)
add_subdirectory(plugins)
endif()
if (BUILD_SERVER)
add_subdirectory(pluginssrv)
endif()

if(LIBUSB_FOUND AND UNIX)
    add_subdirectory(fcdhid)
    add_subdirectory(fcdlib)
endif(LIBUSB_FOUND AND UNIX)

##############################################################################

#install targets
if (BUILD_GUI)
install(TARGETS sdrangel DESTINATION bin)
endif()
if (BUILD_SERVER)
install(TARGETS sdrangelsrv DESTINATION bin)
endif()
install(TARGETS sdrangelbench DESTINATION bin)
#install(TARGETS sdrbase DESTINATION lib)

#install files and directories (linux specific)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES desktop/sdrangel.desktop DESTINATION share/applications)
install(FILES desktop/sdrangel_icon.png DESTINATION share/pixmaps)
endif()

# needed by REST API documentation
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sdrbase.rcc DESTINATION bin)

##############################################################################

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
