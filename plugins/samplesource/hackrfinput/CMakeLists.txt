project(hackrfinput)

set(hackrfinput_SOURCES
    hackrfinputgui.cpp
    hackrfinput.cpp
    hackrfinputplugin.cpp
    hackrfinputsettings.cpp
    hackrfinputthread.cpp
)

set(hackrfinput_HEADERS
    hackrfinputgui.h
    hackrfinput.h
    hackrfinputplugin.h
    hackrfinputsettings.h
    hackrfinputthread.h
)

set(hackrfinput_FORMS
    hackrfinputgui.ui
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

#qt4_wrap_cpp(hackrfinput_HEADERS_MOC ${hackrfinput_HEADERS})
qt5_wrap_ui(hackrfinput_FORMS_HEADERS ${hackrfinput_FORMS})

add_library(inputhackrf SHARED
    ${hackrfinput_SOURCES}
    ${hackrfinput_HEADERS_MOC}
    ${hackrfinput_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputhackrf
    ${QT_LIBRARIES}
    hackrf
    sdrbase
    sdrgui
    hackrfdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputhackrf
    ${QT_LIBRARIES}
    ${LIBHACKRF_LIBRARIES}
    sdrbase
    sdrgui
    hackrfdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputhackrf Core Widgets)

install(TARGETS inputhackrf DESTINATION lib/plugins/samplesource)
