# TODO:
# we need a way to build at compile time and the find_package
#   - use git submodules and add_subdirectory with "EXCLUDE_FROM_ALL" but probably useless
#   - try https://stackoverflow.com/questions/17446981/cmake-externalproject-add-and-findpackage/23570741#23570741

# add the install path to cmake
# TODO need reload after externalproject build
list(APPEND CMAKE_PREFIX_PATH "${EXTERNAL_INSTALL_LIBRARIES}")

# needs boost
ExternalProject_Add(cm256cc
    GIT_REPOSITORY https://github.com/f4exb/cm256cc.git
    GIT_TAG v1.0.5
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/cm256cc"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
)
ExternalProject_Add(mbelib
    GIT_REPOSITORY https://github.com/szechyjs/mbelib.git
    GIT_TAG "debian/1.3.0"
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/mbelib"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
)
if(WIN32 OR LINUX)
    ExternalProject_Add(serialdv
        GIT_REPOSITORY https://github.com/f4exb/serialDV.git
        GIT_TAG v1.0.6
        PREFIX "${EXTERNAL_BUILD_LIBRARIES}/serialDV"
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
    )
endif()
# could use mbelib
ExternalProject_Add(dsdcc
    GIT_REPOSITORY https://github.com/f4exb/dsdcc.git
    GIT_TAG v1.8.4
    DEPENDS mbelib
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/dsdcc"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
)
# needs pkgconfig, libusb
ExternalProject_Add(perseus
    GIT_REPOSITORY https://github.com/f4exb/libperseus-sdr.git
    GIT_TAG afefa23e3140ac79d845acb68cf0beeb86d09028
    PREFIX "${EXTERNAL_BUILD_LIBRARIES}/perseus"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LIBRARIES}
)

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
