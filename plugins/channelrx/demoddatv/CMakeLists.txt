project(datv)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(datv_SOURCES
	datvdemod.cpp
	datvdemodgui.cpp
    datvdemodplugin.cpp
    datvdemodsettings.cpp
	datvideostream.cpp
    datvideorender.cpp
    leansdr/dvb.cpp
    leansdr/filtergen.cpp
    leansdr/framework.cpp
    leansdr/math.cpp
    leansdr/sdr.cpp
)

set(datv_HEADERS
	datvdemod.h
	datvdemodgui.h
	datvdemodplugin.h
    datvdemodsettings.h
	datvideostream.h
    datvideorender.h
    leansdr/dvb.h
    leansdr/filtergen.h
    leansdr/framework.h
    leansdr/math.h
    leansdr/sdr.h
)

set(datv_FORMS
	datvdemodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${AVCODEC_INCLUDE_DIRS}
	${AVFORMAT_INCLUDE_DIRS}
	${AVUTIL_INCLUDE_DIRS}
	${SWSCALE_INCLUDE_DIRS}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(nfm_HEADERS_MOC ${nfm_HEADERS})
qt5_wrap_ui(datv_FORMS_HEADERS ${datv_FORMS})

add_library(demoddatv SHARED
	${datv_SOURCES}
	${datv_HEADERS_MOC}
	${datv_FORMS_HEADERS}
)

target_link_libraries(demoddatv
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	${AVCODEC_LIBRARIES}
	${AVFORMAT_LIBRARIES}
	${AVUTIL_LIBRARIES}
	${SWSCALE_LIBRARIES}
)

find_package(Qt5MultimediaWidgets 5.0 REQUIRED)
target_link_libraries(demoddatv Qt5::Core Qt5::Widgets Qt5::Multimedia Qt5::MultimediaWidgets)

install(TARGETS demoddatv DESTINATION lib/plugins/channelrx)
