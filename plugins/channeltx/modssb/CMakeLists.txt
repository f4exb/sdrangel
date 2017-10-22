project(modssb)

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
)

qt5_use_modules(modssb Core Widgets)

install(TARGETS modssb DESTINATION lib/plugins/channeltx)