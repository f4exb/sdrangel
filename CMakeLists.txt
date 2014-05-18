cmake_minimum_required(VERSION 2.8.9)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

project(sdrangelove)

set(CMAKE_BUILD_TYPE "Release")
#set(CMAKE_BUILD_TYPE "ReleaseWithDebugInfo")
#set(CMAKE_BUILD_TYPE "Debug")

set(QT_USE_QTOPENGL TRUE)
set(CMAKE_AUTOMOC ON)

#find_package(Qt4 REQUIRED)
find_package(Qt5Core 5.0 REQUIRED)
find_package(Qt5Widgets 5.0 REQUIRED)
find_package(Qt5Multimedia 5.0 REQUIRED)
#find_package(QT5OpenGL 5.0 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig)

find_package(FFTW3F)

##############################################################################

#include(${QT_USE_FILE})
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

	sdrbase/dsp/channelizer.cpp
	sdrbase/dsp/channelmarker.cpp
	sdrbase/dsp/dspcommands.cpp
	sdrbase/dsp/dspengine.cpp
	sdrbase/dsp/fftengine.cpp
	sdrbase/dsp/fftwindow.cpp
	sdrbase/dsp/interpolator.cpp
	sdrbase/dsp/inthalfbandfilter.cpp
	sdrbase/dsp/lowpass.cpp
	sdrbase/dsp/movingaverage.cpp
	sdrbase/dsp/nco.cpp
	sdrbase/dsp/pidcontroller.cpp
	sdrbase/dsp/samplefifo.cpp
	sdrbase/dsp/samplesink.cpp
	sdrbase/dsp/scopevis.cpp
	sdrbase/dsp/spectrumvis.cpp
	sdrbase/dsp/threadedsamplesink.cpp

	sdrbase/gui/aboutdialog.cpp
	sdrbase/gui/addpresetdialog.cpp
	sdrbase/gui/basicchannelsettingswidget.cpp
	sdrbase/gui/buttonswitch.cpp
	sdrbase/gui/channelwindow.cpp
	sdrbase/gui/glscope.cpp
	sdrbase/gui/glspectrum.cpp
	sdrbase/gui/glspectrumgui.cpp
	sdrbase/gui/indicator.cpp
	sdrbase/gui/pluginsdialog.cpp
	sdrbase/gui/preferencesdialog.cpp
	sdrbase/gui/presetitem.cpp
	sdrbase/gui/rollupwidget.cpp
	sdrbase/gui/scale.cpp
	sdrbase/gui/scaleengine.cpp
	sdrbase/gui/scopewindow.cpp
	sdrbase/gui/valuedial.cpp

	sdrbase/dsp/samplesource/samplesource.cpp

	sdrbase/plugin/pluginapi.cpp
	sdrbase/plugin/plugingui.cpp
	sdrbase/plugin/plugininterface.cpp
	sdrbase/plugin/pluginmanager.cpp

	sdrbase/settings/preferences.cpp
	sdrbase/settings/preset.cpp
	sdrbase/settings/settings.cpp

	sdrbase/util/message.cpp
	sdrbase/util/messagequeue.cpp
	sdrbase/util/miniz.cpp
	sdrbase/util/simpleserializer.cpp
	sdrbase/util/spinlock.cpp
)

set(sdrbase_HEADERS
	include-gpl/mainwindow.h

	include-gpl/audio/audiodeviceinfo.h
	include-gpl/audio/audiofifo.h
	include-gpl/audio/audiooutput.h

	include-gpl/dsp/channelizer.h
	include/dsp/channelmarker.h
	include-gpl/dsp/dspcommands.h
	include-gpl/dsp/dspengine.h
	include/dsp/dsptypes.h
	include-gpl/dsp/fftengine.h
	include-gpl/dsp/fftwengine.h
	include-gpl/dsp/fftwindow.h
	include-gpl/dsp/interpolator.h
	include-gpl/dsp/inthalfbandfilter.h
	include/dsp/kissfft.h
	include-gpl/dsp/kissengine.h
	include-gpl/dsp/lowpass.h
	include-gpl/dsp/movingaverage.h
	include-gpl/dsp/nco.h
	sdrbase/dsp/pidcontroller.h
	include/dsp/samplefifo.h
	include/dsp/samplesink.h
	include-gpl/dsp/scopevis.h
	include-gpl/dsp/spectrumvis.h
	include/dsp/threadedsamplesink.h

	include-gpl/gui/aboutdialog.h
	include-gpl/gui/addpresetdialog.h
	include/gui/basicchannelsettingswidget.h
	include-gpl/gui/buttonswitch.h
	include-gpl/gui/channelwindow.h
	include-gpl/gui/glscope.h
	include-gpl/gui/glspectrum.h
	include-gpl/gui/glspectrumgui.h
	include-gpl/gui/indicator.h
	include-gpl/gui/physicalunit.h
	include-gpl/gui/pluginsdialog.h
	include-gpl/gui/preferencesdialog.h
	include-gpl/gui/presetitem.h
	include/gui/rollupwidget.h
	include-gpl/gui/scale.h
	include-gpl/gui/scaleengine.h
	include-gpl/gui/scopewindow.h
	include-gpl/gui/valuedial.h

	include/dsp/samplesource/samplesource.h

	include/plugin/pluginapi.h
	include/plugin/plugingui.h
	include/plugin/plugininterface.h
	include-gpl/plugin/pluginmanager.h

	include-gpl/settings/preferences.h
	include-gpl/settings/preset.h
	include-gpl/settings/settings.h

	include/util/export.h
	include/util/message.h
	include/util/messagequeue.h
	include/util/miniz.h
	include/util/simpleserializer.h
	include/util/spinlock.h
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
	sdrbase/gui/glspectrumgui.ui
	sdrbase/gui/pluginsdialog.ui
	sdrbase/gui/preferencesdialog.ui
	sdrbase/gui/scopewindow.ui
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
		include-gpl/dsp/fftwengine.h
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
		include-gpl/dsp/kissengine.h
	)
	add_definitions(-DUSE_KISSFFT)
endif(FFTW3F_FOUND)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

#qt5_wrap_cpp(sdrbase_HEADERS_MOC ${sdrbase_HEADERS})
qt5_wrap_ui(sdrbase_FORMS_HEADERS ${sdrbase_FORMS})
qt5_add_resources(sdrbase_RESOURCES_RCC ${sdrbase_RESOURCES})

if(WIN32)
	SET(sdrbase_SOURCES ${sdrbase_SOURCES} sdrbase/resources/sdrangelove.rc)
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

set_target_properties(sdrbase PROPERTIES DEFINE_SYMBOL "sdrangelove_EXPORTS")

qt5_use_modules(sdrbase Core Widgets OpenGL Multimedia)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
	${OPENGL_INCLUDE_DIR}
)

if(MSVC)
	set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSE2" )
	set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSE2" )
	set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
	add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
else()
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse2" )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -msse2" )
endif()

##############################################################################

set(sdrangelove_SOURCES
	main.cpp
)

if(WIN32)
	SET(sdrangelove_SOURCES ${sdrangelove_SOURCES} sdrbase/resources/sdrangelove.rc)
endif(WIN32)

add_executable(sdrangelove
	${sdrangelove_SOURCES}
)

target_link_libraries(sdrangelove
	sdrbase
	${QT_LIBRARIES}
	${OPENGL_LIBRARIES}
)

if(WIN32)
	set_target_properties(sdrangelove PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
	set_target_properties(sdrangelove PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
	set_target_properties(sdrangelove PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
	set_target_properties(sdrangelove PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
	set_target_properties(sdrangelove PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
	set_target_properties(sdrangelove PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
endif(WIN32)

qt5_use_modules(sdrangelove Widgets Multimedia)

##############################################################################

add_subdirectory(plugins)
