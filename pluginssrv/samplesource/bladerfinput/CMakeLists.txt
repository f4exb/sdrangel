project(bladerfinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/bladerfinput")

set(bladerfinput_SOURCES
	${PLUGIN_PREFIX}/bladerfinput.cpp
	${PLUGIN_PREFIX}/bladerfinputplugin.cpp
	${PLUGIN_PREFIX}/bladerfinputsettings.cpp
	${PLUGIN_PREFIX}/bladerfinputthread.cpp
)

set(bladerfinput_HEADERS
	${PLUGIN_PREFIX}/bladerfinput.h
	${PLUGIN_PREFIX}/bladerfinputplugin.h
	${PLUGIN_PREFIX}/bladerfinputsettings.h
	${PLUGIN_PREFIX}/bladerfinputthread.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBBLADERFLIBSRC}/include
    ${LIBBLADERFLIBSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
	${CMAKE_SOURCE_DIR}/devices
	${LIBBLADERF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputbladerfsrv SHARED
	${bladerfinput_SOURCES}
	${bladerfinput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputbladerfsrv
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    swagger
    bladerfdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputbladerfsrv
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	swagger
	bladerfdevice
)
endif (BUILD_DEBIAN)

target_link_libraries(inputbladerfsrv Qt5::Core)

install(TARGETS inputbladerfsrv DESTINATION lib/pluginssrv/samplesource)
