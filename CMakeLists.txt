cmake_minimum_required(VERSION 3.13.0)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

project(sdrangel)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

# disable only when needed
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# configure version
set(sdrangel_VERSION_MAJOR "7")
set(sdrangel_VERSION_MINOR "8")
set(sdrangel_VERSION_PATCH "5")
set(sdrangel_VERSION_SUFFIX "")

# SDRAngel cmake options
option(DEBUG_OUTPUT "Print debug messages" OFF)
option(SANITIZE_ADDRESS "Activate memory address sanitization" OFF)
option(RX_SAMPLE_24BIT "Internal 24 bit Rx DSP" ON)
option(BUILD_SERVER "Build Server" ON)
option(BUILD_GUI "Build GUI" ON)
option(HIDE_CONSOLE "Hide console when running GUI on Windows" ON)
option(BUNDLE "Enable distribution bundle" OFF)
set(ARCH_OPT "native" CACHE STRING "Specify instruction set to use. Will be passed directly as `-march` or `/arch:` argument on supported compilers. \
                                   'native' option will figure out host machine compatibilities and set flags accordingly (even with MSVC).")
option(ENABLE_QT6 "Build with Qt6 rather than Qt5" OFF)

# Sampling devices enablers
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
option(ENABLE_SDRPLAY "Enable SDRplay support" ON)
option(ENABLE_SOAPYSDR "Enable SoapySDR support" ON)
option(ENABLE_XTRX "Enable XTRX support" ON)
option(ENABLE_PACK_MIRSDRAPI "Enable inclusion of the mirsdr-api library - for personal use only" OFF)
option(ENABLE_USRP "Enable USRP support" ON)

# Channel Rx enablers
option(ENABLE_CHANNELRX "Enable channelrx plugins" ON)
option(ENABLE_CHANNELRX_DEMODADSB "Enable channelrx demodadsb plugin" ON)
option(ENABLE_CHANNELRX_DEMODAM "Enable channelrx demodam plugin" ON)
option(ENABLE_CHANNELRX_DEMODDATV "Enable channelrx demoddatv plugin" ON)
option(ENABLE_CHANNELRX_RADIOCLOCK "Enable channelrx radioclock plugin" ON)
option(ENABLE_CHANNELRX_RADIOASTRONOMY "Enable channelrx radioastronomy plugin" ON)
option(ENABLE_CHANNELRX_DEMODRADIOSONDE "Enable channelrx demodradiosonde plugin" ON)
option(ENABLE_CHANNELRX_FREQTRACKER "Enable channelrx freqtracker plugin" ON)
option(ENABLE_CHANNELRX_DEMODATV "Enable channelrx demodatv plugin" ON)
option(ENABLE_CHANNELRX_DEMODPAGER "Enable channelrx demodpager plugin" ON)
option(ENABLE_CHANNELRX_DEMODDAB "Enable channelrx demoddab plugin" ON)
option(ENABLE_CHANNELRX_UDPSINK "Enable channelrx udpsink plugin" ON)
option(ENABLE_CHANNELRX_DEMODAIS "Enable channelrx demodais plugin" ON)
option(ENABLE_CHANNELRX_DEMODNFM "Enable channelrx demodnfm plugin" ON)
option(ENABLE_CHANNELRX_DEMODM17 "Enable channelrx demodm17 plugin" ON)
option(ENABLE_CHANNELRX_FILESINK "Enable channelrx filesink plugin" ON)
option(ENABLE_CHANNELRX_DEMODFREEDV "Enable channelrx demodfreedv plugin" ON)
option(ENABLE_CHANNELRX_DEMODCHIRPCHAT "Enable channelrx demodchirpchat plugin" ON)
option(ENABLE_CHANNELRX_REMOTESINK "Enable channelrx remotesink plugin" ON)
option(ENABLE_CHANNELRX_REMOTETCPSINK "Enable channelrx remotetcpsink plugin" ON)
option(ENABLE_CHANNELRX_DEMODSSB "Enable channelrx demodssb plugin" ON)
option(ENABLE_CHANNELRX_CHANALYZER "Enable channelrx chanalyzer plugin" ON)
option(ENABLE_CHANNELRX_SIGMFFILESINK "Enable channelrx sigmffilesink plugin" ON)
option(ENABLE_CHANNELRX_DEMODBFM "Enable channelrx demodbfm plugin" ON)
option(ENABLE_CHANNELRX_DEMODWFM "Enable channelrx demodwfm plugin" ON)
option(ENABLE_CHANNELRX_NOISEFIGURE "Enable channelrx noisefigure plugin" ON)
option(ENABLE_CHANNELRX_DEMODVOR "Enable channelrx demodvor plugin" ON)
option(ENABLE_CHANNELRX_LOCALSINK "Enable channelrx localsink plugin" ON)
option(ENABLE_CHANNELRX_DEMODPACKET "Enable channelrx demodpacket plugin" ON)
option(ENABLE_CHANNELRX_DEMODAPT "Enable channelrx demodapt plugin" ON)
option(ENABLE_CHANNELRX_DEMODDSD "Enable channelrx demoddsd plugin" ON)

# Channel Tx enablers
option(ENABLE_CHANNELTX "Enable channeltx plugins" ON)
option(ENABLE_CHANNELTX_MODM17 "Enable channeltx modm17 plugin" ON)
option(ENABLE_CHANNELTX_MODNFM "Enable channeltx modnfm plugin" ON)
option(ENABLE_CHANNELTX_MODAIS "Enable channeltx modais plugin" ON)
option(ENABLE_CHANNELTX_LOCALSOURCE "Enable channeltx localsource plugin" ON)
option(ENABLE_CHANNELTX_MODDATV "Enable channeltx moddatv plugin" ON)
option(ENABLE_CHANNELTX_MODAM "Enable channeltx modam plugin" ON)
option(ENABLE_CHANNELTX_MODFREEDV "Enable channeltx modfreedv plugin" ON)
option(ENABLE_CHANNELTX_MODPACKET "Enable channeltx modpacket plugin" ON)
option(ENABLE_CHANNELTX_MODSSB "Enable channeltx modssb plugin" ON)
option(ENABLE_CHANNELTX_UDPSOURCE "Enable channeltx udpsource plugin" ON)
option(ENABLE_CHANNELTX_MODCHIRPCHAT "Enable channeltx modchirpchat plugin" ON)
option(ENABLE_CHANNELTX_MODWFM "Enable channeltx modwfm plugin" ON)
option(ENABLE_CHANNELTX_MODATV "Enable channeltx modatv plugin" ON)
option(ENABLE_CHANNELTX_MOD802.15.4 "Enable channeltx mod802.15.4 plugin" ON)
option(ENABLE_CHANNELTX_REMOTESOURCE "Enable channeltx remotesource plugin" ON)
option(ENABLE_CHANNELTX_FILESOURCE "Enable channeltx filesource plugin" ON)

# Channel MIMO enablers
option(ENABLE_CHANNELMIMO "Enable channelmimo plugins" ON)
option(ENABLE_CHANNELMIMO_INTERFEROMETER "Enable channelmimo interferometer plugin" ON)
option(ENABLE_CHANNELMIMO_DOA2 "Enable channelmimo doa2 plugin" ON)
option(ENABLE_CHANNELMIMO_BEAMSTEERINGCWMOD "Enable channelmimo beamsteeringcwmod plugin" ON)

# Feature enablers
option(ENABLE_FEATURE "Enable feature plugins" ON)
option(ENABLE_FEATURE_LIMERFE "Enable feature limerfe plugin" ON)
option(ENABLE_FEATURE_VORLOCALIZER "Enable feature vorlocalizer plugin" ON)
option(ENABLE_FEATURE_JOGDIALCONTROLLER "Enable feature jogdialcontroller plugin" ON)
option(ENABLE_FEATURE_MAP "Enable feature map plugin" ON)
option(ENABLE_FEATURE_SIMPLEPTT "Enable feature simpleptt plugin" ON)
option(ENABLE_FEATURE_RADIOSONDE "Enable feature radiosonde plugin" ON)
option(ENABLE_FEATURE_ANTENNATOOLS "Enable feature antennatools plugin" ON)
option(ENABLE_FEATURE_SATELLITETRACKER "Enable feature satellitetracker plugin" ON)
option(ENABLE_FEATURE_APRS "Enable feature aprs plugin" ON)
option(ENABLE_FEATURE_DEMODANALYZER "Enable feature demodanalyzer plugin" ON)
option(ENABLE_FEATURE_AMBE "Enable feature ambe plugin" ON)
option(ENABLE_FEATURE_AIS "Enable feature ais plugin" ON)
option(ENABLE_FEATURE_AFC "Enable feature afc plugin" ON)
option(ENABLE_FEATURE_STARTRACKER "Enable feature startracker plugin" ON)
option(ENABLE_FEATURE_RIGCTLSERVER "Enable feature rigctlserver plugin" ON)
option(ENABLE_FEATURE_PERTESTER "Enable feature pertester plugin" ON)
option(ENABLE_FEATURE_GS232CONTROLLER "Enable feature gs232controller plugin" ON)
option(ENABLE_FEATURE_REMOTECONTROL "Enable feature remote control plugin" ON)

# on windows always build external libraries
if(WIN32)
  set(ENABLE_EXTERNAL_LIBRARIES ON CACHE BOOL "Build external libraries" FORCE)
  # to run fixup_bundle()
  set(BUNDLE ON CACHE BOOL "Enable distribution bundle" FORCE)
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
        string(REGEX REPLACE "^v([0-9]+)\.([0-9]+)\.([0-9]+)" "" git_commit ${out})
        set(sdrangel_VERSION_SUFFIX "${git_commit}")
    endif()
endif()

set(sdrangel_VERSION "${sdrangel_VERSION_MAJOR}.${sdrangel_VERSION_MINOR}.${sdrangel_VERSION_PATCH}${sdrangel_VERSION_SUFFIX}")
message(STATUS "${APPLICATION_NAME} Version: ${sdrangel_VERSION}")
add_definitions("-DSDRANGEL_LIB_VERSION=\"${sdrangel_VERSION_MAJOR}.${sdrangel_VERSION_MINOR}.${sdrangel_VERSION_PATCH}\"")
add_definitions("-DSDRANGEL_VERSION=\"${sdrangel_VERSION}\"")

# configure CPACK information
set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SDR Rx/Tx software for Airspy, Airspy HF+, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay, USRP and FunCube")
set(CPACK_PACKAGE_VENDOR "${COMPANY}")
set(CPACK_PACKAGE_CONTACT "https://github.com/f4exb/sdrangel")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/Readme.md")
set(CPACK_PACKAGE_VERSION "${sdrangel_VERSION_MAJOR}.${sdrangel_VERSION_MINOR}.${sdrangel_VERSION_PATCH}${sdrangel_VERSION_SUFFIX}")
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

if(ENABLE_EXTERNAL_LIBRARIES STREQUAL "AUTO")
  set(AUTO_EXTERNAL_LIBRARIES ON CACHE INTERNAL "")
endif()

# declare build structures
# !! change sdrbase/plugins/pluginmanager.cpp too !!
add_definitions("-DLIB=\"lib${LIB_SUFFIX}\"")
set(SDRANGEL_BINARY_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(SDRANGEL_BINARY_LIB_DIR "${CMAKE_BINARY_DIR}/lib${LIB_SUFFIX}")
if(WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SDRANGEL_BINARY_BIN_DIR}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${SDRANGEL_BINARY_LIB_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${SDRANGEL_BINARY_LIB_DIR}")
if(WIN32)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
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
elseif(WIN32)
  # used to create package
  set(INSTALL_BIN_DIR ".")
  set(INSTALL_LIB_DIR ".")
  set(INSTALL_DOC_DIR ".")
else()
  set(INSTALL_BIN_DIR "bin")
  set(INSTALL_LIB_DIR "lib${LIB_SUFFIX}/${CMAKE_PROJECT_NAME}")
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
  add_definitions(-DLINUX)
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
  message(STATUS "MSVC_VERSION is ${MSVC_VERSION}")
  if(MSVC_VERSION GREATER_EQUAL 1930 AND MSVC_VERSION LESS 1940)
    set(VS2022 ON)
  elseif(MSVC_VERSION GREATER_EQUAL 1920 AND MSVC_VERSION LESS 1930)
    set(VS2019 ON)
  elseif(MSVC_VERSION GREATER_EQUAL 1910 AND MSVC_VERSION LESS 1919)
    set(VS2017 ON)
  elseif(MSVC_VERSION GREATER_EQUAL 1900 AND MSVC_VERSION LESS 1910)
    set(VS2015 ON)
  endif()

  if(NOT VS2015 AND NOT VS2017 AND NOT VS2019 AND NOT VS2022)
    message(FATAL_ERROR "You must use Microsoft Visual Studio 2015, 2017, 2019 or greater as compiler")
  endif()

  # in alternative we can use ExternalProject
  set(EXTERNAL_LIBRARY_FOLDER "${CMAKE_SOURCE_DIR}/external/windows")
  set(FFTW3F_FOUND ON CACHE INTERNAL "")
  set(FFTW3F_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/fftw-3/include" CACHE INTERNAL "")
  set(FFTW3F_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/fftw-3/libfftw3f-3.lib" CACHE INTERNAL "")
  set(FFTW3F_WISDOM "${EXTERNAL_LIBRARY_FOLDER}/fftw-3/fftwf-wisdom.exe" CACHE INTERNAL "")
  set(LIBUSB_FOUND ON CACHE INTERNAL "")
  set(LIBUSB_INCLUDE_DIR "${EXTERNAL_LIBRARY_FOLDER}/libusb/include" CACHE INTERNAL "")
  set(LIBUSB_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/libusb/MS64/dll/libusb-1.0.lib" CACHE INTERNAL "")
  if(VS2019)
    set(OpenCV_DIR "${EXTERNAL_LIBRARY_FOLDER}/opencv4" CACHE INTERNAL "")
  else()
    set(OpenCV_DIR "${EXTERNAL_LIBRARY_FOLDER}/opencv" CACHE INTERNAL "")
  endif()
  set(PKG_CONFIG_EXECUTABLE "${EXTERNAL_LIBRARY_FOLDER}/pkg-config-lite/bin/pkg-config.exe" CACHE INTERNAL "")
  set(SOAPYSUPPORT_DIR "${EXTERNAL_LIBRARY_FOLDER}/soapysdr-support" CACHE INTERNAL "")

  set(CODEC2_FOUND ON CACHE INTERNAL "")
  set(CODEC2_INCLUDE_DIR "${EXTERNAL_LIBRARY_FOLDER}/codec2/include" CACHE INTERNAL "")
  set(CODEC2_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/codec2/lib/codec2.lib" CACHE INTERNAL "")
  set(CODEC2_DLL_DIR "${EXTERNAL_LIBRARY_FOLDER}/codec2/lib" CACHE INTERNAL "")

  set(UHD_FOUND ON CACHE INTERNAL "")
  set(UHD_INCLUDE_DIR "${EXTERNAL_LIBRARY_FOLDER}/uhd/include" CACHE INTERNAL "")
  set(UHD_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/uhd/lib/uhd.lib" CACHE INTERNAL "")
  set(UHD_DLL_DIR "${EXTERNAL_LIBRARY_FOLDER}/uhd/bin" CACHE INTERNAL "")

  set(DAB_FOUND ON CACHE INTERNAL "")
  set(DAB_INCLUDE_DIR "${EXTERNAL_LIBRARY_FOLDER}/dab" CACHE INTERNAL "")
  set(DAB_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/dab/dab_lib.lib" CACHE INTERNAL "")
  set(DAB_DLL_DIR "${EXTERNAL_LIBRARY_FOLDER}/dab" CACHE INTERNAL "")

  set(OPENSSL_FOUND ON CACHE INTERNAL "")
  set(OPENSSL_DLL_DIR "${EXTERNAL_LIBRARY_FOLDER}/openssl" CACHE INTERNAL "")

  # ffmpeg
  set(FFMPEG_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/include" CACHE INTERNAL "")
  set(FFMPEG_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/bin" CACHE INTERNAL "")
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

  # LimeSuite FX3 support
  set(FX3SDK_DIR "${EXTERNAL_LIBRARY_FOLDER}/FX3SDK" CACHE INTERNAL "")

  set(SDRPLAY_FOUND ON CACHE INTERNAL "")
  set(SDRPLAY_INCLUDE_DIR "${EXTERNAL_LIBRARY_FOLDER}/sdrplay/inc" CACHE INTERNAL "")
  set(SDRPLAY_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/sdrplay/x64/sdrplay_api.lib" CACHE INTERNAL "")
  set(SDRPLAY_DLL_DIR "${EXTERNAL_LIBRARY_FOLDER}/sdrplay/x64/" CACHE INTERNAL "")

  set(FAAD_FOUND ON CACHE INTERNAL "")
  set(FAAD_INCLUDE_DIR "${EXTERNAL_LIBRARY_FOLDER}/faad2/include" CACHE INTERNAL "")
  set(FAAD_LIBRARY "${EXTERNAL_LIBRARY_FOLDER}/faad2/lib/libfaad.lib" CACHE INTERNAL "")

  set(ZLIB_FOUND ON CACHE INTERNAL "")
  set(ZLIB_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/zlib/include" CACHE INTERNAL "")
  set(ZLIB_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/zlib/lib/zlibstaticd.lib" CACHE INTERNAL "")

  set(HIDAPI_FOUND ON CACHE INTERNAL "")
  set(HIDAPI_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/hidapi/include" CACHE INTERNAL "")
  set(HIDAPI_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/hidapi/x64/hidapi.lib" CACHE INTERNAL "")
  set(HIDAPI_DLL_DIR "${EXTERNAL_LIBRARY_FOLDER}/hidapi/x64" CACHE INTERNAL "")

  # used on fixup_bundle phase
  set(WINDOWS_FIXUP_BUNDLE_LIB_DIRS
    "${EXTERNAL_LIBRARY_FOLDER}/fftw-3"
    "${EXTERNAL_LIBRARY_FOLDER}/libusb/MS64/dll"
    "${EXTERNAL_LIBRARY_FOLDER}/ffmpeg/bin"
  )
endif()

# When building a bundle on MacOS, we want to keep build paths in the library
# so macdeployqt can find them. The following code is only for installing
if (NOT BUNDLE)
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
endif()

# enable 24 bit receiving path
if (RX_SAMPLE_24BIT)
    message(STATUS "Compiling for 24 bit Rx DSP chain")
    add_compile_definitions(SDR_RX_SAMPLE_24BIT)
else()
    message(STATUS "Compiling for 16 bit Rx DSP chain")
endif()

# Set compiler options based on target architecture and selected extensions
include(CompilerOptions)

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
# See: https://doc-snapshots.qt.io/qt6-dev/cmake-qt5-and-qt6-compatibility.html
if(ENABLE_QT6)
    # Qt6 doesn't currently support Location
    find_package(Qt6
                   COMPONENTS
                     Core
                     Widgets
                     WebSockets
                     Multimedia
                     MultimediaWidgets
                     Positioning
                     Charts
                     SerialPort
                     Core5Compat)
else()
    find_package(Qt5 5.15
                   REQUIRED COMPONENTS
                     Core
                     Widgets
                     WebSockets
                     Multimedia
                     MultimediaWidgets
                     Positioning
                     Charts
                     SerialPort)
endif()

# for the server we don't need OpenGL/Qt Quick components
if (BUILD_GUI)
    find_package(OpenGL REQUIRED)
    if (ENABLE_QT6)
        find_package(Qt6
                       REQUIRED COMPONENTS
                        OpenGL
                        OpenGLWidgets
                        Quick
                        QuickWidgets
                       OPTIONAL_COMPONENTS
                        WebEngineQuick
                        WebEngineCore
                        WebEngineWidgets
                        TextToSpeech)
    else()
        find_package(Qt5
                       REQUIRED COMPONENTS
                        OpenGL
                        Quick
                        QuickWidgets
                        Location
                        TextToSpeech
                        WebEngine
                        WebEngineCore
                        WebEngineWidgets)
    endif()
endif()

# other requirements
find_package(PkgConfig REQUIRED)

if(APPLE)
  find_package(ICONV)
endif()

# unfortunately OpenCV library from windows doesn't set the cache option
if(WIN32)
  find_package(OpenCV OPTIONAL_COMPONENTS core highgui imgproc imgcodecs videoio) # channeltx/modatv

  set(OpenCV_INCLUDE_DIRS "${OpenCV_INCLUDE_DIRS}" CACHE INTERNAL "")
  set(OpenCV_LIBS "${OpenCV_LIBS}" CACHE INTERNAL "")

  # add libs folder for fixup_bundle()
  get_filename_component(GFN_Boost_LIBS_DIR "${Boost_LIBS}" DIRECTORY)

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OpenCV_ARCH "64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(OpenCV_ARCH "86")
  endif()

  set(OpenCV_LIBS_BIN_DIR "${OpenCV_DIR}/x${OpenCV_ARCH}/${OpenCV_RUNTIME}/bin" CACHE INTERNAL "")

  set(WINDOWS_FIXUP_BUNDLE_LIB_DIRS
    "${WINDOWS_FIXUP_BUNDLE_LIB_DIRS}"
    "${GFN_Boost_LIBS_DIR}"
    "${OpenCV_LIBS_BIN_DIR}"
  )
endif()

# When ENABLE_EXTERNAL_LIBRARIES is ON, we can build most of the required 3rd party libraries from source
if(ENABLE_EXTERNAL_LIBRARIES)
    add_subdirectory(external)
    find_package(Boost REQUIRED)
else()
    find_package(Boost REQUIRED)
    find_package(FFTW3F REQUIRED)
    find_package(LibUSB REQUIRED)  # used by so many packages
    find_package(OpenCV OPTIONAL_COMPONENTS core highgui imgproc imgcodecs videoio) # channeltx/modatv
    find_package(LibSigMF) # SigMF recording files support
    find_package(ZLIB) # For DAB
    find_package(FAAD) # For DAB
    find_package(Codec2)
    find_package(CM256cc)
    find_package(LibMbe)
    find_package(SerialDV REQUIRED)
    find_package(LibDSDcc)
    find_package(Sgp4)
    find_package(AptDec)
    find_package(LibDAB)
    find_package(HIDAPI)
endif()

if (LIBSIGMF_FOUND AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_definitions(-DHAS_LIBSIGMF)
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
  if (LIMESUITE_FOUND)
    add_definitions(-DHAS_LIMERFEUSB)
  endif()
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
if(ENABLE_SDRPLAY)
  find_package(SDRplay)
endif()
if(ENABLE_SOAPYSDR)
  find_package(SoapySDR)
endif()
if(ENABLE_XTRX)
  find_package(LibXTRX)
endif()
if(ENABLE_USRP)
  find_package(UHD)
endif()

if(ENABLE_FUNCUBE AND LIBUSB_FOUND AND HIDAPI_FOUND)
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
add_subdirectory(sdrbench)

add_subdirectory(modemm17)

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

# Set up optional make target for developer doc
find_package(Doxygen)
if(DOXYGEN_FOUND)
  option(WITH_DOC "Generate developer doc with Doxygen" ON)
  message(STATUS "added optional 'make dev_docs' for developer documentation generation")

  # It is not an error when 'dot' is not found, just switching off the Doxygen's HAVE_DOT option
  find_package_handle_standard_args (Dot REQUIRED_VARS DOXYGEN_DOT_EXECUTABLE)

  # This sets HAVE_CASE_SENSITIVE_FILESYSTEM which is 0 or 1, we need "YES" or "NO"
  include(cmake/Modules/CheckCaseSensitiveFileSystem.cmake)
  if (HAVE_CASE_SENSITIVE_FILESYSTEM)
    set(CMAKE_CASE_SENSITIVE_FILESYSTEM "YES")
  else()
    set(CMAKE_CASE_SENSITIVE_FILESYSTEM "NO")
  endif()

  if(WIN32 OR APPLE)
    set(CMAKE_SHORT_NAMES "YES")
  else()
    set(CMAKE_SHORT_NAMES "NO")
  endif()

  set(DOX_HTML_OUT ${CMAKE_CURRENT_BINARY_DIR}/dev_docs)

  if (WITH_DOC)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/include/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(dev_docs)
    add_custom_command(TARGET dev_docs POST_BUILD
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating developer documentation with Doxygen" VERBATIM
    )

  endif()
else()
  message(STATUS "Doxygen not found, developer documentation can not be generated")
endif()

############ build sdrangel benchmark ################
set(sdrangelbench_SOURCES
  appbench/main.cpp
)

add_executable(sdrangelbench
  ${sdrangelbench_SOURCES}
)

target_link_libraries(sdrangelbench
  Qt::Multimedia
  sdrbench
  logging
)
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
    Qt::Widgets
    Qt::Multimedia
    sdrbase
    sdrgui
    logging
    )

  if(WIN32)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
                            WIN32_EXECUTABLE ${HIDE_CONSOLE})
  endif()
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
        Qt::Multimedia
        sdrbase
        sdrsrv
        logging
    )
endif()

############ install ##################
install(TARGETS sdrangelbench DESTINATION ${INSTALL_BIN_DIR})
if (BUILD_GUI)
      install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${INSTALL_BIN_DIR})
endif()
if (BUILD_SERVER)
  install(TARGETS sdrangelsrv DESTINATION ${INSTALL_BIN_DIR})
endif()

# install external libraries dependencies (Linux)
if (LINUX AND ENABLE_EXTERNAL_LIBRARIES)
    install(DIRECTORY ${EXTERNAL_BUILD_LIBRARIES}/lib${LIB_SUFFIX}/ DESTINATION ${INSTALL_LIB_DIR})
endif()
if(WIN32)
  include(DeployQt)
  windeployqt(${CMAKE_PROJECT_NAME} ${SDRANGEL_BINARY_BIN_DIR} ${PROJECT_SOURCE_DIR}/sdrgui/resources)
endif()

# Install debug symbols
if (WIN32)
  install(FILES $<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_BIN_DIR} )
endif()

# install documentation
# TODO maybe install readme for every plugins
install(FILES ${CMAKE_SOURCE_DIR}/Readme.md DESTINATION ${INSTALL_DOC_DIR})

if(APPLE OR WIN32)
  install(FILES ${CMAKE_SOURCE_DIR}/CHANGELOG DESTINATION ${INSTALL_DOC_DIR})
  install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION ${INSTALL_DOC_DIR})
endif()

#install files and directories (linux specific)
if (LINUX)
  install(FILES ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.desktop DESTINATION share/applications)
  install(FILES ${CMAKE_SOURCE_DIR}/cmake/cpack/${CMAKE_PROJECT_NAME}_icon.svg DESTINATION share/icons/hicolor/scalable/apps)
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
