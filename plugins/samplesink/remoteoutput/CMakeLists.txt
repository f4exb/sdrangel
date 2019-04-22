project(remoteoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

if (HAS_SSSE3)
    message(STATUS "RemoteFEC: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteFEC: use Neon SIMD" )
else()
    message(STATUS "RemoteFEC: Unsupported architecture")
    return()
endif()

set(remoteoutput_SOURCES
	remoteoutputgui.cpp
	remoteoutput.cpp
	remoteoutputplugin.cpp
	remoteoutputsettings.cpp
	remoteoutputthread.cpp
    udpsinkfec.cpp
    udpsinkfecworker.cpp
)

set(remoteoutput_HEADERS
	remoteoutputgui.h
	remoteoutput.h
	remoteoutputplugin.h
	remoteoutputsettings.h
	remoteoutputthreads.h
    udpsinkfec.h
    udpsinkfecworker.h
)

set(remoteoutput_FORMS
	remoteoutputgui.ui
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

qt5_wrap_ui(remoteoutput_FORMS_HEADERS ${remoteoutput_FORMS})

add_library(outputremote SHARED
	${remoteoutput_SOURCES}
	${remoteoutput_HEADERS_MOC}
	${remoteoutput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(outputremote
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    swagger
    cm256cc
)
else (BUILD_DEBIAN)
target_link_libraries(outputremote
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
    ${CM256CC_LIBRARIES}	
)
endif (BUILD_DEBIAN)

target_link_libraries(outputremote Qt5::Core Qt5::Widgets)

install(TARGETS outputremote DESTINATION lib/plugins/samplesink)
