project(bladerfoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/bladerfoutput")

set(bladerfoutput_SOURCES
	${PLUGIN_PREFIX}/bladerfoutput.cpp
	${PLUGIN_PREFIX}/bladerfoutputplugin.cpp
	${PLUGIN_PREFIX}/bladerfoutputsettings.cpp
	${PLUGIN_PREFIX}/bladerfoutputthread.cpp
)

set(bladerfoutput_HEADERS
	${PLUGIN_PREFIX}/bladerfoutput.h
	${PLUGIN_PREFIX}/bladerfoutputplugin.h
	${PLUGIN_PREFIX}/bladerfoutputsettings.h
	${PLUGIN_PREFIX}/bladerfoutputthread.h
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

add_library(outputbladerfsrv SHARED
	${bladerfoutput_SOURCES}
	${bladerfoutput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputbladerfsrv
    ${QT_LIBRARIES}
    bladerf
    sdrbase
    swagger
    bladerfdevice
)
else (BUILD_DEBIAN)
target_link_libraries(outputbladerfsrv
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	swagger
	bladerfdevice
)
endif (BUILD_DEBIAN)

target_link_libraries(outputbladerfsrv Qt5::Core)

install(TARGETS outputbladerfsrv DESTINATION lib/pluginssrv/samplesink)
