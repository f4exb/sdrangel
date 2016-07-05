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
find_package(LibDSDcc)
find_package(LibMbe)
find_package(SerialDV)

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x86")
 SET(USE_SIMD "SSE2" CACHE STRING "Use SIMD instructions")
ENDIF()

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

	sdrbase/dsp/afsquelch.cpp
	sdrbase/dsp/agc.cpp
	sdrbase/dsp/channelizer.cpp
	sdrbase/dsp/channelmarker.cpp
	sdrbase/dsp/ctcssdetector.cpp
	sdrbase/dsp/dspcommands.cpp
	sdrbase/dsp/dspengine.cpp
	sdrbase/dsp/dspdeviceengine.cpp
	sdrbase/dsp/fftengine.cpp
	sdrbase/dsp/fftfilt.cxx
	sdrbase/dsp/fftwindow.cpp
	sdrbase/dsp/filterrc.cpp
	sdrbase/dsp/filesink.cpp
	sdrbase/dsp/interpolator.cpp
	sdrbase/dsp/inthalfbandfilter.cpp
	sdrbase/dsp/lowpass.cpp
	sdrbase/dsp/movingaverage.cpp
	sdrbase/dsp/nco.cpp
	sdrbase/dsp/pidcontroller.cpp
    sdrbase/dsp/phaselock.cpp
	sdrbase/dsp/samplefifo.cpp
	sdrbase/dsp/samplesink.cpp
	sdrbase/dsp/nullsink.cpp
	sdrbase/dsp/spectrumscopecombovis.cpp
	sdrbase/dsp/scopevis.cpp
	sdrbase/dsp/spectrumvis.cpp
	sdrbase/dsp/threadedsamplesink.cpp

	sdrbase/gui/aboutdialog.cpp
	sdrbase/gui/addpresetdialog.cpp
	sdrbase/gui/basicchannelsettingswidget.cpp
	sdrbase/gui/buttonswitch.cpp
	sdrbase/gui/channelwindow.cpp
	sdrbase/gui/colormapper.cpp
	sdrbase/gui/glscope.cpp
	sdrbase/gui/glscopegui.cpp
	sdrbase/gui/glshadersimple.cpp
	sdrbase/gui/glshadertextured.cpp
    sdrbase/gui/glspectrum.cpp
	sdrbase/gui/glspectrumgui.cpp
	sdrbase/gui/indicator.cpp
	sdrbase/gui/pluginsdialog.cpp
	sdrbase/gui/audiodialog.cpp
	sdrbase/gui/presetitem.cpp
	sdrbase/gui/rollupwidget.cpp
	sdrbase/gui/samplingdevicecontrol.cpp
	sdrbase/gui/scale.cpp
	sdrbase/gui/scaleengine.cpp
	sdrbase/gui/valuedial.cpp

	sdrbase/dsp/samplesource.cpp

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
	
	sdrbase/device/deviceapi.cpp
)

set(sdrbase_HEADERS
	sdrbase/mainwindow.h

	sdrbase/audio/audiodeviceinfo.h
	sdrbase/audio/audiofifo.h
	sdrbase/audio/audiooutput.h

	sdrbase/dsp/afsquelch.h
	sdrbase/dsp/channelizer.h
	sdrbase/dsp/channelmarker.h
	sdrbase/dsp/complex.h
	sdrbase/dsp/decimators.h
	sdrbase/dsp/dspcommands.h
	sdrbase/dsp/dspengine.h
	sdrbase/dsp/dspdeviceengine.h
	sdrbase/dsp/dsptypes.h
	sdrbase/dsp/fftengine.h
	sdrbase/dsp/fftfilt.h
	sdrbase/dsp/fftwengine.h
	sdrbase/dsp/fftwindow.h
	sdrbase/dsp/filterrc.h
	sdrbase/dsp/filesink.h
	sdrbase/dsp/gfft.h
	sdrbase/dsp/interpolator.h
	sdrbase/dsp/inthalfbandfilter.h
	sdrbase/dsp/kissfft.h
	sdrbase/dsp/kissengine.h
	sdrbase/dsp/lowpass.h
	sdrbase/dsp/misc.h
	sdrbase/dsp/movingaverage.h
	sdrbase/dsp/nco.h
	sdrbase/dsp/phasediscri.h
	sdrbase/dsp/phaselock.h
    sdrbase/dsp/pidcontroller.h
	sdrbase/dsp/samplefifo.h
	sdrbase/dsp/samplesink.h
	sdrbase/dsp/nullsink.h
	sdrbase/dsp/scopevis.h
	sdrbase/dsp/spectrumvis.h
	sdrbase/dsp/threadedsamplesink.h

	sdrbase/gui/aboutdialog.h
	sdrbase/gui/addpresetdialog.h
	sdrbase/gui/basicchannelsettingswidget.h
	sdrbase/gui/buttonswitch.h
	sdrbase/gui/channelwindow.h
	sdrbase/gui/colormapper.h
	sdrbase/gui/glscope.h
	sdrbase/gui/glscopegui.h
	sdrbase/gui/glshadersimple.h
	sdrbase/gui/glshadertextured.h
    sdrbase/gui/glspectrum.h
	sdrbase/gui/glspectrumgui.h
	sdrbase/gui/indicator.h
	sdrbase/gui/physicalunit.h
	sdrbase/gui/pluginsdialog.h
	sdrbase/gui/audiodialog.h
	sdrbase/gui/presetitem.h
	sdrbase/gui/rollupwidget.h
	sdrbase/gui/samplingdevicecontrol.h
	sdrbase/gui/scale.h
	sdrbase/gui/scaleengine.h
	sdrbase/gui/valuedial.h

	sdrbase/dsp/samplesource.h

	sdrbase/plugin/pluginapi.h
	sdrbase/plugin/plugingui.h
	sdrbase/plugin/plugininterface.h
	sdrbase/plugin/pluginmanager.h

	sdrbase/settings/preferences.h
	sdrbase/settings/preset.h
	sdrbase/settings/mainsettings.h

	sdrbase/util/CRC64.h
    sdrbase/util/db.h
	sdrbase/util/export.h
	sdrbase/util/message.h
	sdrbase/util/messagequeue.h
	sdrbase/util/movingaverage.h
	sdrbase/util/prettyprint.h
	sdrbase/util/syncmessenger.h
	sdrbase/util/samplesourceserializer.h
	sdrbase/util/simpleserializer.h
	#sdrbase/util/spinlock.h
	
	sdrbase/device/deviceapi.h
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
	sdrbase/gui/glscopegui.ui
	sdrbase/gui/glspectrumgui.ui
	sdrbase/gui/pluginsdialog.ui
	sdrbase/gui/audiodialog.ui
	sdrbase/gui/samplingdevicecontrol.ui
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

set_target_properties(sdrbase PROPERTIES DEFINE_SYMBOL "sdrangel_EXPORTS")
target_compile_features(sdrbase PRIVATE cxx_generalized_initializers) # cmake >= 3.1.0

qt5_use_modules(sdrbase Core Widgets OpenGL Multimedia)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/sdrbase
	${OPENGL_INCLUDE_DIR}
)

if(USE_SIMD MATCHES SSE2)
	if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
		set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse2" )
		set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -msse2" )
		add_definitions(-DUSE_SIMD)
	elseif(MSVC)
		set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSE2" )
		set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSE2" )
		set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
		add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
		add_definitions(-DUSE_SIMD)
	endif()
endif()

##############################################################################

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
