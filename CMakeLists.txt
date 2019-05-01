cmake_minimum_required(VERSION 3.1.0)

# just to be sure that c++11 if fully supported
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9")
    message(FATAL_ERROR "SDRangel requires GCC version 4.9 or higher!")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

project(sdrangel)

# disable only when needed
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# declare build structures
# !! change sdrbase/plugin/pluginmanager.cpp too !!
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(BUILD_PLUGINS_DIR ${CMAKE_BINARY_DIR}/lib/plugins)
set(BUILD_PLUGINSSRV_DIR ${CMAKE_BINARY_DIR}/lib/pluginssrv)
set(INSTALL_BIN_DIR "bin/")
set(INSTALL_LIB_DIR "lib/sdrangel")
set(INSTALL_PLUGINS_DIR ${INSTALL_LIB_DIR}/plugins)
set(INSTALL_PLUGINSSRV_DIR ${INSTALL_LIB_DIR}/pluginssrv)

set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}")

# SDRAngel cmake options
option(DEBUG_OUTPUT "Print debug messages" OFF)
option(SANITIZE_ADDRESS "Activate memory address sanitization" OFF)
option(RX_SAMPLE_24BIT "Internal 24 bit Rx DSP" ON)
option(BUILD_SERVER "Build Server" ON)
option(BUILD_GUI "Build GUI" ON)
option(FORCE_SSSE3 "Compile with SSSE3 instruction only" OFF)
option(FORCE_SSE41 "Compile with SSE4.1 instruction only" OFF)
option(BUILD_EXTERNAL_LIBRARIES "Build external libraries" OFF)
option(ENABLE_AIRSPY "Enable AirSpy support" ON)
option(ENABLE_AIRSPYHF "Enable AirSpyHF support" ON)
option(ENABLE_BLADERF "Enable bladeRF support" ON)
option(ENABLE_FUNCUBE "Enable FUNcube support" ON)
option(ENABLE_HACKRF "Enable HackRF support" ON)
option(ENABLE_IIO "Enable liniio support like PlutoSDR" ON)
option(ENABLE_LIMESUITE "Enable limesuite support" ON)
option(ENABLE_MIRISDR "Enable LibMiri for old SDRPlay" ON)
option(ENABLE_PERSEUS "Enable perseus support" ON)
option(ENABLE_RTLSDR "Enable rtl-sdr support" ON)
option(ENABLE_SOAPYSDR "Enable SoapySDR support" ON)
option(ENABLE_XTRX "Enable XTRX support" ON)

if(NOT DEBUG_OUTPUT)
    add_definitions("-DQT_NO_DEBUG_OUTPUT")
endif()

# set compiler
include(FindCompiler)

# find cpu flags (and set compiler)
include(FindCPUflags)

# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)

# Create code from a list of Qt designer ui files
set(CMAKE_AUTOUIC ON)

# https://cmake.org/cmake/help/v3.0/manual/cmake-qt.7.html#autorcc
set(CMAKE_AUTORCC TRUE)

# As moc files are generated in the binary dir,
# tell CMake to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Qt requirements
find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt5 COMPONENTS Multimedia REQUIRED)
find_package(Qt5 COMPONENTS MultimediaWidgets REQUIRED)

# for the server we don't need OpenGL components
if (BUILD_GUI)
    set(QT_USE_QTOPENGL TRUE)
    find_package(OpenGL REQUIRED)
    find_package(Qt5 COMPONENTS OpenGL REQUIRED)
endif()

# other requirements
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED)
find_package(FFTW3F REQUIRED)
find_package(Codec2)
find_package(LibUSB)
find_package(OpenCV) # channeltx/modatv

# !! TODO think how to manage build/link external libraries
if (NOT BUILD_EXTERNAL_LIBRARIES)
    find_package(LibDSDcc)
    find_package(LibMbe)
    find_package(SerialDV)
    find_package(CM256cc)
endif()

# macOS compatibility
if(APPLE)
    find_package(ICONV)
endif()

# Devices
if(ENABLE_AIRSPY)
    find_package(LibAIRSPY)
endif()
if(ENABLE_AIRSPYHF)
    find_package(LibAIRSPYHF)
endif()
if(ENABLE_BLADERF)
    find_package(LibBLADERF)
endif()
if(ENABLE_HACKRF)
    find_package(LibHACKRF)
endif()
if(ENABLE_LIMESUITE)
    find_package(LimeSuite)
endif()
if(ENABLE_IIO) # PlutoSDR
    find_package(LibIIO)
endif()
if(ENABLE_MIRISDR)
    find_package(LibMiriSDR)
endif()
if(ENABLE_PERSEUS)
    find_package(LibPerseus)
endif()
if(ENABLE_RTLSDR)
    find_package(LibRTLSDR)
endif()
if(ENABLE_SOAPYSDR)
    find_package(SoapySDR)
endif()
if(ENABLE_XTRX)
    find_package(LibXTRX)
endif()

# enable 24 bit receiving path
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

if (C_CLANG OR C_GCC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wvla -Woverloaded-virtual -ffast-math -ftree-vectorize ${EXTRA_FLAGS}")
elseif (C_MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W3 -MP ${EXTRA_FLAGS}")
endif()

if (C_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ferror-limit=1")
elseif (C_GCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmax-errors=1")
endif()

########## version configuration ############
## need more work
execute_process(
    COMMAND git describe --abbrev=8 --always --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions("-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}")

configure_file(
    ${CMAKE_SOURCE_DIR}/custom/version.h.in
    ${CMAKE_BINARY_DIR}/generated/sdrangel_version.h
)

########### include sub-projects ############
if(BUILD_EXTERNAL_LIBRARIES)
    add_subdirectory(external/cm256cc)
    add_subdirectory(external/mbelib)
    add_subdirectory(external/serialdv)
    add_subdirectory(external/dsdcc)
    add_subdirectory(external/libairspy)
    add_subdirectory(external/libairspyhf)
    add_subdirectory(external/libhackrf)
    add_subdirectory(external/librtlsdr)
    add_subdirectory(external/libbladerf)
    add_subdirectory(external/liblimesuite)
    add_subdirectory(external/libmirisdr)
    add_subdirectory(external/libperseus)
    add_subdirectory(external/libiio)
    add_subdirectory(external/libsoapysdr)
endif()

if (CODEC2_FOUND)
    add_subdirectory(libfreedv)
endif(CODEC2_FOUND)

if(ENABLE_FUNCUBE AND UNIX AND LIBUSB_FOUND)
    add_subdirectory(fcdlib)
    add_subdirectory(fcdhid)
endif()

# base libraries
add_subdirectory(sdrbase)
add_subdirectory(sdrbench)
add_subdirectory(httpserver)
add_subdirectory(logging)
add_subdirectory(qrtplib)
add_subdirectory(swagger)
add_subdirectory(devices)

if (BUILD_GUI)
    add_subdirectory(sdrgui)
    add_subdirectory(plugins)
endif()

if (BUILD_SERVER)
    add_subdirectory(sdrsrv)
    set(SERVER_MODE ON)
    add_definitions(-DSERVER_MODE)
    add_subdirectory(pluginssrv)
    remove_definitions(-DSERVER_MODE)
    set(SERVER_MODE OFF)
endif()

# includes needed by the following target
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/generated
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrgui
    ${CMAKE_SOURCE_DIR}/sdrsrv
    ${CMAKE_SOURCE_DIR}/sdrbench
    ${CMAKE_SOURCE_DIR}/logging
    ${OPENGL_INCLUDE_DIR}
)

############ build sdrangel benchmark ################
set(sdrangelbench_SOURCES
    appbench/main.cpp
)

add_executable(sdrangelbench
    ${sdrangelbench_SOURCES}
)

target_link_libraries(sdrangelbench
    Qt5::Multimedia
    sdrbench
    logging
)

############ build sdrangel gui ################
if (BUILD_GUI)
    set(sdrangel_SOURCES
        app/main.cpp
        sdrgui/resources/sdrangel.rc
    )

    add_executable(sdrangel
        ${sdrangel_SOURCES}
    )

    target_link_libraries(sdrangel
        ${OPENGL_LIBRARIES}
        Qt5::Widgets
        Qt5::Multimedia
        sdrbase
        sdrgui
        logging

    )
endif()

############ build sdrangel server ################
if (BUILD_SERVER)
    set(sdrangelsrv_SOURCES
        appsrv/main.cpp
    )

    add_executable(sdrangelsrv
        ${sdrangelsrv_SOURCES}
    )

    target_link_libraries(sdrangelsrv
        Qt5::Multimedia
        sdrbase
        sdrsrv
        logging
    )
endif()

############ install targets ################
install(TARGETS sdrangelbench DESTINATION ${INSTALL_BIN_DIR})
if (BUILD_GUI)
install(TARGETS sdrangel DESTINATION ${INSTALL_BIN_DIR})
endif()
if (BUILD_SERVER)
install(TARGETS sdrangelsrv DESTINATION ${INSTALL_BIN_DIR})
endif()

#install files and directories (linux specific)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(DIRECTORY udev-rules DESTINATION share/sdrangel)
install(FILES udev-rules/install.sh DESTINATION share/sdrangel/udev-rules PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES desktop/sdrangel.desktop DESTINATION share/applications)
install(FILES desktop/sdrangel_icon.png DESTINATION share/pixmaps)
endif()

############ uninstall target ################
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/include/uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P
    ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)
