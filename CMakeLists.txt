cmake_minimum_required(VERSION 3.1.0)

# force 64bit on windows because we have only that library
# TODO discuss because, at this days, a dsp software should be 64bit
if(WIN32 AND NOT CMAKE_GENERATOR_PLATFORM AND NOT CMAKE_C_COMPILER AND NOT CMAKE_CXX_COMPILER)
  set(CMAKE_GENERATOR_PLATFORM "x64" CACHE INTERNAL "")
endif()

project(sdrangel)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

# disable only when needed
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# configure version
set(sdrangel_VERSION_MAJOR "4")
set(sdrangel_VERSION_MINOR "9")
set(sdrangel_VERSION_PATCH "1")
set(sdrangel_VERSION_SUFFIX "")

# SDRAngel cmake options
option(DEBUG_OUTPUT "Print debug messages" OFF)
option(SANITIZE_ADDRESS "Activate memory address sanitization" OFF)
option(RX_SAMPLE_24BIT "Internal 24 bit Rx DSP" ON)
option(BUILD_SERVER "Build Server" ON)
option(BUILD_GUI "Build GUI" ON)
option(BUNDLE "Enable distribution bundle" OFF)
option(FORCE_SSSE3 "Compile with SSSE3 instruction only" OFF)
option(FORCE_SSE41 "Compile with SSE4.1 instruction only" OFF)
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
option(ENABLE_PACK_MIRSDRAPI "Enable inclusion of the mirsdr-api library - for personal use only" OFF)
# on windows always build external libraries
if(WIN32)
  option(ENABLE_EXTERNAL_LIBRARIES "Build external libraries" ON)
else()
  option(ENABLE_EXTERNAL_LIBRARIES "Build external libraries" OFF)
endif()

# Set additional project information
set(COMPANY "f4exb")
add_definitions("-DCOMPANY=\"${COMPANY}\"")
set(APPLICATION_NAME "SDRangel")
add_definitions("-DAPPLICATION_NAME=\"${APPLICATION_NAME}\"")
set(APPLICATION_MAINTAINER="Edouard Griffiths, F4EXB <f4exb06@gmail.com>")
set(COPYRIGHT "Copyright (c) 2019 Edouard Griffiths. All rights reserved.")
add_definitions("-DCOPYRIGHT=\"${COPYRIGHT}\"")
set(IDENTIFIER "com.${COMPANY}.${APPLICATION_NAME}")
add_definitions("-DIDENTIFIER=\"${IDENTIFIER}\"")

find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git/")
  execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE res
    OUTPUT_VARIABLE out
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT res)
        string(REGEX REPLACE "^v([0-9]+)\.([0-9]+)\.([0-9]+)-" "" git_commit ${out})
        set(sdrangel_VERSION_SUFFIX "-${git_commit}")
    endif()
endif()

set(sdrangel_VERSION "${sdrangel_VERSION_MAJOR}.${sdrangel_VERSION_MINOR}.${sdrangel_VERSION_PATCH}${sdrangel_VERSION_SUFFIX}")
message(STATUS "${APPLICATION_NAME} Version: ${sdrangel_VERSION}")
add_definitions("-DSDRANGEL_LIB_VERSION=\"${sdrangel_VERSION_MAJOR}.${sdrangel_VERSION_MINOR}.${sdrangel_VERSION_PATCH}\"")
add_definitions("-DSDRANGEL_VERSION=\"${sdrangel_VERSION}\"")

# configure CPACK infomation
set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SDR Rx/Tx software for Airspy, Airspy HF+, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube")
set(CPACK_PACKAGE_VENDOR "${COMPANY}")
set(CPACK_PACKAGE_CONTACT "https://github.com/f4exb/sdrangel")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/Readme.md")
set(CPACK_PACKAGE_VERSION "${sdrangel_VERSION_MAJOR}.${sdrangel_VERSION_MINOR}.${sdrangel_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_MAJOR ${sdrangel_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${sdrangel_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${sdrangel_VERSION_PATCH})
set(CPACK_SOURCE_STRIP_FILES true)
set(CPACK_STRIP_FILES true)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/Readme.md")
set(CPACK_SOURCE_IGNORE_FILES "${PROJECT_BINARY_DIR};/.git/;.gitignore;menu.yml;.travis.yml;.appveyor.yml;default.nix;.envrc;TODOs.org;/.scripts/")
set(CPACK_SOURCE_GENERATOR "ZIP;TGZ")

# if we don't set build_type
if(NOT DEFINED CMAKE_BUILD_TYPE OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
      set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type set to: ${CMAKE_BUILD_TYPE}")

# as default disable qDebug()
if(NOT DEBUG_OUTPUT)
  add_definitions("-DQT_NO_DEBUG_OUTPUT")
endif()

# declare build structures
# !! change sdrbase/plugins/pluginmanager.cpp too !!
set(SDRANGEL_BINARY_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(SDRANGEL_BINARY_LIB_DIR "${CMAKE_BINARY_DIR}/lib")
if(WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SDRANGEL_BINARY_BIN_DIR}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SDRANGEL_BINARY_LIB_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SDRANGEL_BINARY_LIB_DIR}")
if(WIN32)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  set(BUILD_PLUGINS_RUNTIME_DIR "${SDRANGEL_BINARY_BIN_DIR}/plugins")
  set(BUILD_PLUGINSSRV_RUNTIME_DIR "${SDRANGEL_BINARY_BIN_DIR}/pluginssrv")
endif()
set(BUILD_PLUGINS_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/plugins")
set(BUILD_PLUGINSSRV_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/pluginssrv")
# use only lib when create .app/.dmg
if(APPLE AND BUNDLE AND BUILD_GUI)
  set(INSTALL_BIN_DIR ".")
  set(INSTALL_LIB_DIR "lib")
  set(INSTALL_DOC_DIR "doc")
else()
  set(INSTALL_BIN_DIR "bin")
  set(INSTALL_LIB_DIR "lib/${CMAKE_PROJECT_NAME}")
  set(INSTALL_DOC_DIR "share/${CMAKE_PROJECT_NAME}")
endif()
set(INSTALL_PLUGINS_DIR "${INSTALL_LIB_DIR}/plugins")
set(INSTALL_PLUGINSSRV_DIR "${INSTALL_LIB_DIR}/pluginssrv")
set(EXTERNAL_BUILD_LIBRARIES "${CMAKE_BINARY_DIR}/external")

# custom libraries
set(CUSTOM_APPLE_INCLUDE "${CMAKE_SOURCE_DIR}/custom/apple" CACHE INTERNAL "")
set(CUSTOM_WINDOWS_INCLUDE "${CMAKE_SOURCE_DIR}/custom/windows" CACHE INTERNAL "")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(LINUX TRUE)
  # populate distribution name
  # LSB_CODENAME will hold trusty for example
  find_program(LSB_RELEASE_EXECUTABLE lsb_release)
  if(LSB_RELEASE_EXECUTABLE)
    execute_process(COMMAND ${LSB_RELEASE_EXECUTABLE} -cs
      OUTPUT_VARIABLE LSB_CODENAME
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
  endif()
  configure_file("${CMAKE_SOURCE_DIR}/cmake/cpack/${CMAKE_PROJECT_NAME}.desktop.in"
    "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.desktop" @ONLY)
elseif(APPLE)
  if("${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
    message(STATUS "Build for macOS target: local version")
  else()
    message(STATUS "Build for macOS target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif()

  # prepend path to find_*()
  set(CMAKE_FIND_ROOT_PATH "/opt/local")

  set(CMAKE_MACOSX_RPATH ON)
  message(STATUS "RPATH support: ${CMAKE_MACOSX_RPATH}")

  configure_file("${CMAKE_SOURCE_DIR}/cmake/cpack/MacOSXBundleInfo.plist.in"
    "${CMAKE_BINARY_DIR}/Info.plist" @ONLY)
elseif (WIN32)
  # check compiler version
  if(MSVC_VERSION GREATER 1919 AND MSVC_VERSION LESS 1910)
    message(FATAL_ERROR "You must use Microsoft Visual Studio 2017 as compiler, aka MSVC 14.1")
  endif()

  # used on code but not defined on VS2017
  add_definitions(-D__WINDOWS__)

  # compile with full multicore
  set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

  # find Qt folder
  include(QtLocator)

  # in alternative we can use ExternalProject
  set(EXTERNAL_LIBRARY_FOLDER "${CMAKE_SOURCE_DIR}/external/windows")
  set(BOOST_ROOT "${EXTERNAL_LIBRARY_FOLDER}/boost" CACHE INTERNAL "")
  set(FFTW3F_FOUND ON CACHE INTERNAL "")
  set(FFTW3F_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/fftw-3/include" CACHE INTERNAL "")
  set(FFTW3F_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/fftw-3/libfftw3f-3.lib" CACHE INTERNAL "")
  set(LIBUSB_INCLUDE_DIR "${EXTERNAL_LIBRARY_FOLDER}/libusb/include" CACHE INTERNAL "")
  set(LIBUSB_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/libusb/MS64/dll/libusb-1.0.lib" CACHE INTERNAL "")
  set(OpenCV_DIR "${EXTERNAL_LIBRARY_FOLDER}/opencv" CACHE INTERNAL "")
  set(PKG_CONFIG_EXECUTABLE "${EXTERNAL_LIBRARY_FOLDER}/pkg-config-lite/bin/pkg-config.exe" CACHE INTERNAL "")

  # ffmpeg
  set(FFMPEG_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/include" CACHE INTERNAL "")
  #set(FFMPEG_LIBRARIES)
  set(AVCODEC_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/include" CACHE INTERNAL "")
  set(AVCODEC_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/lib/avcodec.lib" CACHE INTERNAL "")
  set(AVFORMAT_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/include" CACHE INTERNAL "")
  set(AVFORMAT_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/lib/avformat.lib" CACHE INTERNAL "")
  set(AVUTIL_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/include" CACHE INTERNAL "")
  set(AVUTIL_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/lib/avutil.lib" CACHE INTERNAL "")
  set(SWRESAMPLE_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/include" CACHE INTERNAL "")
  set(SWRESAMPLE_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/lib/swresample.lib" CACHE INTERNAL "")
  set(SWSCALE_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/include" CACHE INTERNAL "")
  set(SWSCALE_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/lib/swscale.lib" CACHE INTERNAL "")

  # used on fixup_bundle phase
  set(WINDOWS_FIXUP_BUNDLE_LIB_DIRS
    "${Qt5_DIR}/../../../bin"
    "${EXTERNAL_LIBRARY_FOLDER}/fftw-3"
    "${EXTERNAL_LIBRARY_FOLDER}/libusb/MS64/dll"
    "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/bin"
  )
endif()

# TODO: check if(DEFINED) and fix macports -D
# fix install path
set(CMAKE_INSTALL_RPATH
  "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}" CACHE
  PATH "Library Install RPath" FORCE)
# Allow RPATH to files outside the build tree
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# force full path
set(CMAKE_INSTALL_NAME_DIR
  "${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}" CACHE
  PATH "Library Install Name Destination Directory" FORCE)

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
set(_required_qt_version "5.6.0")
find_package(Qt5 COMPONENTS Core REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt5 COMPONENTS WebSockets REQUIRED)
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
find_package(OpenCV REQUIRED core highgui imgproc imgcodecs videoio) # channeltx/modatv

# unfortunately OpenCV library from windows doesn't set the cache option
if(WIN32)
  set(OpenCV_INCLUDE_DIRS "${OpenCV_INCLUDE_DIRS}" CACHE INTERNAL "")
  set(OpenCV_LIBS "${OpenCV_LIBS}" CACHE INTERNAL "")

  # add libs folder for fixup_bundle()
  get_filename_component(GFN_Boost_LIBS_DIR Boost_LIBS DIRECTORY)
  get_filename_component(GFN_OpenCV_LIBS_DIR OpenCV_LIBS DIRECTORY)
  set(WINDOWS_FIXUP_BUNDLE_LIB_DIRS
    "${WINDOWS_FIXUP_BUNDLE_LIB_DIRS}"
    "${GFN_Boost_LIBS_DIR}"
    "${GFN_OpenCV_LIBS_DIR}"
  )
endif()

# macOS compatibility
if(APPLE)
  find_package(ICONV)
endif()

# include external cmake if needed
if(ENABLE_EXTERNAL_LIBRARIES)
    add_subdirectory(external)
else()
    find_package(Codec2)
    find_package(CM256cc)
    find_package(LibMbe)
    find_package(SerialDV)
    find_package(LibDSDcc)
endif()

# Devices
if(ENABLE_AIRSPY)
  find_package(LibAIRSPY)
endif()
if(ENABLE_AIRSPYHF)
  find_package(LibAIRSPYHF)
endif()
if(ENABLE_BLADERF)
  find_package(LibBLADERF 2.0)
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

if (CODEC2_FOUND)
    add_subdirectory(libfreedv)
endif(CODEC2_FOUND)

if(ENABLE_FUNCUBE AND UNIX AND LIBUSB_FOUND)
    add_subdirectory(fcdlib)
    add_subdirectory(fcdhid)
endif()

# base libraries
add_subdirectory(sdrbase)
add_subdirectory(httpserver)
add_subdirectory(logging)
add_subdirectory(qrtplib)
add_subdirectory(swagger)
add_subdirectory(devices)

# strange symbol dependency
#  mainbench.obj : error LNK2001: unresolved external
#  symbol "public: static float const decimation_scale<12>::scaleIn" (?scaleIn@?$decimation_scale@$0M@@@2MB)
if(NOT WIN32)
  add_subdirectory(sdrbench)
endif()

if (BUILD_GUI)
    add_subdirectory(sdrgui)
    add_subdirectory(plugins plugins)
endif()

if (BUILD_SERVER)
    add_subdirectory(sdrsrv)
    set(SERVER_MODE ON)
    add_definitions(-DSERVER_MODE)
    add_subdirectory(plugins pluginssrv)
    remove_definitions(-DSERVER_MODE)
    set(SERVER_MODE OFF)
endif()

# includes needed by the following target
include_directories(
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrgui
    ${CMAKE_SOURCE_DIR}/sdrsrv
    ${CMAKE_SOURCE_DIR}/sdrbench
    ${CMAKE_SOURCE_DIR}/logging
    ${OPENGL_INCLUDE_DIR}
)

############ build sdrangel benchmark ################
if(NOT WIN32)
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
endif()
############ build sdrangel gui ################
if (BUILD_GUI)
    set(sdrangel_SOURCES
        app/main.cpp
        sdrgui/resources/sdrangel.rc
    )

  add_executable(${CMAKE_PROJECT_NAME}
    ${sdrangel_SOURCES}
    )

  target_link_libraries(${CMAKE_PROJECT_NAME}
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

############ install ##################
if(NOT WIN32)
  install(TARGETS sdrangelbench DESTINATION ${INSTALL_BIN_DIR})
endif()
if (BUILD_GUI)
      install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${INSTALL_BIN_DIR})
endif()
if (BUILD_SERVER)
  install(TARGETS sdrangelsrv DESTINATION ${INSTALL_BIN_DIR})
endif()

# install external libraries dependencies (Linux)
if (LINUX AND ENABLE_EXTERNAL_LIBRARIES)
    install(DIRECTORY ${EXTERNAL_BUILD_LIBRARIES}/lib/ DESTINATION ${INSTALL_LIB_DIR})
endif()

# install documentation
# TODO maybe install readme for every plugins
if (LINUX OR APPLE)
  install(FILES ${CMAKE_SOURCE_DIR}/Readme.md DESTINATION ${INSTALL_DOC_DIR})
endif()

if(APPLE)
  install(FILES ${CMAKE_SOURCE_DIR}/CHANGELOG DESTINATION ${INSTALL_DOC_DIR})
  install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION ${INSTALL_DOC_DIR})
endif()

#install files and directories (linux specific)
if (LINUX)
  install(FILES ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.desktop DESTINATION share/applications)
  install(FILES ${CMAKE_SOURCE_DIR}/cmake/cpack/${CMAKE_PROJECT_NAME}_icon.png DESTINATION share/pixmaps)
endif()

############ uninstall target ################
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/include/uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -P
  ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake)

############ packaging ################
add_subdirectory(cmake/cpack)
