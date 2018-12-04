project(soapysdrinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/soapysdrinput")

set(soapysdrinput_SOURCES
	${PLUGIN_PREFIX}/soapysdrinput.cpp
	${PLUGIN_PREFIX}/soapysdrinputplugin.cpp
	${PLUGIN_PREFIX}/soapysdrinputsettings.cpp
	${PLUGIN_PREFIX}/soapysdrinputthread.cpp
)

set(soapysdrinput_HEADERS
	${PLUGIN_PREFIX}/soapysdrinput.h
	${PLUGIN_PREFIX}/soapysdrinputplugin.h
	${PLUGIN_PREFIX}/soapysdrinputsettings.h
	${PLUGIN_PREFIX}/soapysdrinputthread.h
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

add_library(inputsoapysdrsrv SHARED
	${soapysdrinput_SOURCES}
	${soapysdrinput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputsoapysdrsrv
    ${QT_LIBRARIES}
    soapysdr
    sdrbase
    swagger
    soapysdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputsoapysdrsrv
	${QT_LIBRARIES}
	${SOAPYSDR_LIBRARY}
	sdrbase
	swagger
	soapysdrdevice
)
endif (BUILD_DEBIAN)

target_link_libraries(inputsoapysdrsrv Qt5::Core)

install(TARGETS inputsoapysdrsrv DESTINATION lib/pluginssrv/samplesource)
