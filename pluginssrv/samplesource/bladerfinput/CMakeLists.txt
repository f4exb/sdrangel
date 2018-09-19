project(bladerfinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/bladerfinput")

set(bladerfinput_SOURCES
	${PLUGIN_PREFIX}/bladerf1input.cpp
	${PLUGIN_PREFIX}/bladerf1inputplugin.cpp
	${PLUGIN_PREFIX}/bladerf1inputsettings.cpp
	${PLUGIN_PREFIX}/bladerf1inputthread.cpp
)

set(bladerfinput_HEADERS
	${PLUGIN_PREFIX}/bladerf1input.h
	${PLUGIN_PREFIX}/bladerf1inputplugin.h
	${PLUGIN_PREFIX}/bladerf1inputsettings.h
	${PLUGIN_PREFIX}/bladerf1inputthread.h
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
    bladerf1device
)
else (BUILD_DEBIAN)
target_link_libraries(inputbladerfsrv
	${QT_LIBRARIES}
	${LIBBLADERF_LIBRARIES}
	sdrbase
	swagger
	bladerf1device
)
endif (BUILD_DEBIAN)

target_link_libraries(inputbladerfsrv Qt5::Core)

install(TARGETS inputbladerfsrv DESTINATION lib/pluginssrv/samplesource)
