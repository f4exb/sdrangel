project(rtlsdr)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(rtlsdr_SOURCES
    rtlsdrgui.cpp
    rtlsdrinput.cpp
    rtlsdrplugin.cpp
    rtlsdrsettings.cpp
    rtlsdrthread.cpp
)

set(rtlsdr_HEADERS
    rtlsdrgui.h
    rtlsdrinput.h
    rtlsdrplugin.h
    rtlsdrsettings.h
    rtlsdrthread.h
)

set(rtlsdr_FORMS
    rtlsdrgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBRTLSDRSRC}/include
    ${LIBRTLSDRSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBRTLSDR_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(rtlsdr_HEADERS_MOC ${rtlsdr_HEADERS})
qt5_wrap_ui(rtlsdr_FORMS_HEADERS ${rtlsdr_FORMS})

add_library(inputrtlsdr SHARED
    ${rtlsdr_SOURCES}
    ${rtlsdr_HEADERS_MOC}
    ${rtlsdr_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputrtlsdr
    ${QT_LIBRARIES}
    rtlsdr
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputrtlsdr
    ${QT_LIBRARIES}
    ${LIBRTLSDR_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)


qt5_use_modules(inputrtlsdr Core Widgets)

install(TARGETS inputrtlsdr DESTINATION lib/plugins/samplesource)
