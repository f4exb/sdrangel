project(airspyhff)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(airspyhff_SOURCES
    airspyhffgui.cpp
    airspyhffinput.cpp
    airspyhffplugin.cpp
    airspyhffsettings.cpp
    airspyhffthread.cpp
)

set(airspyhff_HEADERS
    airspyhffgui.h
    airspyhffinput.h
    airspyhffplugin.h
    airspyhffsettings.h
    airspyhffthread.h
)

set(airspyhff_FORMS
    airspyhffgui.ui
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

#qt4_wrap_cpp(airspyhff_HEADERS_MOC ${airspyhff_HEADERS})
qt5_wrap_ui(airspyhff_FORMS_HEADERS ${airspyhff_FORMS})

add_library(inputairspyhff SHARED
    ${airspyhff_SOURCES}
    ${airspyhff_HEADERS_MOC}
    ${airspyhff_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputairspyhff
    ${QT_LIBRARIES}
    airspyhff
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputairspyhff
    ${QT_LIBRARIES}
    ${LIBAIRSPYHF_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)


qt5_use_modules(inputairspyhff Core Widgets)

install(TARGETS inputairspyhff DESTINATION lib/plugins/samplesource)
