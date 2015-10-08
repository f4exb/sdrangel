cmake_minimum_required(VERSION 2.8.9)
cmake_policy(SET CMP0043 OLD)

option(V4L-RTL "Use Linux Kernel RTL-SDR Source." OFF)
option(V4L-MSI "Use Linux Kernel MSI2500 Source." OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

project(sdrangel)

#set(CMAKE_BUILD_TYPE "Release")
#set(CMAKE_BUILD_TYPE "ReleaseWithDebugInfo")
set(CMAKE_BUILD_TYPE "Debug")

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

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x86")
 SET(USE_SIMD "SSE2" CACHE STRING "Use SIMD instructions")
ENDIF()

##############################################################################

#include(${QT_USE_FILE})
set( QT_DEFINITIONS "${QT_DEFINITIONS} -DQT_NO_DEBUG_OUTPUT" )
#set( QT_DEFINITIONS "${QT_DEFINITIONS}" )
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
	sdrbase/dsp/fftengine.cpp
	sdrbase/dsp/fftfilt.cxx
	sdrbase/dsp/fftwindow.cpp
	sdrbase/dsp/filesink.cpp
	sdrbase/dsp/interpolator.cpp
	sdrbase/dsp/inthalfbandfilter.cpp
	sdrbase/dsp/lowpass.cpp
	sdrbase/dsp/movingaverage.cpp
	sdrbase/dsp/nco.cpp
	sdrbase/dsp/pidcontroller.cpp
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
	sdrbase/gui/glspectrum.cpp
	sdrbase/gui/glspectrumgui.cpp
	sdrbase/gui/indicator.cpp
	sdrbase/gui/pluginsdialog.cpp
	sdrbase/gui/preferencesdialog.cpp
	sdrbase/gui/presetitem.cpp
	sdrbase/gui/rollupwidget.cpp
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

	sdrbase/util/db.cpp
	sdrbase/util/message.cpp
	sdrbase/util/messagequeue.cpp
	sdrbase/util/prettyprint.cpp
	sdrbase/util/syncmessenger.cpp
	sdrbase/util/samplesourceserializer.cpp
	sdrbase/util/simpleserializer.cpp
	#sdrbase/util/spinlock.cpp
)

set(sdrbase_HEADERS
	include/mainwindow.h

	include/audio/audiodeviceinfo.h
	include/audio/audiofifo.h
	include/audio/audiooutput.h

	include/dsp/afsquelch.h
	include/dsp/channelizer.h
	include/dsp/channelmarker.h
	include/dsp/complex.h
	include/dsp/decimators.h
	include/dsp/dspcommands.h
	include/dsp/dspengine.h
	include/dsp/dsptypes.h
	include/dsp/fftengine.h
	include/dsp/fftfilt.h
	include/dsp/fftwengine.h
	include/dsp/fftwindow.h
	include/dsp/filesink.h
	include/dsp/gfft.h
	include/dsp/interpolator.h
	include/dsp/inthalfbandfilter.h
	include/dsp/kissfft.h
	include/dsp/kissengine.h
	include/dsp/lowpass.h
	include/dsp/misc.h
	include/dsp/movingaverage.h
	include/dsp/nco.h
	sdrbase/dsp/pidcontroller.h
	include/dsp/samplefifo.h
	include/dsp/samplesink.h
	include/dsp/nullsink.h
	include/dsp/scopevis.h
	include/dsp/spectrumvis.h
	include/dsp/threadedsamplesink.h

	include/gui/aboutdialog.h
	include/gui/addpresetdialog.h
	include/gui/basicchannelsettingswidget.h
	include/gui/buttonswitch.h
	include/gui/channelwindow.h
	include/gui/colormapper.h
	include/gui/glscope.h
	include/gui/glscopegui.h
	include/gui/glspectrum.h
	include/gui/glspectrumgui.h
	include/gui/indicator.h
	include/gui/physicalunit.h
	include/gui/pluginsdialog.h
	include/gui/preferencesdialog.h
	include/gui/presetitem.h
	include/gui/rollupwidget.h
	include/gui/scale.h
	include/gui/scaleengine.h
	include/gui/valuedial.h

	include/dsp/samplesource.h

	include/plugin/pluginapi.h
	include/plugin/plugingui.h
	include/plugin/plugininterface.h
	include/plugin/pluginmanager.h

	include/settings/preferences.h
	include/settings/preset.h
	include/settings/mainsettings.h

	include/util/db.h
	include/util/export.h
	include/util/message.h
	include/util/messagequeue.h
	include/util/prettyprint.h
	include/util/syncmessenger.h
	include/util/samplesourceserializer.h
	include/util/simpleserializer.h
	#include/util/spinlock.h
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
	sdrbase/gui/preferencesdialog.ui
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
		include/dsp/fftwengine.h
	)
	add_definitions(-DUSE_FFTW)
	include_directories(${FFTW3F_INCLUDE_DIRS})
else(FFTW3F_FOUND)
	set(sdrbase_SOURCES
		${sdrbase_SOURCES}
		sdrbase/dsp/kissengine.cpp
		include/dsp/kissfft.h
	)
	set(sdrbase_HEADERS
		${sdrbase_HEADERS}
		include/dsp/kissengine.h
	)
	add_definitions(-DUSE_KISSFFT)
endif(FFTW3F_FOUND)

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
	${LIBUSB_LIBRARIES}
)

if(FFTW3F_FOUND)
	target_link_libraries(sdrbase ${FFTW3F_LIBRARIES})
endif(FFTW3F_FOUND)

set_target_properties(sdrbase PROPERTIES DEFINE_SYMBOL "sdrangel_EXPORTS")

qt5_use_modules(sdrbase Core Widgets OpenGL Multimedia)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include
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
	main.cpp
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
add_subdirectory(fcdhid)
add_subdirectory(fcdlib)
