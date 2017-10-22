project(sdrdaemonsink)

if (HAS_SSSE3)
    message(STATUS "SDRdaemonFEC: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "SDRdaemonFEC: use Neon SIMD" )
else()
    message(STATUS "SDRdaemonFEC: Unsupported architecture")
    return()
endif()

set(sdrdaemonsink_SOURCES
	sdrdaemonsinkgui.cpp
	sdrdaemonsinkoutput.cpp
	sdrdaemonsinkplugin.cpp
	sdrdaemonsinksettings.cpp
	sdrdaemonsinkthread.cpp
    udpsinkfec.cpp
    UDPSocket.cpp
)

set(sdrdaemonsink_HEADERS
	sdrdaemonsinkgui.h
	sdrdaemonsinkoutput.h
	sdrdaemonsinkplugin.h
	sdrdaemonsinksettings.h
	sdrdaemonsinkthread.h
    udpsinkfec.h
    UDPSocket.h
)

set(sdrdaemonsink_FORMS
	sdrdaemonsinkgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBCM256CCSRC}
    ${LIBNANOMSG_INCLUDE_DIR}
)
else (BUILD_DEBIAN)
include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/devices
    ${CM256CC_INCLUDE_DIR}	
    ${LIBNANOMSG_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(sdrdaemonsink_FORMS_HEADERS ${sdrdaemonsink_FORMS})

add_library(outputsdrdaemonsink SHARED
	${sdrdaemonsink_SOURCES}
	${sdrdaemonsink_HEADERS_MOC}
	${sdrdaemonsink_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(outputsdrdaemonsink
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    cm256cc
    ${LIBNANOMSG_LIBRARIES}
)
else (BUILD_DEBIAN)
target_link_libraries(outputsdrdaemonsink
	${QT_LIBRARIES}
	sdrbase
	sdrgui
    ${CM256CC_LIBRARIES}	
    ${LIBNANOMSG_LIBRARIES}    
)
endif (BUILD_DEBIAN)

qt5_use_modules(outputsdrdaemonsink Core Widgets)

install(TARGETS outputsdrdaemonsink DESTINATION lib/plugins/samplesink)
