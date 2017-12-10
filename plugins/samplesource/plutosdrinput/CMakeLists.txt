project(plutosdrinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(plutosdrinput_SOURCES
    plutosdrinputgui.cpp
    plutosdrinput.cpp
    plutosdrinputplugin.cpp
    plutosdrinputsettings.cpp
    plutosdrinputthread.cpp
)

set(plutosdrinput_HEADERS
    plutosdrinputgui.h
    plutosdrinput.h
    plutosdrinputplugin.h
    plutosdrinputsettings.h
    plutosdrinputthread.h
)

set(plutosdrinput_FORMS
    plutosdrinputgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBIIOSRC}
)
else (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBIIO_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
#add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(plutosdrinput_HEADERS_MOC ${plutosdrinput_HEADERS})
qt5_wrap_ui(plutosdrinput_FORMS_HEADERS ${plutosdrinput_FORMS})

add_library(inputplutosdr SHARED
    ${plutosdrinput_SOURCES}
    ${plutosdrinput_HEADERS_MOC}
    ${plutosdrinput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputplutosdr
    ${QT_LIBRARIES}
    iio
    sdrbase
    sdrgui
    swagger
    plutosdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputplutosdr
    ${QT_LIBRARIES}
    ${LIBIIO_LIBRARIES}
    sdrbase
    sdrgui
    swagger
    plutosdrdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputplutosdr Core Widgets)

install(TARGETS inputplutosdr DESTINATION lib/plugins/samplesource)
