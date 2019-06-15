# Many idea are exposed at
#  https://stackoverflow.com/questions/15175318/cmake-how-to-build-external-projects-and-include-their-targets
#  https://crascit.com/2015/07/25/cmake-gtest/
#
# in essence, we need a way to build external libraries before use it;
# we choose add_dependencies() to the target.
# other viable solution is to use execute_process() during config
# but make the configuration dirty

# which version/tag/checkout to use
set(CODEC2_TAG 76a20416d715ee06f8b36a9953506876689a3bd2)
set(CM256CC_TAG f21e8bc1e9afdb0b28672743dcec111aec1d32d9)
set(MBELIB_TAG "debian/1.3.0")
set(SERIALDV_TAG abd65a0fc2d5339808194862d14119b0cba70424)
set(DSDCC_TAG a0f46941383120904c7211cf0fd94f04e2abfe73)
set(LIMESUITE_TAG "v19.01.0")
set(BLADERF_TAG 896d2431b3a35a4b31b6e729386202ebf5fdc5c0)
set(LIBIIO_TAG 826563e41b5ce9890b75506f672017de8d76d52d)
set(AIRSPYHF_TAG "1.1.5")
set(PERSEUS_TAG afefa23e3140ac79d845acb68cf0beeb86d09028)


# For some external project macros
include(ExternalProject)

# default build destination in windows to avoid Release/Debug folder
if(WIN32)
  # the build will be output on the same folder of sdrangel
  set(DEFAULT_OUTPUT_DIRECTORY
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_BIN_DIR}
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_BIN_DIR}
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_BIN_DIR}
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG=${SDRANGEL_BINARY_LIB_DIR}
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=${SDRANGEL_BINARY_LIB_DIR})
elseif(LINUX)
set(DEFAULT_OUTPUT_DIRECTORY
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

# probably build with mingw64 on windows
if(NOT WIN32)
  # needs boost
  ExternalProject_Add(codec2
    GIT_REPOSITORY https://github.com/drowe67/codec2.git
    GIT_TAG ${CODEC2_TAG}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/codec2"
    CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORY}
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
    CMAKE_ARGS -DBUILD_TOOLS=OFF -DBOOST_ROOT="${BOOST_ROOT}" -DENABLE_DISTRIBUTION=ON ${DEFAULT_OUTPUT_DIRECTORY}
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
ExternalProject_Get_Property(cm256cc source_dir binary_dir)
set(CM256CC_FOUND ON CACHE INTERNAL "")
# we need cm256cc/library.h
set(CM256CC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/cm256cc/src" CACHE INTERNAL "")
if(WIN32)
  set(CM256CC_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/cm256cc.lib" CACHE INTERNAL "")
elseif(LINUX)
  set(CM256CC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libcm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
else()
  set(CM256CC_LIBRARIES "${binary_dir}/libcm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
endif()

set(USE_MBELIB OFF)
if(NOT WIN32)
ExternalProject_Add(mbelib
    GIT_REPOSITORY https://github.com/szechyjs/mbelib.git
    GIT_TAG ${MBELIB_TAG}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/mbelib"
    CMAKE_ARGS -DDISABLE_TEST=ON ${DEFAULT_OUTPUT_DIRECTORY}
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
elseif(LINUX)
  set(LIBMBE_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libmbe${CMAKE_SHARED_LIBRARY_SUFFIX}")
else()
  set(LIBMBE_LIBRARIES "${binary_dir}/libmbe${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()
endif(NOT WIN32)

if(WIN32 OR LINUX)
    ExternalProject_Add(serialdv
        GIT_REPOSITORY https://github.com/f4exb/serialDV.git
        GIT_TAG ${SERIALDV_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/serialdv"
        CMAKE_ARGS -DBUILD_TOOL=OFF ${DEFAULT_OUTPUT_DIRECTORY}
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    ExternalProject_Get_Property(serialdv source_dir binary_dir)
    set(DSDCC_DEPENDS ${DSDCC_DEPENDS} serialdv)
    set(LIBSERIALDV_FOUND ON CACHE INTERNAL "")
    set(LIBSERIALDV_INCLUDE_DIR "${source_dir}" CACHE INTERNAL "")
    if(WIN32)
      set(LIBSERIALDV_LIBRARY "${SDRANGEL_BINARY_LIB_DIR}/serialdv.lib" CACHE INTERNAL "")
      makeCopy("${source_dir}" "${source_dir}/dsp" serialdv)
    elseif(LINUX)
      set(LIBSERIALDV_LIBRARY "${EXTERNAL_BUILD_LIBRARIES}/lib/libserialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      # because sdrbase/dsp/dvserialworker.h use dsp/dvcontroller.h
      # so we need a link
      makeLink("${source_dir}" "${source_dir}/dsp" serialdv)
    else()
      set(LIBSERIALDV_LIBRARY "${binary_dir}/libserialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
      # because sdrbase/dsp/dvserialworker.h use dsp/dvcontroller.h
      # so we need a link
      makeLink("${source_dir}" "${source_dir}/dsp" serialdv)
    endif()
endif()

ExternalProject_Add(dsdcc
    GIT_REPOSITORY https://github.com/f4exb/dsdcc.git
    GIT_TAG ${DSDCC_TAG}
    DEPENDS ${DSDCC_DEPENDS}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/dsdcc"
    CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORY} -DBUILD_TOOL=OFF -DUSE_MBELIB=${USE_MBELIB} -DLIBMBE_INCLUDE_DIR=${LIBMBE_INCLUDE_DIR} -DLIBMBE_LIBRARY=${LIBMBE_LIBRARIES} -DLIBSERIALDV_INCLUDE_DIR=${LIBSERIALDV_INCLUDE_DIR} -DLIBSERIALDV_LIBRARY=${LIBSERIALDV_LIBRARY}
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
ExternalProject_Get_Property(dsdcc source_dir binary_dir)
set(LIBDSDCC_FOUND ON CACHE INTERNAL "")
set(LIBDSDCC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/dsdcc/src" CACHE INTERNAL "")
if(WIN32)
  set(LIBDSDCC_LIBRARIES "${SDRANGEL_BINARY_LIB_DIR}/dsdcc.lib" CACHE INTERNAL "")
elseif(LINUX)
  set(LIBDSDCC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libdsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
else()
  set(LIBDSDCC_LIBRARIES "${binary_dir}/libdsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
endif()

# Device interface libraries. Use external libraries build for these. Only Linux for now.
if (LINUX)
    # apt install liblimesuite-dev (only on ubuntu 18.04). Can be picky on version though so let's build it.
    # needs pkgconfig and libusb
    ExternalProject_Add(limesuite
        GIT_REPOSITORY https://github.com/myriadrf/LimeSuite.git
        GIT_TAG ${LIMESUITE_TAG}
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/limesuite"
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORY} -DLIME_SUITE_EXTVER=release -DENABLE_GUI=OFF -DENABLE_NOVENARF7=OFF -DENABLE_SOAPY_LMS7=OFF -DENABLE_OCTAVE=OFF -DENABLE_SIMD_FLAGS=SSE3
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    set(LIMESUITE_FOUND ON CACHE INTERNAL "")
    set(LIMESUITE_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/limesuite/src/limesuite/src" CACHE INTERNAL "")
    set(LIMESUITE_LIBRARY "${EXTERNAL_BUILD_LIBRARIES}/lib/libLimeSuite.so" CACHE INTERNAL "")

    # apt install libbladerf-dev
    # needs pkgconfig and libusb
    ExternalProject_Add(bladerf
        GIT_REPOSITORY https://github.com/Nuand/bladeRF.git
        GIT_TAG ${BLADERF_TAG}
        GIT_SUBMODULES
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/bladerf"
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORY}
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
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORY} -DOSX_PACKAGE=OFF -DWITH_DOC=OFF
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
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORY}
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
        CMAKE_ARGS ${DEFAULT_OUTPUT_DIRECTORY}
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    set(LIBPERSEUS_FOUND ON CACHE INTERNAL "")
    set(LIBPERSEUS_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/perseus/src/perseus" CACHE INTERNAL "")
    set(LIBPERSEUS_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/lib/libperseus-sdr.so" CACHE INTERNAL "")
endif()

# already on the respository
# TODO decide what to do
if(OFF)
# apt install libcodec2-dev
# needs speexdsp
ExternalProject_Add(codec2
    GIT_REPOSITORY https://github.com/drowe67/codec2.git
    GIT_TAG ff5841a18bfd9df0e8a250dc57fb7388cabccda1
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/codec2"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES} -DCMAKE_BUILD_TYPE=Release -DUNITTEST=OFF
)
# apt install libairspy-dev
# needs libusb and pkgconfig
ExternalProject_Add(airspy
    GIT_REPOSITORY https://github.com/airspy/airspyone_host.git
    GIT_TAG bfb667080936ca5c2d23b3282f5893931ec38d3f
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/airspy"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
)
# apt install libhackrf-dev
# needs pkgconfig, libusb and fftw-3
ExternalProject_Add(hackrf
    GIT_REPOSITORY https://github.com/mossmann/hackrf.git
    GIT_TAG v2018.01.1
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/hackrf"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
    SOURCE_SUBDIR host/
)
# apt install librtlsdr-dev
# needs pkgconfig and libusb
ExternalProject_Add(rtlsdr
    GIT_REPOSITORY https://github.com/osmocom/rtl-sdr.git
    GIT_TAG f68bb2fa772ad94f58c59babd78353667570630b
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/rtlsdr"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
)
# apt install libmirisdr-dev
# needs pkgconfig and libusb
ExternalProject_Add(mirisdr
    GIT_REPOSITORY https://github.com/f4exb/libmirisdr-4.git
    GIT_TAG v1.1.2
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/mirisdr"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
)
# apt install libiio-dev
# needs pkgconfig, libusb, libxml2, lzma, xz, libiconv
ExternalProject_Add(libiio
    GIT_REPOSITORY https://github.com/analogdevicesinc/libiio.git
    GIT_TAG 826563e41b5ce9890b75506f672017de8d76d52d
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/libiio"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES} -DOSX_PACKAGE=OFF -DWITH_DOC=OFF
)
# apt install libsoapysdr-dev (only on ubuntu 18.04)
ExternalProject_Add(soapysdr
    GIT_REPOSITORY https://github.com/pothosware/SoapySDR.git
    GIT_TAG soapy-sdr-0.7.1
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/soapysdr"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES} -DENABLE_PYTHON=OFF -DENABLE_PYTHON3=OFF
)
# TODO: sdrplay is binary only!
# needs soapysdr, sdrplay
ExternalProject_Add(soapysdrplay
    GIT_REPOSITORY https://github.com/pothosware/SoapySDRPlay.git
    GIT_TAG soapy-sdrplay-0.2.0
    DEPENDS soapysdr
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/soapysdrplay"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES} -DCMAKE_BUILD_TYPE=Release
)
endif(OFF)
