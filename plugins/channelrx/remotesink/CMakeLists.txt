project(remotesink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

if (HAS_SSSE3)
    message(STATUS "RemoteSink: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteSink: use Neon SIMD" )
else()
    message(STATUS "RemoteSink: Unsupported architecture")
    return()
endif()

set(remotesink_SOURCES
	remotesink.cpp
	remotesinkgui.cpp
	remotesinksettings.cpp
	remotesinkthread.cpp
	remotesinkplugin.cpp
)

set(remotesink_HEADERS
	remotesink.h
	remotesinkgui.h
	remotesinksettings.h
	remotesinkthread.h
	remotesinkplugin.h
)

set(remotesink_FORMS
	remotesinkgui.ui
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(remotesink_FORMS_HEADERS ${remotesink_FORMS})

add_library(remotesink SHARED
	${remotesink_SOURCES}
	${remotesink_HEADERS_MOC}
	${remotesink_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_include_directories(remotesink PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBCM256CCSRC}/..
)
else (BUILD_DEBIAN)
target_include_directories(remotesink PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(remotesink
    ${QT_LIBRARIES}
    cm256cc
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(remotesink
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(remotesink Qt5::Core Qt5::Widgets)

install(TARGETS remotesink DESTINATION lib/plugins/channelrx)
