project(modatv)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/modatv")

set(modatv_SOURCES
	${PLUGIN_PREFIX}/atvmod.cpp
	${PLUGIN_PREFIX}/atvmodplugin.cpp
	${PLUGIN_PREFIX}/atvmodsettings.cpp
)

set(modatv_HEADERS
	${PLUGIN_PREFIX}/atvmod.h
	${PLUGIN_PREFIX}/atvmodplugin.h
	${PLUGIN_PREFIX}/atvmodsettings.h
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

add_library(modatvsrv SHARED
	${modatv_SOURCES}
	${modatv_HEADERS_MOC}
)

target_link_libraries(modatvsrv
    ${OpenCV_LIBS}
	${QT_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(modatvsrv Qt5::Core)

install(TARGETS modatvsrv DESTINATION lib/pluginssrv/channeltx)