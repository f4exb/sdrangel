cmake_minimum_required(VERSION 3.1.0)
cmake_policy(SET CMP0043 OLD)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
    message(FATAL_ERROR "SDRangel requires GCC version 4.9 or higher!")
endif()

# QT Framework
set(CMAKE_PREFIX_PATH "/Applications/Qt/5.7/clang_64/lib/cmake")

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
option(HOST_RPI "Compiling on RPi" OFF)
option(RX_SAMPLE_24BIT "Internal 24 bit Rx DSP" OFF)

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

#find_package(Qt4 REQUIRED)
find_package(Qt5Core 5.0 REQUIRED)
find_package(Qt5Widgets 5.0 REQUIRED)
find_package(Qt5Multimedia 5.0 REQUIRED)
#find_package(QT5OpenGL 5.0 REQUIRED)
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

EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
message( STATUS "Architecture: ${ARCHITECTURE}" )

if (${ARCHITECTURE} MATCHES "x86_64|AMD64|x86")
    EXECUTE_PROCESS( COMMAND grep flags /proc/cpuinfo OUTPUT_VARIABLE CPU_FLAGS )
#    if (${CPU_FLAGS} MATCHES "avx2")
#        set(HAS_AVX2 ON CACHE BOOL "Architecture has AVX2 SIMD enabled")
#        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
#            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx2" )
#            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mavx2" )
#            message(STATUS "Use AVX2 SIMD instructions")
#            add_definitions(-DUSE_AVX2)
#    else()
#        set(HAS_AVX2 OFF CACHE BOOL "Architecture does not have AVX2 SIMD enabled")
#        endif()
#    endif()
    if (${CPU_FLAGS} MATCHES "sse4_1")
        set(HAS_SSE4_1 ON CACHE BOOL "Architecture has SSE 4.1 SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
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
    if (${CPU_FLAGS} MATCHES "ssse3")
        set(HAS_SSSE3 ON CACHE BOOL "Architecture has SSSE3 SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mssse3" )
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mssse3" )
            message(STATUS "Use SSSE3 SIMD instructions")
            add_definitions(-DUSE_SSSE3)
        elseif(MSVC)
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSSE3" )
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSSE3" )
            set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
            add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
            add_definitions(-DUSE_SSSE3)
        endif()
    else()
        set(HAS_SSSE3 OFF CACHE BOOL "Architecture does not have SSSE3 SIMD enabled")
    endif()
    if (${CPU_FLAGS} MATCHES "sse2")
        set(HAS_SSE2 ON CACHE BOOL "Architecture has SSE2 SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse2" )
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -msse2" )
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
        set(HAS_SSE2 OFF CACHE BOOL "Architecture does not have SSE2 SIMD enabled")
    endif()
elseif (${ARCHITECTURE} MATCHES "armv7l")
    EXECUTE_PROCESS( COMMAND grep Features /proc/cpuinfo OUTPUT_VARIABLE CPU_FLAGS )
    if (${CPU_FLAGS} MATCHES "neon")
        set(HAS_NEON ON CACHE BOOL "Architecture has NEON SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpu=neon" )
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mfpu=neon" )
            message(STATUS "Use NEON SIMD instructions")
            add_definitions(-DUSE_NEON)
        endif()
    else()
        set(HAS_NEON OFF CACHE BOOL "Architecture does not have NEON SIMD enabled")
    endif()
elseif (${ARCHITECTURE} MATCHES "aarch64")
    set(HAS_NEON ON CACHE BOOL FORCE "Architecture has NEON SIMD enabled")
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        message(STATUS "Aarch64 always has NEON SIMD instructions")
        add_definitions(-DUSE_NEON)
    endif()
endif()

# Compiler flags.
if (RX_SAMPLE_24BIT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSDR_RX_SAMPLE_24BIT")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fmax-errors=10 -ffast-math -ftree-vectorize ${EXTRA_FLAGS}")

##############################################################################
# base libraries
add_subdirectory(sdrbase)
add_subdirectory(sdrgui)
add_subdirectory(sdrsrv)
add_subdirectory(httpserver)
add_subdirectory(logging)
add_subdirectory(swagger)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/sdrgui
    ${CMAKE_SOURCE_DIR}/logging
    ${OPENGL_INCLUDE_DIR}
)

##############################################################################
# main GUI application

set(sdrangel_SOURCES
    app/main.cpp
)

if(WIN32)
    SET(sdrangel_SOURCES ${sdrangel_SOURCES} sdrbase/resources/sdrangel.rc)
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
    set_target_properties(sdrangel PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
    set_target_properties(sdrangel PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
    set_target_properties(sdrangel PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
    set_target_properties(sdrangel PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
    set_target_properties(sdrangel PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
endif(WIN32)

qt5_use_modules(sdrangel Widgets Multimedia)

##############################################################################
# main server application

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

qt5_use_modules(sdrangelsrv Multimedia)

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
    add_subdirectory(libmirisdr)
    add_subdirectory(liblimesuite)
    add_subdirectory(libiio)
endif (BUILD_DEBIAN)

add_subdirectory(devices)
add_subdirectory(plugins)
add_subdirectory(pluginssrv)

if(LIBUSB_FOUND AND UNIX)
    add_subdirectory(fcdhid)
    add_subdirectory(fcdlib)
endif(LIBUSB_FOUND AND UNIX)

##############################################################################

#install targets
install(TARGETS sdrangel DESTINATION bin)
install(TARGETS sdrangelsrv DESTINATION bin)
#install(TARGETS sdrbase DESTINATION lib)

#install files and directories
install(DIRECTORY udev-rules DESTINATION share/sdrangel)
install(FILES udev-rules/install.sh DESTINATION share/sdrangel/udev-rules PERMISSIONS WORLD_EXECUTE)

##############################################################################

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
