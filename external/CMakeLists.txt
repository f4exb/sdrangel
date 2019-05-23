# Many idea are exposed at
#  https://stackoverflow.com/questions/15175318/cmake-how-to-build-external-projects-and-include-their-targets
#  https://crascit.com/2015/07/25/cmake-gtest/
#
# in essence, we need a way to build external libraries before use it;
# we choose add_dependencies() to the target.
# other viable solution is to use execute_process() during config
# but make the configuration dirty

# For some external project macros
include(ExternalProject)

# add the build path to cmake
#list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_BUILD_LIBRARIES}")

# macro that create symbolic links
macro(makeLink src dest target)
  add_custom_command(TARGET ${target} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${src} ${dest} DEPENDS  ${dest} COMMENT "mklink ${src} -> ${dest}")
endmacro()

# needs boost
ExternalProject_Add(codec2
    GIT_REPOSITORY https://github.com/drowe67/codec2.git
    GIT_TAG 7a0187da3ffb06fd90c081216b50bab94d02d046
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/codec2"
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
set(CODEC2_FOUND ON CACHE INTERNAL "")
set(CODEC2_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/codec2/src/codec2/src" CACHE INTERNAL "")
set(CODEC2_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/codec2/src/codec2-build/libcodec2${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")
# some source include "codec2/comp.h" and some without codec2
makeLink("${EXTERNAL_BUILD_LIBRARIES}/codec2/src/codec2/src" "${EXTERNAL_BUILD_LIBRARIES}/codec2/src/codec2/src/codec2" codec2)

# needs boost
ExternalProject_Add(cm256cc
    GIT_REPOSITORY https://github.com/f4exb/cm256cc.git
    GIT_TAG v1.0.5
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/cm256cc"
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
set(CM256CC_FOUND ON CACHE INTERNAL "")
set(CM256CC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/cm256cc/src" CACHE INTERNAL "")
set(CM256CC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/cm256cc/src/cm256cc-build/libcm256cc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")

ExternalProject_Add(mbelib
    GIT_REPOSITORY https://github.com/szechyjs/mbelib.git
    GIT_TAG "debian/1.3.0"
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/mbelib"
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
set(DSDCC_DEPENDS mbelib)
set(LIBMBE_FOUND)
set(LIBMBE_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/mbelib/src/mbelib")
set(LIBMBE_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/mbelib/src/mbelib-build/libmbe${CMAKE_SHARED_LIBRARY_SUFFIX}")

if(WIN32 OR LINUX)
    ExternalProject_Add(serialdv
        GIT_REPOSITORY https://github.com/f4exb/serialDV.git
        GIT_TAG 89c841ce5b24b8b75c329d6330ec216773f002d2
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/serialdv"
        INSTALL_COMMAND ""
        TEST_COMMAND ""
    )
    set(DSDCC_DEPENDS ${DSDCC_DEPENDS} serialdv)
    set(LIBSERIALDV_FOUND ON CACHE INTERNAL "")
    set(LIBSERIALDV_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/serialdv/src/serialdv" CACHE INTERNAL "")
    set(LIBSERIALDV_LIBRARY "${EXTERNAL_BUILD_LIBRARIES}/serialdv/src/serialdv-build/libserialdv${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")

    # because sdrbase/dsp/dvserialworker.h use dsp/dvcontroller.h
    # so we need a link
    makeLink("${EXTERNAL_BUILD_LIBRARIES}/serialdv/src/serialdv" "${EXTERNAL_BUILD_LIBRARIES}/serialdv/src/serialdv/dsp" serialdv)
endif()

ExternalProject_Add(dsdcc
    GIT_REPOSITORY https://github.com/f4exb/dsdcc.git
    GIT_TAG cee82816bc8982ecb218abbd2baeb5bb5186711b
    DEPENDS ${DSDCC_DEPENDS}
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/dsdcc"
    CMAKE_ARGS "-DUSE_MBELIB=ON -DLIBMBE_INCLUDE_DIR=${LIBMBE_INCLUDE_DIR} -DLIBMBE_LIBRARY=${LIBMBE_LIBRARIES} -DLIBSERIALDV_INCLUDE_DIR=${LIBSERIALDV_INCLUDE_DIR} -DLIBSERIALDV_LIBRARY=${LIBSERIALDV_LIBRARY}"
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
set(LIBDSDCC_FOUND ON CACHE INTERNAL "")
set(LIBDSDCC_INCLUDE_DIR "${EXTERNAL_BUILD_LIBRARIES}/dsdcc/src" CACHE INTERNAL "")
set(LIBDSDCC_LIBRARIES "${EXTERNAL_BUILD_LIBRARIES}/dsdcc/src/dsdcc-build/libdsdcc${CMAKE_SHARED_LIBRARY_SUFFIX}" CACHE INTERNAL "")

# already on the respository
# TODO decide what to do
if(OFF)
# needs pkgconfig, libusb, autoconf, automake and autolib
ExternalProject_Add(perseus
    GIT_REPOSITORY https://github.com/f4exb/libperseus-sdr.git
    GIT_TAG afefa23e3140ac79d845acb68cf0beeb86d09028
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/perseus"
    INSTALL_COMMAND ""
    TEST_COMMAND ""
)
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
# apt install libairspyhf-dev (only on ubuntu 18.04)
# needs libusb and pkgconfig
ExternalProject_Add(airspyhf
    GIT_REPOSITORY https://github.com/airspy/airspyhf.git
    GIT_TAG 1.1.5
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/airspyhf"
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
# apt install libbladerf-dev
# needs pkgconfig and libusb
ExternalProject_Add(bladerf
    GIT_REPOSITORY https://github.com/Nuand/bladeRF.git
    GIT_TAG 1da130cb5ac9e9f722737e9aee141ecccebef00d
    GIT_SUBMODULES
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/bladerf"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
)
# apt install liblimesuite-dev (only on ubuntu 18.04)
# needs pkgconfig, libusb and sqlite3
ExternalProject_Add(limesuite
    GIT_REPOSITORY https://github.com/myriadrf/LimeSuite.git
    GIT_TAG v19.01.0
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/limesuite"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES} -DLIME_SUITE_EXTVER=release -DENABLE_GUI=OFF -DENABLE_NOVENARF7=OFF -DENABLE_SOAPY_LMS7=OFF -DENABLE_OCTAVE=OFF
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
