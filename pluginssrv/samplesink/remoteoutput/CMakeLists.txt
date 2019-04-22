project(remoteoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/remoteoutput")

if (HAS_SSSE3)
    message(STATUS "RemoteOutput: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteOutput: use Neon SIMD" )
else()
    message(STATUS "RemoteOutput: Unsupported architecture")
    return()
endif()

set(remoteoutput_SOURCES
	${PLUGIN_PREFIX}/remoteoutput.cpp
	${PLUGIN_PREFIX}/remoteoutputplugin.cpp
	${PLUGIN_PREFIX}/remoteoutputsettings.cpp
	${PLUGIN_PREFIX}/remoteoutputthread.cpp
    ${PLUGIN_PREFIX}/udpsinkfec.cpp
    ${PLUGIN_PREFIX}/udpsinkfecworker.cpp
)

set(remoteoutput_HEADERS
	${PLUGIN_PREFIX}/remoteoutput.h
	${PLUGIN_PREFIX}/remoteoutputplugin.h
	${PLUGIN_PREFIX}/remoteoutputsettings.h
	${PLUGIN_PREFIX}/remoteoutputthread.h
    ${PLUGIN_PREFIX}/udpsinkfec.h
    ${PLUGIN_PREFIX}/udpsinkfecworker.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client        
    ${LIBCM256CCSRC}/..
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    	
	${CMAKE_SOURCE_DIR}/devices
    ${CM256CC_INCLUDE_DIR}	
)
endif (BUILD_DEBIAN)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(outputremotesrv SHARED
	${remoteoutput_SOURCES}
	${remoteoutput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputremotesrv
    ${QT_LIBRARIES}
    sdrbase
    swagger
    cm256cc
)
else (BUILD_DEBIAN)
target_link_libraries(outputremotesrv
	${QT_LIBRARIES}
	sdrbase
	swagger
    ${CM256CC_LIBRARIES}	
)
endif (BUILD_DEBIAN)

target_link_libraries(outputremotesrv Qt5::Core)

install(TARGETS outputremotesrv DESTINATION lib/pluginssrv/samplesink)
