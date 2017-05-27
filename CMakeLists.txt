cmake_minimum_required(VERSION 3.1.0)
cmake_policy(SET CMP0043 OLD)

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

find_package(Boost)
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

set(sdrbase_SOURCES
    sdrbase/mainwindow.cpp

    sdrbase/audio/audiodeviceinfo.cpp
    sdrbase/audio/audiofifo.cpp
    sdrbase/audio/audiooutput.cpp
    sdrbase/audio/audioinput.cpp

    sdrbase/dsp/afsquelch.cpp
    sdrbase/dsp/agc.cpp
    sdrbase/dsp/downchannelizer.cpp
    sdrbase/dsp/upchannelizer.cpp
    sdrbase/dsp/channelmarker.cpp
    sdrbase/dsp/ctcssdetector.cpp
    sdrbase/dsp/cwkeyer.cpp
    sdrbase/dsp/dspcommands.cpp
    sdrbase/dsp/dspengine.cpp
    sdrbase/dsp/dspdevicesourceengine.cpp
    sdrbase/dsp/dspdevicesinkengine.cpp
    sdrbase/dsp/fftengine.cpp
    sdrbase/dsp/fftfilt.cxx
    sdrbase/dsp/fftwindow.cpp
    sdrbase/dsp/filterrc.cpp
    sdrbase/dsp/filtermbe.cpp
    sdrbase/dsp/filerecord.cpp
    sdrbase/dsp/interpolator.cpp
    sdrbase/dsp/hbfiltertraits.cpp
    sdrbase/dsp/lowpass.cpp
    sdrbase/dsp/nco.cpp
    sdrbase/dsp/ncof.cpp
    sdrbase/dsp/pidcontroller.cpp
    sdrbase/dsp/phaselock.cpp
    sdrbase/dsp/samplesinkfifo.cpp
    sdrbase/dsp/samplesourcefifo.cpp
    sdrbase/dsp/samplesinkfifodoublebuffered.cpp
    sdrbase/dsp/basebandsamplesink.cpp
    sdrbase/dsp/basebandsamplesource.cpp
    sdrbase/dsp/nullsink.cpp
    sdrbase/dsp/recursivefilters.cpp
    sdrbase/dsp/spectrumscopecombovis.cpp
    sdrbase/dsp/spectrumscopengcombovis.cpp
    sdrbase/dsp/scopevis.cpp
    sdrbase/dsp/scopevisng.cpp
    sdrbase/dsp/scopevismulti.cpp
    sdrbase/dsp/spectrumvis.cpp
    sdrbase/dsp/threadedbasebandsamplesink.cpp
    sdrbase/dsp/threadedbasebandsamplesource.cpp

    sdrbase/gui/aboutdialog.cpp
    sdrbase/gui/addpresetdialog.cpp
    sdrbase/gui/basicchannelsettingswidget.cpp
    sdrbase/gui/buttonswitch.cpp
    sdrbase/gui/channelwindow.cpp
    sdrbase/gui/clickablelabel.cpp
    sdrbase/gui/colormapper.cpp
    sdrbase/gui/cwkeyergui.cpp
    sdrbase/gui/glscope.cpp
    sdrbase/gui/glscopegui.cpp
    sdrbase/gui/glscopeng.cpp
    sdrbase/gui/glscopemulti.cpp
    sdrbase/gui/glscopenggui.cpp
    sdrbase/gui/glscopemultigui.cpp
    sdrbase/gui/glshadersimple.cpp
    sdrbase/gui/glshadertextured.cpp
    sdrbase/gui/glspectrum.cpp
    sdrbase/gui/glspectrumgui.cpp
    sdrbase/gui/indicator.cpp
    sdrbase/gui/levelmeter.cpp
    sdrbase/gui/mypositiondialog.cpp
    sdrbase/gui/pluginsdialog.cpp
    sdrbase/gui/audiodialog.cpp
    sdrbase/gui/presetitem.cpp
    sdrbase/gui/rollupwidget.cpp
    sdrbase/gui/samplingdevicecontrol.cpp
    sdrbase/gui/scale.cpp
    sdrbase/gui/scaleengine.cpp
    sdrbase/gui/valuedial.cpp
    sdrbase/gui/valuedialz.cpp

    sdrbase/dsp/devicesamplesource.cpp
    sdrbase/dsp/devicesamplesink.cpp

    sdrbase/plugin/pluginapi.cpp
    #sdrbase/plugin/plugingui.cpp
    sdrbase/plugin/plugininterface.cpp
    sdrbase/plugin/pluginmanager.cpp

    sdrbase/settings/preferences.cpp
    sdrbase/settings/preset.cpp
    sdrbase/settings/mainsettings.cpp

    sdrbase/util/CRC64.cpp
    sdrbase/util/db.cpp
    sdrbase/util/message.cpp
    sdrbase/util/messagequeue.cpp
    sdrbase/util/prettyprint.cpp
    sdrbase/util/syncmessenger.cpp
    sdrbase/util/samplesourceserializer.cpp
    sdrbase/util/simpleserializer.cpp
    #sdrbase/util/spinlock.cpp
    
    sdrbase/device/devicesourceapi.cpp
    sdrbase/device/devicesinkapi.cpp
)

set(sdrbase_HEADERS
    sdrbase/mainwindow.h

    sdrbase/audio/audiodeviceinfo.h
    sdrbase/audio/audiofifo.h
    sdrbase/audio/audiooutput.h
    sdrbase/audio/audioinput.h

    sdrbase/dsp/afsquelch.h
    sdrbase/dsp/downchannelizer.h
    sdrbase/dsp/upchannelizer.h
    sdrbase/dsp/channelmarker.h
    sdrbase/dsp/complex.h
    sdrbase/dsp/cwkeyer.h
    sdrbase/dsp/decimators.h
    sdrbase/dsp/interpolators.h
    sdrbase/dsp/dspcommands.h
    sdrbase/dsp/dspengine.h
    sdrbase/dsp/dspdevicesourceengine.h
    sdrbase/dsp/dspdevicesinkengine.h
    sdrbase/dsp/dsptypes.h
    sdrbase/dsp/fftengine.h
    sdrbase/dsp/fftfilt.h
    sdrbase/dsp/fftwengine.h
    sdrbase/dsp/fftwindow.h
    sdrbase/dsp/filterrc.h
    sdrbase/dsp/filtermbe.h
    sdrbase/dsp/filerecord.h
    sdrbase/dsp/gfft.h
    sdrbase/dsp/interpolator.h
    sdrbase/dsp/hbfiltertraits.h
    sdrbase/dsp/inthalfbandfilter.h
    sdrbase/dsp/inthalfbandfilterdb.h
    sdrbase/dsp/inthalfbandfiltereo1.h
    sdrbase/dsp/inthalfbandfiltereo1i.h
    sdrbase/dsp/inthalfbandfilterst.h
    sdrbase/dsp/inthalfbandfiltersti.h
    sdrbase/dsp/kissfft.h
    sdrbase/dsp/kissengine.h
    sdrbase/dsp/lowpass.h
    sdrbase/dsp/misc.h
    sdrbase/dsp/movingaverage.h
    sdrbase/dsp/nco.h
    sdrbase/dsp/ncof.h
    sdrbase/dsp/phasediscri.h
    sdrbase/dsp/phaselock.h
    sdrbase/dsp/pidcontroller.h
    sdrbase/dsp/recursivefilters.h
    sdrbase/dsp/samplesinkfifo.h
    sdrbase/dsp/samplesourcefifo.h
    sdrbase/dsp/samplesinkfifodoublebuffered.h
    sdrbase/dsp/samplesinkfifodecimator.h
    sdrbase/dsp/basebandsamplesink.h
    sdrbase/dsp/basebandsamplesource.h
    sdrbase/dsp/nullsink.h
    sdrbase/dsp/spectrumscopecombovis.h
    sdrbase/dsp/spectrumscopengcombovis.h
    sdrbase/dsp/scopevis.h
    sdrbase/dsp/scopevisng.h
    sdrbase/dsp/scopevismulti.h
    sdrbase/dsp/spectrumvis.h
    sdrbase/dsp/threadedbasebandsamplesink.h
    sdrbase/dsp/threadedbasebandsamplesource.h

    sdrbase/gui/aboutdialog.h
    sdrbase/gui/addpresetdialog.h
    sdrbase/gui/basicchannelsettingswidget.h
    sdrbase/gui/buttonswitch.h
    sdrbase/gui/channelwindow.h
    sdrbase/gui/colormapper.h
    sdrbase/gui/cwkeyergui.h
    sdrbase/gui/glscope.h
    sdrbase/gui/glscopegui.h
    sdrbase/gui/glscopeng.h
    sdrbase/gui/glscopemulti.h
    sdrbase/gui/glscopenggui.h
    sdrbase/gui/glscopemultigui.h
    sdrbase/gui/glshadersimple.h
    sdrbase/gui/glshadertextured.h
    sdrbase/gui/glspectrum.h
    sdrbase/gui/glspectrumgui.h
    sdrbase/gui/indicator.h
    sdrbase/gui/levelmeter.h    
    sdrbase/gui/mypositiondialog.h
    sdrbase/gui/physicalunit.h
    sdrbase/gui/pluginsdialog.h
    sdrbase/gui/audiodialog.h
    sdrbase/gui/presetitem.h
    sdrbase/gui/rollupwidget.h
    sdrbase/gui/samplingdevicecontrol.h
    sdrbase/gui/scale.h
    sdrbase/gui/scaleengine.h
    sdrbase/gui/valuedial.h
    sdrbase/gui/valuedialz.h

    sdrbase/dsp/devicesamplesource.h
    sdrbase/dsp/devicesamplesink.h

    sdrbase/plugin/pluginapi.h
    sdrbase/plugin/plugingui.h
    sdrbase/plugin/plugininterface.h
    sdrbase/plugin/pluginmanager.h

    sdrbase/settings/preferences.h
    sdrbase/settings/preset.h
    sdrbase/settings/mainsettings.h

    sdrbase/util/CRC64.h
    sdrbase/util/db.h
    sdrbase/util/doublebuffer.h
    sdrbase/util/export.h
    sdrbase/util/message.h
    sdrbase/util/messagequeue.h
    sdrbase/util/movingaverage.h
    sdrbase/util/prettyprint.h
    sdrbase/util/syncmessenger.h
    sdrbase/util/samplesourceserializer.h
    sdrbase/util/simpleserializer.h
    #sdrbase/util/spinlock.h
    
    sdrbase/device/devicesourceapi.h
    sdrbase/device/devicesinkapi.h
)

set(sdrbase_SOURCES
    ${sdrbase_SOURCES}
    ${sdrbase_HEADERS}
)

set(sdrbase_FORMS
    sdrbase/mainwindow.ui

    sdrbase/gui/aboutdialog.ui
    sdrbase/gui/addpresetdialog.ui
    sdrbase/gui/basicchannelsettingswidget.ui
    sdrbase/gui/cwkeyergui.ui
    sdrbase/gui/glscopegui.ui
    sdrbase/gui/glscopenggui.ui
    sdrbase/gui/glscopemultigui.ui
    sdrbase/gui/glspectrumgui.ui
    sdrbase/gui/pluginsdialog.ui
    sdrbase/gui/audiodialog.ui
    sdrbase/gui/samplingdevicecontrol.ui
    sdrbase/gui/myposdialog.ui
)

set(sdrbase_RESOURCES
    sdrbase/resources/res.qrc
)

if(FFTW3F_FOUND)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        sdrbase/dsp/fftwengine.cpp
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        sdrbase/dsp/fftwengine.h
    )
    add_definitions(-DUSE_FFTW)
    include_directories(${FFTW3F_INCLUDE_DIRS})
else(FFTW3F_FOUND)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        sdrbase/dsp/kissengine.cpp
        sdrbase/dsp/kissfft.h
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        sdrbase/dsp/kissengine.h
    )
    add_definitions(-DUSE_KISSFFT)
endif(FFTW3F_FOUND)

if (LIBSERIALDV_FOUND)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        sdrbase/dsp/dvserialworker.cpp
        sdrbase/dsp/dvserialengine.cpp
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        sdrbase/dsp/dvserialworker.h
        sdrbase/dsp/dvserialengine.h
    )
    add_definitions(-DDSD_USE_SERIALDV)
    include_directories(${LIBSERIALDV_INCLUDE_DIR})
endif(LIBSERIALDV_FOUND)

if (BUILD_DEBIAN)
    set(sdrbase_SOURCES
        ${sdrbase_SOURCES}
        sdrbase/dsp/dvserialworker.cpp
        sdrbase/dsp/dvserialengine.cpp
    )
    set(sdrbase_HEADERS
        ${sdrbase_HEADERS}
        sdrbase/dsp/dvserialworker.h
        sdrbase/dsp/dvserialengine.h
    )
    add_definitions(-DDSD_USE_SERIALDV)
    include_directories(${LIBSERIALDVSRC})
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

#qt5_wrap_cpp(sdrbase_HEADERS_MOC ${sdrbase_HEADERS})
qt5_wrap_ui(sdrbase_FORMS_HEADERS ${sdrbase_FORMS})
qt5_add_resources(sdrbase_RESOURCES_RCC ${sdrbase_RESOURCES})

if(WIN32)
    SET(sdrbase_SOURCES ${sdrbase_SOURCES} sdrbase/resources/sdrangel.rc)
endif(WIN32)

add_library(sdrbase SHARED
    ${sdrbase_SOURCES}
    ${sdrbase_HEADERS_MOC}
    ${sdrbase_FORMS_HEADERS}
    ${sdrbase_RESOURCES_RCC}
)

target_link_libraries(sdrbase
    ${QT_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

if(FFTW3F_FOUND)
    target_link_libraries(sdrbase ${FFTW3F_LIBRARIES})
endif(FFTW3F_FOUND)

if(LIBSERIALDV_FOUND)
    target_link_libraries(sdrbase ${LIBSERIALDV_LIBRARY})
endif(LIBSERIALDV_FOUND)

if (BUILD_DEBIAN)
    target_link_libraries(sdrbase serialdv)
endif (BUILD_DEBIAN)

set_target_properties(sdrbase PROPERTIES DEFINE_SYMBOL "sdrangel_EXPORTS")
target_compile_features(sdrbase PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0

qt5_use_modules(sdrbase Core Widgets OpenGL Multimedia)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${OPENGL_INCLUDE_DIR}
)

##############################################################################

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
endif()

##############################################################################

# Compiler flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fmax-errors=10 -ffast-math -ftree-vectorize ${EXTRA_FLAGS}")

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

if (BUILD_DEBIAN)
    add_subdirectory(cm256cc)
    add_subdirectory(mbelib)
    add_subdirectory(serialdv)
    add_subdirectory(dsdcc)
    add_subdirectory(libairspy)
    add_subdirectory(libhackrf)
    add_subdirectory(librtlsdr)
    add_subdirectory(libbladerf)
    add_subdirectory(libmirisdr)
    add_subdirectory(liblimesuite)
endif (BUILD_DEBIAN)

add_subdirectory(devices)
add_subdirectory(plugins)

if(LIBUSB_FOUND AND UNIX)
    add_subdirectory(fcdhid)
    add_subdirectory(fcdlib)
endif(LIBUSB_FOUND AND UNIX)

##############################################################################

#install targets
install(TARGETS sdrangel DESTINATION bin)
install(TARGETS sdrbase DESTINATION lib)

##############################################################################

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
