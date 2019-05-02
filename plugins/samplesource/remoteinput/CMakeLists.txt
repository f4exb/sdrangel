project(remoteinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

if (HAS_SSSE3)
    message(STATUS "RemoteInput: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteInput: use Neon SIMD" )
else()
    message(STATUS "RemoteInput: Unsupported architecture")
    return()
endif()

set(remoteinput_SOURCES
    remoteinputbuffer.cpp
    remoteinputudphandler.cpp
    remoteinputgui.cpp
    remoteinput.cpp
    remoteinputsettings.cpp
    remoteinputplugin.cpp
)

set(remoteinput_HEADERS
    remoteinputbuffer.h
    remoteinputudphandler.h
    remoteinputgui.h
    remoteinput.h
    remoteinputsettings.h
    remoteinputplugin.h
)

set(remoteinput_FORMS
    remoteinputgui.ui
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(remoteinput_FORMS_HEADERS ${remoteinput_FORMS})

add_library(inputremote SHARED
    ${remoteinput_SOURCES}
    ${remoteinput_HEADERS_MOC}
    ${remoteinput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_include_directories(inputremote PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBCM256CCSRC}/..
)
else (BUILD_DEBIAN)
target_include_directories(inputremote PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(inputremote
    ${QT_LIBRARIES}
    cm256cc
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputremote
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(inputremote Qt5::Core Qt5::Widgets)

install(TARGETS inputremote DESTINATION lib/plugins/samplesource)
