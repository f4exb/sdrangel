project(airspyhf)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(airspyhf_SOURCES
    airspyhfgui.cpp
    airspyhfinput.cpp
    airspyhfplugin.cpp
    airspyhfsettings.cpp
    airspyhfthread.cpp
)

set(airspyhf_HEADERS
    airspyhfgui.h
    airspyhfinput.h
    airspyhfplugin.h
    airspyhfsettings.h
    airspyhfthread.h
)

set(airspyhf_FORMS
    airspyhfgui.ui
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
qt5_wrap_ui(airspyhf_FORMS_HEADERS ${airspyhf_FORMS})

add_library(inputairspyhf SHARED
    ${airspyhf_SOURCES}
    ${airspyhf_HEADERS_MOC}
    ${airspyhf_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_link_libraries(inputairspyhf
    ${QT_LIBRARIES}
    airspyhf
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputairspyhf
    ${QT_LIBRARIES}
    ${LIBAIRSPYHF_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)


qt5_use_modules(inputairspyhf Core Widgets)

install(TARGETS inputairspyhf DESTINATION lib/plugins/samplesource)
