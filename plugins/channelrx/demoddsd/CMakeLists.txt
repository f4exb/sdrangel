project(dsddemod)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(dsddemod_SOURCES
    dsddemod.cpp
    dsddemodgui.cpp
    dsddemodplugin.cpp
    dsddemodbaudrates.cpp
    dsddemodsettings.cpp
    dsddecoder.cpp
    dsdstatustextdialog.cpp
)

set(dsddemod_HEADERS
    dsddemod.h
    dsddemodgui.h
    dsddemodplugin.h
    dsddemodbaudrates.h
    dsddemodsettings.h
    dsddecoder.h
    dsdstatustextdialog.h
)

set(dsddemod_FORMS
    dsddemodgui.ui
    dsdstatustextdialog.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
    ${LIBDSDCCSRC}
    ${LIBMBELIBSRC}
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
    ${LIBDSDCC_INCLUDE_DIR}
    ${LIBMBE_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(dsddemod_FORMS_HEADERS ${dsddemod_FORMS})

add_library(demoddsd SHARED
    ${dsddemod_SOURCES}
    ${dsddemod_HEADERS_MOC}
    ${dsddemod_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(demoddsd
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    dsdcc
    mbelib
)
else (BUILD_DEBIAN)
target_link_libraries(demoddsd
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    ${LIBDSDCC_LIBRARIES}
    ${LIBMBE_LIBRARY}
)
endif (BUILD_DEBIAN)


target_link_libraries(demoddsd Qt5::Core Qt5::Widgets)

install(TARGETS demoddsd DESTINATION lib/plugins/channelrx)
