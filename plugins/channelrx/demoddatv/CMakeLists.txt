project(datv)

set(datv_SOURCES
	datvdemod.cpp
	datvdemodgui.cpp
	datvdemodplugin.cpp
	datvscreen.cpp
	glshaderarray.cpp
	datvideostream.cpp
	datvideorender.cpp
)

set(datv_HEADERS
	datvdemod.h
	datvdemodgui.h
	datvdemodplugin.h
	datvscreen.h
	glshaderarray.h
	datvideostream.h
	datvideorender.h
)

set(datv_FORMS
	datvdemodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
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
	avcodec
	avformat
	swscale
)

qt5_use_modules(demoddatv Core Widgets Multimedia MultimediaWidgets)

install(TARGETS demoddatv DESTINATION lib/plugins/channelrx)
