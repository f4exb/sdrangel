project(bladerf2output)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/bladerf2output")

set(bladerf2output_SOURCES
	${PLUGIN_PREFIX}/bladerf2output.cpp
	${PLUGIN_PREFIX}/bladerf2outputplugin.cpp
	${PLUGIN_PREFIX}/bladerf2outputsettings.cpp
	${PLUGIN_PREFIX}/bladerf2outputthread.cpp
)

set(bladerf2output_HEADERS
	${PLUGIN_PREFIX}/bladerf2output.h
	${PLUGIN_PREFIX}/bladerf2outputplugin.h
	${PLUGIN_PREFIX}/bladerf2outputsettings.h
	${PLUGIN_PREFIX}/bladerf2outputthread.h
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

add_library(outputbladerf2srv SHARED
	${bladerf2output_SOURCES}
	${bladerf2output_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputbladerf2srv
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    swagger
    bladerf2device
)
else (BUILD_DEBIAN)
target_link_libraries(outputbladerf2srv
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	swagger
	bladerf2device
)
endif (BUILD_DEBIAN)

target_link_libraries(outputbladerf2srv Qt5::Core)

install(TARGETS outputbladerf2srv DESTINATION lib/pluginssrv/samplesink)
