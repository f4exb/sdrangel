project(sdrplay)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(sdrplay_SOURCES
    sdrplaygui.cpp
    sdrplayinput.cpp
    sdrplayplugin.cpp
    sdrplaysettings.cpp
    sdrplaythread.cpp
)

set(sdrplay_HEADERS
    sdrplaygui.h
    sdrplayinput.h
    sdrplayplugin.h
    sdrplaysettings.h
    sdrplaythread.h
)

set(sdrplay_FORMS
    sdrplaygui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBMIRISDRSRC}/include
    ${LIBMIRISDRSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBMIRISDR_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(sdrplay_FORMS_HEADERS ${sdrplay_FORMS})

add_library(inputsdrplay SHARED
    ${sdrplay_SOURCES}
    ${sdrplay_HEADERS_MOC}
    ${sdrplay_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputsdrplay
    ${QT_LIBRARIES}
    mirisdr
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputsdrplay
    ${QT_LIBRARIES}
    ${LIBMIRISDR_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputsdrplay Core Widgets)

install(TARGETS inputsdrplay DESTINATION lib/plugins/samplesource)
