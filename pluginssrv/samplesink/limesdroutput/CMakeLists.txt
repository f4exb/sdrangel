project(limesdroutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/limesdroutput")

set(limesdroutput_SOURCES
	${PLUGIN_PREFIX}/limesdroutput.cpp
	${PLUGIN_PREFIX}/limesdroutputplugin.cpp
	${PLUGIN_PREFIX}/limesdroutputsettings.cpp
	${PLUGIN_PREFIX}/limesdroutputthread.cpp
)

set(limesdroutput_HEADERS
	${PLUGIN_PREFIX}/limesdroutput.h
	${PLUGIN_PREFIX}/limesdroutputplugin.h
	${PLUGIN_PREFIX}/limesdroutputsettings.h
	${PLUGIN_PREFIX}/limesdroutputthread.h
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

add_library(outputlimesdrsrv SHARED
	${limesdroutput_SOURCES}
	${limesdroutput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputlimesdrsrv
    ${QT_LIBRARIES}
    limesuite
    sdrbase
    swagger
    limesdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(outputlimesdrsrv
	${QT_LIBRARIES}
	${LIMESUITE_LIBRARY}
	sdrbase
	swagger
	limesdrdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(outputlimesdrsrv Core)

install(TARGETS outputlimesdrsrv DESTINATION lib/pluginssrv/samplesink)
