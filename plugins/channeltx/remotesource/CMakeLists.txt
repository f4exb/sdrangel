project(remotesource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

if (HAS_SSSE3)
    message(STATUS "RemoteSource: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteSource: use Neon SIMD" )
else()
    message(STATUS "RemoteSource: Unsupported architecture")
    return()
endif()

set(remotesource_SOURCES
	remotesource.cpp
	remotesourcethread.cpp
	remotesourcegui.cpp
	remotesourceplugin.cpp
	remotesourcesettings.cpp
)

set(remotesource_HEADERS
	remotesource.h
	remotesourcethread.h
	remotesourcegui.h
	remotesourceplugin.h
	remotesourcesettings.h
)

set(remotesource_FORMS
	remotesourcegui.ui
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(remotesource_FORMS_HEADERS ${remotesource_FORMS})

add_library(remotesource SHARED
	${remotesource_SOURCES}
	${remotesource_HEADERS_MOC}
	${remotesource_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_include_directories(remotesource PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBCM256CCSRC}/..
)
else (BUILD_DEBIAN)
target_include_directories(remotesource PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(remotesource
    ${QT_LIBRARIES}
    cm256cc
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(remotesource
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(remotesource Qt5::Core Qt5::Widgets Qt5::Network)

install(TARGETS remotesource DESTINATION lib/plugins/channeltx)
