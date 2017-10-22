project(airspy)

set(airspy_SOURCES
    airspygui.cpp
    airspyinput.cpp
    airspyplugin.cpp
    airspysettings.cpp
    airspythread.cpp
)

set(airspy_HEADERS
    airspygui.h
    airspyinput.h
    airspyplugin.h
    airspysettings.h
    airspythread.h
)

set(airspy_FORMS
    airspygui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBAIRSPYSRC}
    ${LIBAIRSPYSRC}/libairspy/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBAIRSPY_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
#add_definitions(${QT_DEFINITIONS})
add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(airspy_HEADERS_MOC ${airspy_HEADERS})
qt5_wrap_ui(airspy_FORMS_HEADERS ${airspy_FORMS})

add_library(inputairspy SHARED
    ${airspy_SOURCES}
    ${airspy_HEADERS_MOC}
    ${airspy_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputairspy
    ${QT_LIBRARIES}
    airspy
    sdrbase
    sdrgui
)
else (BUILD_DEBIAN)
target_link_libraries(inputairspy
    ${QT_LIBRARIES}
    ${LIBAIRSPY_LIBRARIES}
    sdrbase
    sdrgui
)
endif (BUILD_DEBIAN)


qt5_use_modules(inputairspy Core Widgets)

install(TARGETS inputairspy DESTINATION lib/plugins/samplesource)
