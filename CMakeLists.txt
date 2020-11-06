cmake_minimum_required(VERSION 3.10.0)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

project(sdrangel)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

# disable only when needed
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# configure version
set(sdrangel_VERSION_MAJOR "4")
set(sdrangel_VERSION_MINOR "21")
set(sdrangel_VERSION_PATCH "2")
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
option(ENABLE_USRP "Enable USRP support" ON)
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

add_compile_definitions(_USE_MATH_DEFINES)
if(WIN32)
  add_compile_definitions(NOMINMAX)
endif()

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
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SDR Rx/Tx software for Airspy, Airspy HF+, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube")
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
set(SDRANGEL_BINARY_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(SDRANGEL_BINARY_LIB_DIR "${CMAKE_BINARY_DIR}/lib")
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
  if(MSVC_VERSION GREATER 1920 AND MSVC_VERSION LESS 1929)
    set(VS2019 ON)
  elseif(MSVC_VERSION GREATER 1910 AND MSVC_VERSION LESS 1919)
    set(VS2017 ON)
  elseif(MSVC_VERSION GREATER 1899 AND MSVC_VERSION LESS 1910)
    set(VS2015 ON)
  endif()

  if(NOT VS2015 AND NOT VS2017 AND NOT VS2019)
    message(FATAL_ERROR "You must use Microsoft Visual Studio 2015, 2017 or 2019 as compiler")
  endif()

  # compile with full multicore
  if(MSVC)
    add_compile_definitions(/MP)
  endif()

  # in alternative we can use ExternalProject
  set(EXTERNAL_LIBRARY_FOLDER "${CMAKE_SOURCE_DIR}/external/windows")
  set(FFTW3F_FOUND ON CACHE INTERNAL "")
  set(FFTW3F_INCLUDE_DIRS "${EXTERNAL_LIBRARY_FOLDER}/fftw-3/include" CACHE INTERNAL "")
  set(FFTW3F_LIBRARIES "${EXTERNAL_LIBRARY_FOLDER}/fftw-3/libfftw3f-3.lib" CACHE INTERNAL "")
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

  # used on fixup_bundle phase
  set(WINDOWS_FIXUP_BUNDLE_LIB_DIRS
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
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -MP ${EXTRA_FLAGS}")
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
find_package(Qt5 COMPONENTS SerialPort)

# for the server we don't need OpenGL/Qt Quick components
if (BUILD_GUI)
    find_package(OpenGL REQUIRED)
    find_package(Qt5 COMPONENTS OpenGL REQUIRED)
    find_package(Qt5 COMPONENTS Quick)
    find_package(Qt5 COMPONENTS QuickWidgets)
    find_package(Qt5 COMPONENTS Positioning)
endif()

# other requirements
find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED)
find_package(FFTW3F REQUIRED)
find_package(LibUSB REQUIRED)  # used by so many packages
find_package(OpenCV OPTIONAL_COMPONENTS core highgui imgproc imgcodecs videoio) # channeltx/modatv

# macOS compatibility
if(APPLE)
  find_package(ICONV)
endif()

# unfortunately OpenCV library from windows doesn't set the cache option
if(WIN32)
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

# include external cmake if needed
if(ENABLE_EXTERNAL_LIBRARIES)
    add_subdirectory(external)
else()
    find_package(Codec2)
    if (NOT APPLE)
        find_package(CM256cc)
    endif()
    find_package(LibMbe)
    find_package(SerialDV REQUIRED)
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
  if (LINUX AND LIMESUITE_FOUND)
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
if(ENABLE_SOAPYSDR)
  find_package(SoapySDR)
endif()
if(ENABLE_XTRX)
  find_package(LibXTRX)
endif()
if(ENABLE_USRP)
  find_package(UHD)
endif()

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

    if (MSVC)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
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
if(WIN32)
  include(DeployQt)
  windeployqt(${CMAKE_PROJECT_NAME} ${SDRANGEL_BINARY_BIN_DIR} ${PROJECT_SOURCE_DIR}/sdrgui/resources)
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
