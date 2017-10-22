project(hackrfoutput)

set(hackrfoutput_SOURCES
    hackrfoutputgui.cpp
    hackrfoutput.cpp
    hackrfoutputplugin.cpp
    hackrfoutputsettings.cpp
    hackrfoutputthread.cpp
)

set(hackrfoutput_HEADERS
    hackrfoutputgui.h
    hackrfoutput.h
    hackrfoutputplugin.h
    hackrfoutputsettings.h
    hackrfoutputthread.h
)

set(hackrfoutput_FORMS
    hackrfoutputgui.ui
)

if (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBHACKRFSRC}
    ${LIBHACKRFSRC}/libhackrf/src
)
else (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBHACKRF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
#add_definitions(${QT_DEFINITIONS})
add_definitions("${QT_DEFINITIONS} -DLIBHACKRF_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(hackrfoutput_HEADERS_MOC ${hackrfoutput_HEADERS})
qt5_wrap_ui(hackrfoutput_FORMS_HEADERS ${hackrfoutput_FORMS})

add_library(outputhackrf SHARED
    ${hackrfoutput_SOURCES}
    ${hackrfoutput_HEADERS_MOC}
    ${hackrfoutput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(outputhackrf
    ${QT_LIBRARIES}
    hackrf
    sdrbase
    sdrgui
    hackrfdevice
)
else (BUILD_DEBIAN)
target_link_libraries(outputhackrf
    ${QT_LIBRARIES}
    ${LIBHACKRF_LIBRARIES}
    sdrbase
    sdrgui
    hackrfdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(outputhackrf Core Widgets)

install(TARGETS outputhackrf DESTINATION lib/plugins/samplesink)
