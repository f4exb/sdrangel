project(localinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(localinput_SOURCES
    localinputgui.cpp
    localinput.cpp
    localinputsettings.cpp
    localinputplugin.cpp
)

set(localinput_HEADERS
    localinputgui.h
    localinput.h
    localinputsettings.h
    localinputplugin.h
)

set(localinput_FORMS
    localinputgui.ui
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(localinput_FORMS_HEADERS ${localinput_FORMS})

add_library(inputlocal SHARED
    ${localinput_SOURCES}
    ${localinput_HEADERS_MOC}
    ${localinput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_include_directories(inputlocal PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBCM256CCSRC}/..
)
else (BUILD_DEBIAN)
target_include_directories(inputlocal PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(inputlocal
    ${QT_LIBRARIES}
    cm256cc
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputlocal
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(inputlocal Qt5::Core Qt5::Widgets)

install(TARGETS inputlocal DESTINATION lib/plugins/samplesource)
