project(airspyhfi)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(airspyhfi_SOURCES
    airspyhfigui.cpp
    airspyhfiinput.cpp
    airspyhfiplugin.cpp
    airspyhfisettings.cpp
    airspyhfithread.cpp
)

set(airspyhfi_HEADERS
    airspyhfigui.h
    airspyhfiinput.h
    airspyhfiplugin.h
    airspyhfisettings.h
    airspyhfithread.h
)

set(airspyhfi_FORMS
    airspyhfigui.ui
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client        
    ${LIBAIRSPYHFSRC}
    ${LIBAIRSPYHFSRC}/libairspyhf/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client        
    ${LIBAIRSPYHF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
#add_definitions(${QT_DEFINITIONS})
add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(airspyhf_HEADERS_MOC ${airspyhf_HEADERS})
qt5_wrap_ui(airspyhfi_FORMS_HEADERS ${airspyhfi_FORMS})

add_library(inputairspyhfi SHARED
    ${airspyhfi_SOURCES}
    ${airspyhfi_HEADERS_MOC}
    ${airspyhfi_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputairspyhfi
    ${QT_LIBRARIES}
    airspyhf
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputairspyhfi
    ${QT_LIBRARIES}
    ${LIBAIRSPYHF_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)


qt5_use_modules(inputairspyhfi Core Widgets)

install(TARGETS inputairspyhfi DESTINATION lib/plugins/samplesource)
