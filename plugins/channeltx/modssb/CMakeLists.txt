project(modssb)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(modssb_SOURCES
    ssbmod.cpp
    ssbmodgui.cpp
    ssbmodplugin.cpp
    ssbmodsettings.cpp
)

set(modssb_HEADERS
    ssbmod.h
    ssbmodgui.h
    ssbmodplugin.h
    ssbmodsettings.h
)

set(modssb_FORMS
    ssbmodgui.ui
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(modssb_FORMS_HEADERS ${modssb_FORMS})

add_library(modssb SHARED
    ${modssb_SOURCES}
    ${modssb_HEADERS_MOC}
    ${modssb_FORMS_HEADERS}
)

target_link_libraries(modssb
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)

target_link_libraries(modssb Qt5::Core Qt5::Widgets)

install(TARGETS modssb DESTINATION lib/plugins/channeltx)
