project(bladerf1output)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/bladerf1output")

set(bladerf1output_SOURCES
	${PLUGIN_PREFIX}/bladerf1output.cpp
	${PLUGIN_PREFIX}/bladerf1outputplugin.cpp
	${PLUGIN_PREFIX}/bladerf1outputsettings.cpp
	${PLUGIN_PREFIX}/bladerf1outputthread.cpp
)

set(bladerf1output_HEADERS
	${PLUGIN_PREFIX}/bladerf1output.h
	${PLUGIN_PREFIX}/bladerf1outputplugin.h
	${PLUGIN_PREFIX}/bladerf1outputsettings.h
	${PLUGIN_PREFIX}/bladerf1outputthread.h
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

add_library(outputbladerf1srv SHARED
	${bladerf1output_SOURCES}
	${bladerf1output_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputbladerf1srv
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    swagger
    bladerf1device
)
else (BUILD_DEBIAN)
target_link_libraries(outputbladerf1srv
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	swagger
	bladerf1device
)
endif (BUILD_DEBIAN)

target_link_libraries(outputbladerf1srv Qt5::Core)

install(TARGETS outputbladerf1srv DESTINATION lib/pluginssrv/samplesink)
