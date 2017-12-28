project(limesdrinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/limesdrinput")

set(limesdrinput_SOURCES
	${PLUGIN_PREFIX}/limesdrinput.cpp
	${PLUGIN_PREFIX}/limesdrinputplugin.cpp
	${PLUGIN_PREFIX}/limesdrinputsettings.cpp
	${PLUGIN_PREFIX}/limesdrinputthread.cpp
)

set(limesdrinput_HEADERS
	${PLUGIN_PREFIX}/limesdrinput.h
	${PLUGIN_PREFIX}/limesdrinputplugin.h
	${PLUGIN_PREFIX}/limesdrinputsettings.h
	${PLUGIN_PREFIX}/limesdrinputthread.h
)

if (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBLIMESUITESRC}/src
    ${LIBLIMESUITESRC}/src/ADF4002
    ${LIBLIMESUITESRC}/src/ConnectionRegistry
    ${LIBLIMESUITESRC}/src/FPGA_common
    ${LIBLIMESUITESRC}/src/GFIR
    ${LIBLIMESUITESRC}/src/lms7002m
    ${LIBLIMESUITESRC}/src/lms7002m_mcu
    ${LIBLIMESUITESRC}/src/Si5351C
    ${LIBLIMESUITESRC}/src/protocols
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
	${CMAKE_SOURCE_DIR}/devices
	${LIMESUITE_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputlimesdrsrv SHARED
	${limesdrinput_SOURCES}
	${limesdrinput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputlimesdrsrv
    ${QT_LIBRARIES}
    limesuite
    sdrbase
    swagger
    limesdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputlimesdrsrv
	${QT_LIBRARIES}
	${LIMESUITE_LIBRARY}
	sdrbase
	swagger
	limesdrdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputlimesdrsrv Core)

install(TARGETS inputlimesdrsrv DESTINATION lib/pluginssrv/samplesource)
