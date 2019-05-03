cmake_minimum_required(VERSION 3.1.0)

project(sdrangel)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

# disable only when needed
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# For some external project macros
include(ExternalProject)

# configure version
set(sdrangel_VERSION_MAJOR "4")
set(sdrangel_VERSION_MINOR "6")
set(sdrangel_VERSION_PATCH "0")
set(sdrangel_VERSION_SUFFIX "")

# Set additional project information
set(COMPANY "f4exb")
add_definitions("-DCOMPANY=\"${COMPANY}\"")
set(APPLICATION_NAME "SDRangel")
add_definitions("-DAPPLICATION_NAME=\"${APPLICATION_NAME}\"")
set(APPLICATION_MAINTAINER="Edouard Griffiths")
set(COPYRIGHT "Copyright (c) 2019 Edouard Griffiths. All rights reserved.")
add_definitions("-DCOPYRIGHT=\"${COPYRIGHT}\"")
set(IDENTIFIER "com.${COMPANY}.${APPLICATION_NAME}")
add_definitions("-DIDENTIFIER=\"${IDENTIFIER}\"")

find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git/")
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE res
    OUTPUT_VARIABLE out
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT res)
        set(sdrangel_VERSION_SUFFIX "-${out}")
    endif()
endif()

set(sdrangel_VERSION "${sdrangel_VERSION_MAJOR}.${sdrangel_VERSION_MINOR}.${sdrangel_VERSION_PATCH}${sdrangel_VERSION_SUFFIX}")
message(STATUS "${APPLICATION_NAME} Version: ${sdrangel_VERSION}")
add_definitions("-DSDRANGEL_LIB_VERSION=\"${sdrangel_VERSION_MAJOR}.${sdrangel_VERSION_MINOR}.${sdrangel_VERSION_PATCH}\"")
add_definitions("-DSDRANGEL_VERSION=\"${sdrangel_VERSION}\"")

# configure CPACK infomation
set(CPACK_PACKAGE_NAME "${APPLICATION_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SDR Rx/Tx software for Airspy, Airspy HF+, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube")
set(CPACK_PACKAGE_VENDOR "${COMPANY}")
set(CPACK_PACKAGE_CONTACT "https://github.com/f4exb/sdrangel")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/Readme.md")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${VERSION}")
set(CPACK_PACKAGE_VERSION "${sdrangel_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR ${sdrangel_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${sdrangel_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${sdrangel_VERSION_PATCH})
set(CPACK_SOURCE_STRIP_FILES true)
set(CPACK_STRIP_FILES true)
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${sdrangel_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/Readme.md")
set(CPACK_SOURCE_IGNORE_FILES "${PROJECT_BINARY_DIR};/.git/;.gitignore;menu.yml;.travis.yml;.appveyor.yml;default.nix;.envrc;TODOs.org;/.scripts/")
set(CPACK_SOURCE_GENERATOR "ZIP;TGZ")
set(CPACK_GENERATOR "ZIP;TGZ")

# quite unfair with *BSD/...
if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# SDRAngel cmake options
option(DEBUG_OUTPUT "Print debug messages" OFF)
option(SANITIZE_ADDRESS "Activate memory address sanitization" OFF)
option(RX_SAMPLE_24BIT "Internal 24 bit Rx DSP" ON)
option(BUILD_SERVER "Build Server" ON)
option(BUILD_GUI "Build GUI" ON)
option(BUNDLE "Enable distribution bundle" OFF)
option(FORCE_SSSE3 "Compile with SSSE3 instruction only" OFF)
option(FORCE_SSE41 "Compile with SSE4.1 instruction only" OFF)
option(ENABLE_EXTERNAL_LIBRARIES "Build external libraries" OFF)
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

# if we don't set build_type
if(NOT DEFINED CMAKE_BUILD_TYPE OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type set to ${CMAKE_BUILD_TYPE}")

# as default disable qDebug()
if(NOT DEBUG_OUTPUT)
  add_definitions("-DQT_NO_DEBUG_OUTPUT")
endif()

# declare build structures
# !! change sdrbase/plugin/pluginmanager.cpp too !!
if(WIN32)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/)
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(BUILD_PLUGINS_DIR ${CMAKE_BINARY_DIR}/lib/plugins)
set(BUILD_PLUGINSSRV_DIR ${CMAKE_BINARY_DIR}/lib/pluginssrv)
set(INSTALL_BIN_DIR "bin/")
# use only lib when create .app/.dmg
if(APPLE AND BUNDLE AND BUILD_GUI)
  set(INSTALL_LIB_DIR "lib")
else()
  set(INSTALL_LIB_DIR "lib/${CMAKE_PROJECT_NAME}")
endif()
set(INSTALL_PLUGINS_DIR ${INSTALL_LIB_DIR}/plugins)
set(INSTALL_PLUGINSSRV_DIR ${INSTALL_LIB_DIR}/pluginssrv)
set(EXTERNAL_BUILD_LIBRARIES "${CMAKE_BINARY_DIR}/external_build")
set(EXTERNAL_INSTALL_LIBRARIES "${CMAKE_BINARY_DIR}/external")

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET, "10.9")
  set(CMAKE_MACOSX_RPATH ON)
endif()

if (WIN32)
  set(EXTERNAL_LIBRARY_FOLDER "${CMAKE_SOURCE_DIR}/external/windows")
  set(BOOST_ROOT "${EXTERNAL_LIBRARY_FOLDER}/boot")
  set(FFTW3F_INCLUDE_DIRS="${EXTERNAL_LIBRARY_FOLDER}/fftw-3/include")
  set(FFTW3F_LIBRARIES="${EXTERNAL_LIBRARY_FOLDER}/fftw-3/libfftw3f-3.dll")
  set(LIBUSB_INCLUDE_DIR="${EXTERNAL_LIBRARY_FOLDER}/libusb/include")
  set(LIBUSB_LIBRARIES="${EXTERNAL_LIBRARY_FOLDER}/libusb/libusb-1.0.x64.dll")
  set(OpenCV_INCLUDE_DIRS="${EXTERNAL_LIBRARY_FOLDER}/opencv/include")
  set(OpenCV_LIBS="${EXTERNAL_LIBRARY_FOLDER}/opencv/opencv_ffmpeg410_64.dll")
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
# TODO check minimum version
set(_required_qt_version "5.6.0")
find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt5 COMPONENTS Multimedia REQUIRED)
find_package(Qt5 COMPONENTS MultimediaWidgets REQUIRED)

# for the server we don't need OpenGL components
if (BUILD_GUI)
    find_package(OpenGL REQUIRED)
    find_package(Qt5 COMPONENTS OpenGL REQUIRED)
endif()

# other requirements
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED)
find_package(FFTW3F REQUIRED)
find_package(LibUSB REQUIRED)  # used by so many packages
find_package(OpenCV) # channeltx/modatv

# macOS compatibility
if(APPLE)
    find_package(ICONV)
endif()

# include external cmake if needed
if(ENABLE_EXTERNAL_LIBRARIES)
    add_subdirectory(external)
endif()

# after external libraries
find_package(LibDSDcc)
find_package(LibMbe)
find_package(SerialDV)
find_package(CM256cc)
find_package(Codec2)

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
    find_package(SoapySDR 0.4.0)
endif()
if(ENABLE_XTRX)
    find_package(LibXTRX)
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

############ install/package targets ################
if(APPLE AND BUNDLE AND BUILD_GUI)

  # https://cmake.org/cmake/help/v3.0/module/InstallRequiredSystemLibraries.html
  #CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS

  install(TARGETS sdrangelbench)
  if (BUILD_SERVER)
    install(TARGETS sdrangelsrv)
  endif()

  #include(DeployQt)
  #macdeployqt(sdrangel)

  include(InstallRequiredSystemLibraries)

  configure_file("${CMAKE_SOURCE_DIR}/custom/desktop/MacOSXBundleInfo.plist.in"
    "${CMAKE_BINARY_DIR}/Info.plist" @ONLY)

  set(CPACK_GENERATOR "Bundle")
  set(CPACK_BINARY_DRAGNDROP ON)
  set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/custom/desktop/sdrangel_icon.icns")
  set(CPACK_BUNDLE_NAME "${APPLICATION_NAME}")
  set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/custom/desktop/sdrangel_icon.icns")
  set(CPACK_BUNDLE_PLIST "${CMAKE_BINARY_DIR}/Info.plist")
  set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}")
  #set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};lib;/")

  set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/custom/desktop/sdrangel_icon.icns")

elseif(LINUX AND BUNDLE)

  # see https://gitlab.kitware.com/cmake/community/wikis/doc/cpack/PackageGenerators
  list(APPEND CPACK_GENERATOR "DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${APPLICATION_MAINTAINER}")
  set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12)")

  list(APPEND CPACK_GENERATOR "RPM")
  set(CPACK_RPM_PACKAGE_RELEASE "1")
  set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0")
  set(CPACK_RPM_PACKAGE_REQUIRES "libusb")

elseif((WIN32 OR MINGW) AND BUNDLE)

  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)

  list(APPEND CPACK_GENERATOR "NSIS")
  set(CPACK_NSIS_PACKAGE_NAME "${APPLICATION_NAME}")
  set(CPACK_NSIS_CONTACT "${APPLICATION_MAINTAINER}")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MODIFY_PATH ON)

  # from https://blog.nathanosman.com/2017/11/24/using-windeployqt-with-cpack.html
  # DeployQt.cmake

else()

  if(NOT CMAKE_INSTALL_RPATH)
    set(CMAKE_INSTALL_RPATH
      ${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR} CACHE
      PATH "Library Install RPath" FORCE)
  endif()

  # force full path
  if(APPLE AND NOT CMAKE_INSTALL_NAME_DIR)
    set(CMAKE_INSTALL_NAME_DIR
      ${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR} CACHE
      PATH "Library Install Name Destination Directory" FORCE)
  endif()

  install(TARGETS sdrangelbench DESTINATION ${INSTALL_BIN_DIR})
  if (BUILD_GUI)
  install(TARGETS sdrangel DESTINATION ${INSTALL_BIN_DIR})
  endif()
  if (BUILD_SERVER)
    install(TARGETS sdrangelsrv DESTINATION ${INSTALL_BIN_DIR})
  endif()

  #install files and directories (linux specific)
  if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # TODO remove useless udev
    #install(DIRECTORY custom/udev-rules DESTINATION share/sdrangel)
    #install(FILES custom/udev-rules/install.sh DESTINATION share/sdrangel/udev-rules PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    install(FILES custom/desktop/sdrangel.desktop DESTINATION share/applications)
    install(FILES custom/desktop/sdrangel_icon.png DESTINATION share/pixmaps)
  endif()

endif()

message(STATUS "CPack generators: ${CPACK_GENERATOR}")
include(CPack)

############ uninstall target ################
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/include/uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -P
  ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)
