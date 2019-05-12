project(perseus)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(perseus_SOURCES
    perseusgui.cpp
    perseusinput.cpp
    perseusplugin.cpp
    perseussettings.cpp
    perseusthread.cpp
)

set(perseus_HEADERS
    perseusgui.h
    perseusinput.h
    perseusplugin.h
    perseussettings.h
    perseusthread.h
)

set(perseus_FORMS
    perseusgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBUSB_INCLUDE_DIR}
    ${LIBPERSEUSSRC}
    ${LIBPERSEUSSRC}/libperseus/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBUSB_INCLUDE_DIR}
    ${LIBPERSEUS_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
#add_definitions(${QT_DEFINITIONS})
add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(perseus_HEADERS_MOC ${perseus_HEADERS})
qt5_wrap_ui(perseus_FORMS_HEADERS ${perseus_FORMS})

add_library(inputperseus SHARED
    ${perseus_SOURCES}
    ${perseus_HEADERS_MOC}
    ${perseus_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputperseus
    ${QT_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    perseus
    sdrbase
    sdrgui
    swagger
    perseusdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputperseus
    ${QT_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    ${LIBPERSEUS_LIBRARIES}
    sdrbase
    sdrgui
    swagger
    perseusdevice
)
endif (BUILD_DEBIAN)


qt5_use_modules(inputperseus Core Widgets)

install(TARGETS inputperseus DESTINATION lib/plugins/samplesource)
