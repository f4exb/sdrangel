project(plutosdroutput)

set(plutosdroutput_SOURCES
    plutosdroutputgui.cpp
    plutosdroutput.cpp
    plutosdroutputplugin.cpp
    plutosdroutputsettings.cpp
    plutosdroutputthread.cpp
)

set(plutosdroutput_HEADERS
    plutosdroutputgui.h
    plutosdroutput.h
    plutosdroutputplugin.h
    plutosdroutputsettings.h
    plutosdroutputthread.h
)

set(plutosdroutput_FORMS
    plutosdroutputgui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBIIOSRC}
)
else (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBIIO_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
#add_definitions(${QT_DEFINITIONS})
add_definitions("${QT_DEFINITIONS} -DLIBHACKRF_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(plutosdroutput_HEADERS_MOC ${plutosdroutput_HEADERS})
qt5_wrap_ui(plutosdroutput_FORMS_HEADERS ${plutosdroutput_FORMS})

add_library(inputplutosdr SHARED
    ${plutosdroutput_SOURCES}
    ${plutosdroutput_HEADERS_MOC}
    ${plutosdroutput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputplutosdr
    ${QT_LIBRARIES}
    iio
    sdrbase
    plutosdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputplutosdr
    ${QT_LIBRARIES}
    ${LIBIIO_LIBRARIES}
    sdrbase
    plutosdrdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputplutosdr Core Widgets)

install(TARGETS inputplutosdr DESTINATION lib/plugins/samplesource)
