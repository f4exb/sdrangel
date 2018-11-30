project(modatv)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(modatv_SOURCES
	atvmod.cpp
	atvmodgui.cpp
	atvmodplugin.cpp
	atvmodsettings.cpp
)

set(modatv_HEADERS
	atvmod.h
	atvmodgui.h
	atvmodplugin.h
	atvmodsettings.h
)

set(modatv_FORMS
	atvmodgui.ui
)

# OpenCV variables defined in /usr/share/OpenCV/OpenCVConfig.cmake (Ubuntu)

include_directories(
	.
	${OpenCV_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(modatv_FORMS_HEADERS ${modatv_FORMS})

add_library(modatv SHARED
	${modatv_SOURCES}
	${modatv_HEADERS_MOC}
	${modatv_FORMS_HEADERS}
)

target_link_libraries(modatv
    ${OpenCV_LIBS}
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
)

target_link_libraries(modatv Qt5::Core Qt5::Widgets)

install(TARGETS modatv DESTINATION lib/plugins/channeltx)
