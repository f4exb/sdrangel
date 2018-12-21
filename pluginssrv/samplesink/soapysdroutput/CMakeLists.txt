project(soapysdroutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/soapysdroutput")

set(soapysdroutput_SOURCES
	${PLUGIN_PREFIX}/soapysdroutput.cpp
	${PLUGIN_PREFIX}/soapysdroutputplugin.cpp
	${PLUGIN_PREFIX}/soapysdroutputsettings.cpp
	${PLUGIN_PREFIX}/soapysdroutputthread.cpp
)

set(soapysdroutput_HEADERS
	${PLUGIN_PREFIX}/soapysdroutput.h
	${PLUGIN_PREFIX}/soapysdroutputplugin.h
	${PLUGIN_PREFIX}/soapysdroutputsettings.h
	${PLUGIN_PREFIX}/soapysdroutputthread.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices
    ${SOAPYSDR_SOURCE_DIR}/include
    ${SOAPYSDR_SOURCE_DIR}/src
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
	${CMAKE_SOURCE_DIR}/devices
	${SOAPYSDR_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(outputsoapysdrsrv SHARED
	${soapysdroutput_SOURCES}
	${soapysdroutput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputsoapysdrsrv
    ${QT_LIBRARIES}
    soapysdr
    sdrbase
    swagger
    soapysdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(outputsoapysdrsrv
	${QT_LIBRARIES}
	${SOAPYSDR_LIBRARY}
	sdrbase
	swagger
	soapysdrdevice
)
endif (BUILD_DEBIAN)

target_link_libraries(outputsoapysdrsrv Qt5::Core)

install(TARGETS outputsoapysdrsrv DESTINATION lib/plugins/samplesink)
