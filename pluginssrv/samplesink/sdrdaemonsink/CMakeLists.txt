project(sdrdaemonsink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/sdrdaemonsink")

if (HAS_SSSE3)
    message(STATUS "SDRdaemonFEC: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "SDRdaemonFEC: use Neon SIMD" )
else()
    message(STATUS "SDRdaemonFEC: Unsupported architecture")
    return()
endif()

set(sdrdaemonsink_SOURCES
	${PLUGIN_PREFIX}/sdrdaemonsinkoutput.cpp
	${PLUGIN_PREFIX}/sdrdaemonsinkplugin.cpp
	${PLUGIN_PREFIX}/sdrdaemonsinksettings.cpp
	${PLUGIN_PREFIX}/sdrdaemonsinkthread.cpp
    ${PLUGIN_PREFIX}/udpsinkfec.cpp
    ${PLUGIN_PREFIX}/UDPSocket.cpp
)

set(sdrdaemonsink_HEADERS
	${PLUGIN_PREFIX}/sdrdaemonsinkoutput.h
	${PLUGIN_PREFIX}/sdrdaemonsinkplugin.h
	${PLUGIN_PREFIX}/sdrdaemonsinksettings.h
	${PLUGIN_PREFIX}/sdrdaemonsinkthread.h
    ${PLUGIN_PREFIX}/udpsinkfec.h
    ${PLUGIN_PREFIX}/UDPSocket.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client        
    ${LIBCM256CCSRC}
    ${LIBNANOMSG_INCLUDE_DIR}
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    	
	${CMAKE_SOURCE_DIR}/devices
    ${CM256CC_INCLUDE_DIR}	
    ${LIBNANOMSG_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(outputsdrdaemonsinksrv SHARED
	${sdrdaemonsink_SOURCES}
	${sdrdaemonsink_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputsdrdaemonsinksrv
    ${QT_LIBRARIES}
    sdrbase
    swagger
    cm256cc
    ${LIBNANOMSG_LIBRARIES}
)
else (BUILD_DEBIAN)
target_link_libraries(outputsdrdaemonsinksrv
	${QT_LIBRARIES}
	sdrbase
	swagger
    ${CM256CC_LIBRARIES}	
    ${LIBNANOMSG_LIBRARIES}    
)
endif (BUILD_DEBIAN)

target_link_libraries(outputsdrdaemonsinksrv Qt5::Core)

install(TARGETS outputsdrdaemonsinksrv DESTINATION lib/pluginssrv/samplesink)
