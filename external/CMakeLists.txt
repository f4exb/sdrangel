# Many idea are exposed at
#  https://stackoverflow.com/questions/15175318/cmake-how-to-build-external-projects-and-include-their-targets
#  https://crascit.com/2015/07/25/cmake-gtest/
#
# in essence, we need a way to build external libraries before use it;
# we choose add_dependencies() to the target.
# other viable solution is to use execute_process() during config
# but make the configuration dirty

# which version/tag/checkout to use
set(CODEC2_TAG "v0.9.2")
set(CM256CC_TAG f21e8bc1e9afdb0b28672743dcec111aec1d32d9)
set(MBELIB_TAG 9a04ed5c78176a9965f3d43f7aa1b1f5330e771f)
set(SERIALDV_TAG "v1.1.4")
set(DSDCC_TAG "v1.8.6")
set(LIMESUITE_TAG 6159754269a45e3d2c6b355285f501261ec5b34e)
set(BLADERF_TAG "2019.07")
set(LIBIIO_TAG 826563e41b5ce9890b75506f672017de8d76d52d)
set(AIRSPYHF_TAG "1.1.5")
set(PERSEUS_TAG afefa23e3140ac79d845acb68cf0beeb86d09028)

set(PTHREADS4W_TAG "Version-2-11-0-release")
set(RTLSDR_TAG be1d1206bfb6e6c41f7d91b20b77e20f929fa6a7)
set(RTLSDR_UDEV OFF)
set(SOAPYSDR_TAG "soapy-sdr-0.7.1")
set(SOAPYSDR_SDRPLAY_TAG "soapy-sdrplay-0.2.0")
set(SOAPYSDR_AUDIO_TAG "soapy-audio-0.1.1")
set(SOAPYSDR_REMOTE_TAG "soapy-remote-0.5.1")
set(AIRSPY_TAG bfb667080936ca5c2d23b3282f5893931ec38d3f)
set(HACKRF_TAG "v2018.01.1")
set(LIBXML2_TAG "v2.9.9")

# For some external project macros
include(ExternalProject)

# default build destination in windows to avoid Release/Debug folder
if(WIN32)
  # the build will be output on the same folder of sdrangel
  set(DEFAULT_OUTPUT_DIRECTORIES
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_BIN_DIR}
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_BIN_DIR}
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_BIN_DIR}
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_LIB_DIR}
  )
elseif(LINUX)
set(DEFAULT_OUTPUT_DIRECTORIES
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${EXTERNAL_BUILD_LIBRARIES}/bin
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${EXTERNAL_BUILD_LIBRARIES}/bin
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${EXTERNAL_BUILD_LIBRARIES}/bin
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${EXTERNAL_BUILD_LIBRARIES}/lib
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${EXTERNAL_BUILD_LIBRARIES}/lib
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${EXTERNAL_BUILD_LIBRARIES}/lib
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${EXTERNAL_BUILD_LIBRARIES}/lib
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG=${EXTERNAL_BUILD_LIBRARIES}/lib
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=${EXTERNAL_BUILD_LIBRARIES}/lib)
endif()

if(LINUX)
  # macro that create symbolic links
  macro(makeLink src dest target)
    if(NOT EXISTS ${dest})
      add_custom_command(TARGET ${target} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${src} ${dest} DEPENDS ${dest} COMMENT "mklink ${src} -> ${dest}")
    endif()
  endmacro()
  # macro that copy folder
  macro(makeCopy src dest target)
    if(NOT EXISTS ${dest})
      add_custom_command(TARGET ${target} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${src} ${dest} DEPENDS ${dest} COMMENT "copy ${src} -> ${dest}")
    endif()
  endmacro()
  # macro that copy file
  macro(makeCopyFile src dest target)
    if(NOT EXISTS ${dest})
      add_custom_command(TARGET ${target} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${src} ${dest} DEPENDS ${dest} COMMENT "copy ${src} -> ${dest}")
    endif()
  endmacro()
else()
# macro that create symbolic links
  macro(makeLink src dest target)
    if(NOT EXISTS ${dest})
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${src} ${dest} DEPENDS ${dest} COMMENT "mklink ${src} -> ${dest}")
    endif()
  endmacro()

  # macro that copy folder
  macro(makeCopyDir src dest target)
    if(NOT EXISTS ${dest})
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${src} ${dest} DEPENDS ${dest} COMMENT "copy ${src} -> ${dest}")
    endif()
  endmacro()

  # macro that copy file
  macro(makeCopy src dest target)
    if(NOT EXISTS ${dest})
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${src} ${dest} DEPENDS ${dest} COMMENT "copy ${src} -> ${dest}")
    endif()
  endmacro()
endif(LINUX)

if(WIN32)
  if(VS2015)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(VCREDIST_FILE "vcredist_msvc2015_x64${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
    # maybe add arm architecture
    else()
      set(VCREDIST_FILE "vcredist_msvc2015_x86${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
    endif()
  elseif(VS2017)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(VCREDIST_FILE "vcredist_msvc2017_x64${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
    # maybe add arm architecture
    else()
      set(VCREDIST_FILE "vcredist_msvc2017_x86${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
    endif()
  else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(VCREDIST_FILE "vc_redist.x64${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
    # maybe add arm architecture
    else()
      set(VCREDIST_FILE "vc_redist.x86${CMAKE_EXECUTABLE_SUFFIX}" CACHE INTERNAL "")
    endif()
  endif()
  install(FILES "${EXTERNAL_LIBRARY_FOLDER}/vcredist/${VCREDIST_FILE}" DESTINATION "${INSTALL_BIN_DIR}")
endif(WIN32)

if(NOT WIN32)
  # needs speexdsp
  ExternalProject_Add(codec2
    GIT_REPOSITORY https://github.com/drowe67/codec2.git
    GIT_TAG ${CODEC2_TAG}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/codec2"
    CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
    INSTALL_COMMAND ""
    TEST_COMMAND ""
  )
  ExternalProject_Get_Property(codec2 source_dir binary_dir)
  set(CODEC2_FOUND ON CACHE INTERNAL "")
  set(CODEC2_INCLUDE_DIR "${source_dir}/src" "${binary_dir}" CACHE INTERNAL "")
  if(WIN32)
    set(CODEC2_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/codec2.lib" CACHE INTERNAL "")
    makeCopy("${source_dir}/src" "${source_dir}/src/codec2" codec2)
  elseif(LINUX)
    set(CODEC2_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libcodec2${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    makeLink("${source_dir}/src" "${source_dir}/src/codec2" codec2)
  else()
    set(CODEC2_LIBRARIES "${binary_dir}/src/libcodec2${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    # some source include "codec2/comp.h" and some without codec2
    makeLink("${source_dir}/src" "${source_dir}/src/codec2" codec2)
  endif()
endif()

# needs boost
ExternalProject_Add(cm256cc
    GIT_REPOSITORY https://github.com/f4exb/cm256cc.git
    GIT_TAG ${CM256CC_TAG}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/cm256cc"
    CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
      -DBUILD_TOOLS=OFF
      -DBOOST_ROOT="${BOOST_ROOT}"
      -DBoost_INCLUDE_DIR=${Boost_INCLUDE_DIRS}
      -DBoost_INCLUDE_DIRS=${Boost_INCLUDE_DIRS}
      -DENABLE_DISTRIBUTION=ON
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
ExternalProject_Get_Property(cm256cc source_dir binary_dir)
set(CM256CC_FOUND ON CACHE INTERNAL "")
# we need cm256cc/library.h
set(CM256CC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/cm256cc/src" CACHE INTERNAL "")
if(WIN32)
  set(CM256CC_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/cm256cc.lib" CACHE INTERNAL "")
  install(FILES "${SDRANGEL_BINARY_BIN_DIR}/cm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
elseif(LINUX)
  set(CM256CC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libcm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
elseif(APPLE)
  set(CM256CC_LIBRARIES "${binary_dir}/libcm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
  install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
  FILES_MATCHING PATTERN "libcm256cc*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}")
endif()

set(USE_MBELIB OFF)
ExternalProject_Add(mbelib
    GIT_REPOSITORY https://github.com/szechyjs/mbelib.git
    GIT_TAG ${MBELIB_TAG}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/mbelib"
    CMAKE_ARGS -DDISABLE_TEST=ON ${DEFAULT_OUTPUT_DIRECTORIES}
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
ExternalProject_Get_Property(mbelib source_dir binary_dir)
set(USE_MBELIB ON)
set(DSDCC_DEPENDS mbelib)
set(LIBMBE_FOUND ON CACHE INTERNAL "")
set(LIBMBE_INCLUDE_DIR "${source_dir}")
if(WIN32)
  set(LIBMBE_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/mbe.lib")
  install(FILES "${SDRANGEL_BINARY_BIN_DIR}/mbe${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
elseif(LINUX)
  set(LIBMBE_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libmbe${CMAKE_SHARED_LIBRARY_SUFFIX}")
elseif(APPLE)
  set(LIBMBE_LIBRARIES "${binary_dir}/libmbe${CMAKE_SHARED_LIBRARY_SUFFIX}")
  install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
  FILES_MATCHING PATTERN "libmbe*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}")
endif()

# Works on MacOS with a dummy serialDV
ExternalProject_Add(serialdv
    GIT_REPOSITORY https://github.com/f4exb/serialDV.git
    GIT_TAG ${SERIALDV_TAG}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/serialdv"
    CMAKE_ARGS -DBUILD_TOOL=OFF ${DEFAULT_OUTPUT_DIRECTORIES}
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
ExternalProject_Get_Property(serialdv source_dir binary_dir)
set(DSDCC_DEPENDS ${DSDCC_DEPENDS} serialdv)
set(LIBSERIALDV_FOUND ON CACHE INTERNAL "")
set(LIBSERIALDV_INCLUDE_DIR "${source_dir}" CACHE INTERNAL "")
if(WIN32)
    set(LIBSERIALDV_LIBRARY "${SDRANGEL_BINARY_LIB_DIR}/serialdv.lib" CACHE INTERNAL "")
    makeCopyDir("${source_dir}" "${source_dir}/dsp" serialdv)
    install(FILES "${SDRANGEL_BINARY_BIN_DIR}/serialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
elseif(LINUX)
    set(LIBSERIALDV_LIBRARY "${EXTERNAL_BUILD_LIBRARIES}/lib/libserialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    # because sdrbase/dsp/dvserialworker.h use dsp/dvcontroller.h
    # so we need a link
    makeLink("${source_dir}" "${source_dir}/dsp" serialdv)
elseif(APPLE)
    set(LIBSERIALDV_LIBRARY "${binary_dir}/libserialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
    # because sdrbase/dsp/dvserialworker.h use dsp/dvcontroller.h
    # so we need a link
    makeLink("${source_dir}" "${source_dir}/dsp" serialdv)
    install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
    FILES_MATCHING PATTERN "libserialdv*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}")
endif()

ExternalProject_Add(dsdcc
    GIT_REPOSITORY https://github.com/f4exb/dsdcc.git
    GIT_TAG ${DSDCC_TAG}
    DEPENDS ${DSDCC_DEPENDS}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/dsdcc"
    CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
      -DBUILD_TOOL=OFF -DUSE_MBELIB=${USE_MBELIB}
      -DLIBMBE_INCLUDE_DIR=${LIBMBE_INCLUDE_DIR}
      -DLIBMBE_LIBRARY=${LIBMBE_LIBRARIES}
      -DLIBSERIALDV_INCLUDE_DIR=${LIBSERIALDV_INCLUDE_DIR}
      -DLIBSERIALDV_LIBRARY=${LIBSERIALDV_LIBRARY}
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
ExternalProject_Get_Property(dsdcc source_dir binary_dir)
set(LIBDSDCC_FOUND ON CACHE INTERNAL "")
set(LIBDSDCC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/dsdcc/src" CACHE INTERNAL "")
if(WIN32)
  set(LIBDSDCC_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/dsdcc.lib" CACHE INTERNAL "")
  install(FILES "${SDRANGEL_BINARY_BIN_DIR}/dsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
elseif(LINUX)
  set(LIBDSDCC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libdsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
elseif(APPLE)
  set(LIBDSDCC_LIBRARIES "${binary_dir}/libdsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
  install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
  FILES_MATCHING PATTERN "libdsdcc*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/")
endif()

# requirements needed by many packages on windows
if(WIN32)
  ExternalProject_Add(pthreads4w
    GIT_REPOSITORY https://git.code.sf.net/p/pthreads4w/code
    GIT_TAG ${PTHREADS4W_TAG}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/pthreads4w"
    PATCH_COMMAND ${CMAKE_COMMAND} -E touch "<SOURCE_DIR>/COPYING.LIB"
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND nmake VC
    INSTALL_COMMAND ""
    TEST_COMMAND ""
  )
  ExternalProject_Get_Property(pthreads4w source_dir binary_dir)
  set(PTHREADS4W_DEPENDS pthreads4w)
  set(PTHREADS4W_FOUND ON CACHE INTERNAL "")
  set(PTHREADS4W_INCLUDE_DIR "${source_dir}" CACHE INTERNAL "")
  set(PTHREADS4W_LIBRARIES "${source_dir}/pthreadVC2.lib" CACHE INTERNAL "")
  makeCopy("${source_dir}/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" "${SDRANGEL_BINARY_BIN_DIR}/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" pthreads4w)
  # needed by bladerf
  makeCopy("${source_dir}/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" "${source_dir}/dll/x64/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" pthreads4w)
  install(FILES "${source_dir}/pthreadVC2${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
endif(WIN32)

# Device interface libraries. Use external libraries build for these. Only Linux for now.
if (LINUX)
    # apt install liblimesuite-dev (only on ubuntu 18.04). Can be picky on version though so let's build it.
    # needs pkgconfig and libusb
    ExternalProject_Add(limesuite
        GIT_REPOSITORY https://github.com/myriadrf/LimeSuite.git
        GIT_TAG ${LIMESUITE_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/limesuite"
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES} -DLIME_SUITE_EXTVER=release -DENABLE_GUI=OFF -DENABLE_NOVENARF7=OFF -DENABLE_SOAPY_LMS7=OFF -DENABLE_OCTAVE=OFF -DENABLE_SIMD_FLAGS=SSE3
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    ExternalProject_Get_Property(limesuite source_dir)
    set(LIMESUITE_FOUND ON CACHE INTERNAL "")
    set(LIMESUITE_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/limesuite/src/limesuite/src" CACHE INTERNAL "")
    set(LIMESUITE_LIBRARY "${EXTERNAL_BUILD_LIBRARIES}/lib/libLimeSuite.so" CACHE INTERNAL "")
    makeCopyFile("${source_dir}/src/limeRFE/limeRFE.h" "${source_dir}/src/lime/limeRFE.h" limesuite)

    # apt install libbladerf-dev
    # needs pkgconfig and libusb
    ExternalProject_Add(bladerf
        GIT_REPOSITORY https://github.com/Nuand/bladeRF.git
        GIT_TAG ${BLADERF_TAG}
        GIT_SUBMODULES
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/bladerf"
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
        SOURCE_SUBDIR host/
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    set(LIBBLADERF_FOUND ON CACHE INTERNAL "")
    set(LIBBLADERF_INCLUDE_DIRS "${EXTERNAL_BUILD_LIBRARIES}/bladerf/src/bladerf/host/libraries/libbladeRF/include" CACHE INTERNAL "")
    set(LIBBLADERF_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libbladeRF.so" CACHE INTERNAL "")

    # apt install libiio-dev
    # needs pkgconfig, libusb, libxml2, lzma, xz, libiconv
    ExternalProject_Add(libiio
        GIT_REPOSITORY https://github.com/analogdevicesinc/libiio.git
        GIT_TAG ${LIBIIO_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libiio"
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES} -DOSX_PACKAGE=OFF -DWITH_DOC=OFF
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    set(LIBIIO_FOUND ON CACHE INTERNAL "")
    set(LIBIIO_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/libiio/src/libiio" CACHE INTERNAL "")
    set(LIBIIO_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libiio.so" CACHE INTERNAL "")

    # apt install libairspyhf-dev (only on ubuntu 18.04)
    # needs libusb and pkgconfig
    ExternalProject_Add(airspyhf
        GIT_REPOSITORY https://github.com/airspy/airspyhf.git
        GIT_TAG ${AIRSPYHF_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/airspyhf"
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    ExternalProject_Get_Property(airspyhf source_dir)
    set(LIBAIRSPYHF_FOUND ON CACHE INTERNAL "")
    set(LIBAIRSPYHF_INCLUDE_DIR "${source_dir}/.." CACHE INTERNAL "")
    set(LIBAIRSPYHF_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libairspyhf.so" CACHE INTERNAL "")
    makeLink("${source_dir}/libairspyhf/src" "${source_dir}/../libairspyhf" airspyhf)

    # needs pkgconfig, libusb, autoconf, automake and libtool
    ExternalProject_Add(perseus
        GIT_REPOSITORY https://github.com/f4exb/libperseus-sdr.git
        GIT_TAG ${PERSEUS_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/perseus"
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    set(LIBPERSEUS_FOUND ON CACHE INTERNAL "")
    set(LIBPERSEUS_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/perseus/src/perseus" CACHE INTERNAL "")
    set(LIBPERSEUS_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libperseus-sdr.so" CACHE INTERNAL "")

    # apt install librtlsdr-dev
    # needs pkgconfig and libusb
    ExternalProject_Add(rtlsdr
        GIT_REPOSITORY https://github.com/osmocom/rtl-sdr.git
        GIT_TAG ${RTLSDR_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/rtlsdr"
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES} -DDETACH_KERNEL_DRIVER=ON -DINSTALL_UDEV_RULES=${RTLSDR_UDEV}
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    set(LIBRTLSDR_FOUND ON CACHE INTERNAL "")
    set(LIBRTLSDR_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/rtlsdr/src/rtlsdr/include" CACHE INTERNAL "")
    set(LIBRTLSDR_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/librtlsdr.so" CACHE INTERNAL "")
endif(LINUX)

if(WIN32 OR APPLE)
  if(ENABLE_RTLSDR)
    if(WIN32)
      set(RTLSDR_LIBUSB_INCLUDE "${LIBUSB_INCLUDE_DIR}/libusb-1.0")
    else()
      set(RTLSDR_LIBUSB_INCLUDE "${LIBUSB_INCLUDE_DIR}")
    endif()
    # needs pkgconfig and libusb
    ExternalProject_Add(rtlsdr
      GIT_REPOSITORY https://github.com/osmocom/rtl-sdr.git
      GIT_TAG ${RTLSDR_TAG}
      DEPENDS ${PTHREADS4W_DEPENDS}
      PREFIX "${EXTERNAL_BUILD_LIBRARIES}/rtlsdr"
      CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
          -DINSTALL_UDEV_RULES=${RTLSDR_UDEV}
          -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
          -DLIBUSB_INCLUDE_DIR=${RTLSDR_LIBUSB_INCLUDE}
          -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
          -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
      INSTALL_COMMAND ""
      TEST_COMMAND ""
    )
    ExternalProject_Get_Property(rtlsdr source_dir binary_dir)
    set(LIBRTLSDR_FOUND ON CACHE INTERNAL "")
    set(LIBRTLSDR_INCLUDE_DIR "${source_dir}/include" CACHE INTERNAL "")
    if(WIN32)
      set(LIBRTLSDR_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/rtlsdr.lib" CACHE INTERNAL "")
      install(FILES "${SDRANGEL_BINARY_BIN_DIR}/rtlsdr${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif(APPLE)
      set(LIBRTLSDR_LIBRARIES "${binary_dir}/src/librtlsdr${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      install(DIRECTORY "${binary_dir}/src/" DESTINATION "${INSTALL_LIB_DIR}"
        FILES_MATCHING PATTERN "librtlsdr*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/src/")
    endif()
  endif(ENABLE_RTLSDR)

  if(ENABLE_LIMESUITE)
    # needs pkgconfig, libusb
    ExternalProject_Add(limesuite
      GIT_REPOSITORY https://github.com/myriadrf/LimeSuite.git
      GIT_TAG ${LIMESUITE_TAG}
      PREFIX "${EXTERNAL_BUILD_LIBRARIES}/limesuite"
      CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
          -DLIME_SUITE_EXTVER=release
          -DENABLE_GUI=OFF
          -DENABLE_NOVENARF7=OFF
          -DENABLE_SOAPY_LMS7=OFF
          -DENABLE_OCTAVE=OFF
          -DENABLE_QUICKTEST=OFF
          -DENABLE_EXAMPLES=OFF
          -DENABLE_SIMD_FLAGS=SSE3
          -DFX3_SDK_PATH=${FX3SDK_DIR}
      INSTALL_COMMAND ""
      TEST_COMMAND ""
    )
    ExternalProject_Get_Property(limesuite source_dir binary_dir)
    set(LIMESUITE_FOUND ON CACHE INTERNAL "")
    set(LIMESUITE_INCLUDE_DIR "${source_dir}/src" CACHE INTERNAL "")
    if(WIN32)
      set(LIMESUITE_LIBRARY "${SDRANGEL_BINARY_LIB_DIR}/LimeSuite.lib" CACHE INTERNAL "")
      install(FILES "${SDRANGEL_BINARY_BIN_DIR}/LimeSuite${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
      makeCopy("${source_dir}/src/limeRFE/limeRFE.h" "${source_dir}/src/lime/limeRFE.h" limesuite)
    elseif(APPLE)
      set(LIMESUITE_LIBRARY "${binary_dir}/src/libLimeSuite${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      install(DIRECTORY "${binary_dir}/src/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "libLimeSuite*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/src/")
    endif()
  endif(ENABLE_LIMESUITE)

  if(ENABLE_SOAPYSDR)
    ExternalProject_Add(soapysdr
      GIT_REPOSITORY https://github.com/pothosware/SoapySDR.git
      GIT_TAG ${SOAPYSDR_TAG}
      PREFIX "${EXTERNAL_BUILD_LIBRARIES}/soapysdr"
      CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
          -DENABLE_PYTHON=OFF
          -DENABLE_PYTHON3=OFF
          -DENABLE_TESTS=OFF
      INSTALL_COMMAND ""
      TEST_COMMAND ""
    )
    ExternalProject_Get_Property(soapysdr source_dir binary_dir)
    set(SOAPYSDR_FOUND ON CACHE INTERNAL "")
    set(SOAPYSDR_INCLUDE_DIR "${source_dir}/include" CACHE INTERNAL "")
    if(WIN32)
      set(SOAPYSDR_LIBRARY "${SDRANGEL_BINARY_LIB_DIR}/SoapySDR.lib" CACHE INTERNAL "")
      install(FILES "${SDRANGEL_BINARY_BIN_DIR}/SoapySDR${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif(APPLE)
      set(SOAPYSDR_LIBRARY "${binary_dir}/lib/libSoapySDR${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      install(DIRECTORY "${binary_dir}/lib/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "libSoapySDR*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/lib/")
    endif()
  endif(ENABLE_SOAPYSDR)

  if(ENABLE_AIRSPY)
    if(WIN32)
      set(AIRSPY_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR}/libusb-1.0)
    else()
      set(AIRSPY_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR})
    endif()
    # needs libusb, pthreads
    ExternalProject_Add(airspy
      GIT_REPOSITORY https://github.com/airspy/airspyone_host
      GIT_TAG ${AIRSPY_TAG}
      DEPENDS ${PTHREADS4W_DEPENDS}
      PREFIX "${EXTERNAL_BUILD_LIBRARIES}/airspy"
      CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
          -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
          -DLIBUSB_INCLUDE_DIR=${AIRSPY_LIBUSB_INCLUDE_DIR}
          -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
          -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
      INSTALL_COMMAND ""
      TEST_COMMAND ""
    )
    add_dependencies(airspy pthreads4w)
    ExternalProject_Get_Property(airspy source_dir binary_dir)
    set(LIBAIRSPY_FOUND ON CACHE INTERNAL "")
    set(LIBAIRSPY_INCLUDE_DIR "${source_dir}/libairspy/src" CACHE INTERNAL "")
    if(WIN32)
      set(LIBAIRSPY_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/airspy.lib" CACHE INTERNAL "")
      makeCopyDir("${source_dir}/libairspy/src" "${source_dir}/libairspy/src/libairspy" airspy)
      makeCopy("${binary_dir}/airspy-tools/src/airspy${CMAKE_SHARED_LIBRARY_SUFFIX}" "${SDRANGEL_BINARY_BIN_DIR}/airspy${CMAKE_SHARED_LIBRARY_SUFFIX}" airspy)
      install(FILES "${SDRANGEL_BINARY_BIN_DIR}/airspy${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif(APPLE)
      set(LIBAIRSPY_LIBRARIES "${binary_dir}/libairspy/src/libairspy${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      makeLink("${source_dir}/libairspy/src" "${source_dir}/libairspy/src/libairspy" airspy)
      install(DIRECTORY "${binary_dir}/libairspy/src/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "libairspy*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/libairspy/src")
    endif()
  endif(ENABLE_AIRSPY)

  if(ENABLE_AIRSPYHF)
    if(WIN32)
      set(AIRSPYHF_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR}/libusb-1.0)
    else()
      set(AIRSPYHF_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR})
    endif()
    # needs pkgconfig, libusb, pthreads
    ExternalProject_Add(airspyhf
      GIT_REPOSITORY https://github.com/airspy/airspyhf.git
      GIT_TAG ${AIRSPYHF_TAG}
      DEPENDS ${PTHREADS4W_DEPENDS}
      PREFIX "${EXTERNAL_BUILD_LIBRARIES}/airspyhf"
      CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
          -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
          -DLIBUSB_INCLUDE_DIR=${AIRSPYHF_LIBUSB_INCLUDE_DIR}
          -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
          -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
      INSTALL_COMMAND ""
      TEST_COMMAND ""
    )
    add_dependencies(airspyhf pthreads4w)
    ExternalProject_Get_Property(airspyhf source_dir binary_dir)
    set(LIBAIRSPYHF_FOUND ON CACHE INTERNAL "")
    set(LIBAIRSPYHF_INCLUDE_DIR "${source_dir}/libairspyhf/src" CACHE INTERNAL "")
    if(WIN32)
      set(LIBAIRSPYHF_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/airspyhf.lib" CACHE INTERNAL "")
      makeCopyDir("${source_dir}/libairspyhf/src" "${source_dir}/libairspyhf/src/libairspyhf" airspyhf)
      install(FILES "${SDRANGEL_BINARY_BIN_DIR}/airspyhf${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif(APPLE)
      set(LIBAIRSPYHF_LIBRARIES "${binary_dir}/libairspyhf/src/libairspyhf${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      makeLink("${source_dir}/libairspyhf/src" "${source_dir}/libairspyhf/src/libairspyhf" airspyhf)
      install(DIRECTORY "${binary_dir}/libairspyhf/src/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "libairspyhf*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/libairspyhf/src")
    endif()
  endif(ENABLE_AIRSPYHF)

  if(ENABLE_HACKRF)
    # SOURCE_SUBDIR require cmake 3.7.0
    # otherwise use CONFIGURE_COMMAND
    cmake_minimum_required(VERSION 3.7.0)
    # needs pkgconfig, libusb, fftw, pthreads
    if(WIN32)
      set(HACKRF_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR}/libusb-1.0)
    else()
      set(HACKRF_LIBUSB_INCLUDE_DIR ${LIBUSB_INCLUDE_DIR})
    endif()
    ExternalProject_Add(hackrf
      GIT_REPOSITORY https://github.com/mossmann/hackrf.git
      GIT_TAG ${HACKRF_TAG}
      GIT_SUBMODULES ""
      DEPENDS ${PTHREADS4W_DEPENDS}
      PREFIX "${EXTERNAL_BUILD_LIBRARIES}/hackrf"
      SOURCE_SUBDIR "host/libhackrf"
      CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
          -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
          -DLIBUSB_INCLUDE_DIR=${HACKRF_LIBUSB_INCLUDE_DIR}
          -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
          -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
          -DFFTW_INCLUDES=${FFTW3F_INCLUDE_DIRS}
          -DFFTW_LIBRARIES=${FFTW3F_LIBRARIES}
      INSTALL_COMMAND ""
      TEST_COMMAND ""
    )
    add_dependencies(hackrf pthreads4w)
    ExternalProject_Get_Property(hackrf source_dir binary_dir)
    set(LIBHACKRF_FOUND ON CACHE INTERNAL "")
    set(LIBHACKRF_INCLUDE_DIR "${source_dir}/host/libhackrf" CACHE INTERNAL "")
    if(WIN32)
      set(LIBHACKRF_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/hackrf.lib" CACHE INTERNAL "")
      # include "libhackrf/hackrf.h"
      makeCopyDir("${source_dir}/host/libhackrf/src" "${source_dir}/host/libhackrf/libhackrf" hackrf)
      install(FILES "${SDRANGEL_BINARY_BIN_DIR}/hackrf${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif(APPLE)
      set(LIBHACKRF_LIBRARIES "${binary_dir}/src/libhackrf${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      # include "libhackrf/hackrf.h"
      makeLink("${source_dir}/host/libhackrf/src" "${source_dir}/host/libhackrf/libhackrf" hackrf)
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/")
      install(DIRECTORY "${binary_dir}/src/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "libhackrf*${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()
  endif(ENABLE_HACKRF)

  if(ENABLE_IIO)
    if(WIN32)
      ExternalProject_Add(libxml2
        GIT_REPOSITORY https://github.com/GNOME/libxml2.git
        GIT_TAG ${LIBXML2_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libxml2"
        BUILD_IN_SOURCE 1
        CONFIGURE_COMMAND cd <SOURCE_DIR>/win32 && cscript configure.js compiler=msvc iconv=no
        BUILD_COMMAND cd <SOURCE_DIR>/win32 && nmake -f Makefile.msvc
        INSTALL_COMMAND ""
        TEST_COMMAND ""
      )
    elseif(APPLE)
      set(LIBXML2_PATCH_COMMAND /usr/bin/sed -i '' "s/libtoolize/glibtoolize/g" autogen.sh)
      ExternalProject_Add(libxml2
        GIT_REPOSITORY https://github.com/GNOME/libxml2.git
        GIT_TAG ${LIBXML2_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libxml2"
        BUILD_IN_SOURCE 1
        PATCH_COMMAND ${LIBXML2_PATCH_COMMAND}
        CONFIGURE_COMMAND ./autogen.sh
        BUILD_COMMAND make
        INSTALL_COMMAND ""
        TEST_COMMAND ""
      )
    endif()
    ExternalProject_Get_Property(libxml2 source_dir binary_dir)
    set(LIBXML2_DEPENDS libxml2)
    set(LIBXML2_FOUND ON CACHE INTERNAL "")
    set(LIBXML2_INCLUDE_DIR "${source_dir}/include" CACHE INTERNAL "")
    if(WIN32)
      set(LIBXML2_LIBRARIES "${source_dir}/win32/bin.msvc/libxml2.lib" CACHE INTERNAL "")
      makeCopy("${source_dir}/win32/bin.msvc/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}" "${SDRANGEL_BINARY_BIN_DIR}/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}" libxml2)
      install(FILES "${source_dir}/win32/bin.msvc/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif(APPLE)
      set(LIBXML2_LIBRARIES "${source_dir}/.libs/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      install(DIRECTORY "${source_dir}/.libs/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "libxml2*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${source_dir}/.libs")
    endif()
    # needs pkgconfig, libusb, pthreads, libxml2
    ExternalProject_Add(libiio
      GIT_REPOSITORY https://github.com/analogdevicesinc/libiio.git
      GIT_TAG ${LIBIIO_TAG}
      DEPENDS ${PTHREADS4W_DEPENDS} ${LIBXML2_DEPENDS}
      PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libiio"
      CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
          -DLIBUSB_LIBRARIES=${LIBUSB_LIBRARIES}
          -DLIBUSB_INCLUDE_DIR=${LIBUSB_INCLUDE_DIR}
          -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
          -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
          -DLIBXML2_INCLUDE_DIR=${LIBXML2_INCLUDE_DIR}
          -DLIBXML2_LIBRARIES=${LIBXML2_LIBRARIES}
          -DCSHARP_BINDINGS=OFF
          -DPYTHON_BINDINGS=OFF
          -DWITH_MATLAB_BINDINGS_API=OFF
          -DOSX_PACKAGE=OFF
      INSTALL_COMMAND ""
      TEST_COMMAND ""
    )
    add_dependencies(libiio pthreads4w)
    ExternalProject_Get_Property(libiio source_dir binary_dir)
    set(LIBIIO_FOUND ON CACHE INTERNAL "")
    set(LIBIIO_INCLUDE_DIR "${source_dir}" CACHE INTERNAL "")
    if(WIN32)
      set(LIBIIO_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/libiio.lib" CACHE INTERNAL "")
      install(FILES "${SDRANGEL_BINARY_BIN_DIR}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif(APPLE)
      set(LIBIIO_LIBRARIES "${binary_dir}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      install(DIRECTORY "${binary_dir}/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "libiio*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      string(REPLACE "v" "" IIO_VERSION ${IIO_TAG})
      makeCopy("${binary_dir}/libiio.framework/Versions/${IIO_VERSION}/libiio" "${binary_dir}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}" libiio)
      add_custom_command(TARGET libiio POST_BUILD COMMAND install_name_tool -id "${binary_dir}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}" "${binary_dir}/libiio${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}")
    endif()
  endif(ENABLE_IIO)

  if(ENABLE_BLADERF)
    # needs pkgconfig, libusb, pthreads
    ExternalProject_Add(bladerf
      GIT_REPOSITORY https://github.com/Nuand/bladeRF.git
      GIT_TAG ${BLADERF_TAG}
      DEPENDS ${PTHREADS4W_DEPENDS}
      PREFIX "${EXTERNAL_BUILD_LIBRARIES}/bladerf"
      CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORIES}
          -DPKG_CONFIG_EXECUTABLE=${PKG_CONFIG_EXECUTABLE}
          -DENABLE_BACKEND_USB=ON
          -DENABLE_BACKEND_LIBUSB=ON
          -DLIBUSB_HEADER_FILE=${LIBUSB_INCLUDE_DIR}/libusb-1.0/libusb.h
          -Dusb_LIBRARY=${LIBUSB_LIBRARIES}
          -DLIBUSB_PATH=${EXTERNAL_LIBRARY_FOLDER}/libusb
          -DTHREADS_PTHREADS_INCLUDE_DIR=${PTHREADS4W_INCLUDE_DIR}
          -DTHREADS_PTHREADS_WIN32_LIBRARY=${PTHREADS4W_LIBRARIES}
          -DLIBPTHREADSWIN32_PATH=${PTHREADS4W_INCLUDE_DIR}
          -DENABLE_FX3_BUILD=OFF
          -DENABLE_HOST_BUILD=ON
          -DENABLE_BACKEND_CYAPI=OFF
          -DTREAT_WARNINGS_AS_ERRORS=OFF
      INSTALL_COMMAND ""
      TEST_COMMAND ""
    )
    add_dependencies(bladerf pthreads4w)
    ExternalProject_Get_Property(bladerf source_dir binary_dir)
    set(LIBBLADERF_FOUND ON CACHE INTERNAL "")
    set(LIBBLADERF_INCLUDE_DIRS "${source_dir}/host/libraries/libbladeRF/include" CACHE INTERNAL "")
    if(WIN32)
      set(LIBBLADERF_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/bladeRF.lib" CACHE INTERNAL "")
      install(FILES "${SDRANGEL_BINARY_BIN_DIR}/bladeRF${CMAKE_SHARED_LIBRARY_SUFFIX}" DESTINATION "${INSTALL_LIB_DIR}")
    elseif(APPLE)
      set(LIBBLADERF_LIBRARIES "${binary_dir}/host/output/libbladeRF${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      install(DIRECTORY "${binary_dir}/host/output/" DESTINATION "${INSTALL_LIB_DIR}"
      FILES_MATCHING PATTERN "libbladeRF*${CMAKE_SHARED_LIBRARY_SUFFIX}")
      set(MACOS_EXTERNAL_LIBS_FIXUP "${MACOS_EXTERNAL_LIBS_FIXUP};${binary_dir}/host/output")
    endif()
  endif(ENABLE_BLADERF)
endif(WIN32 OR APPLE)
