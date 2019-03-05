project(modfreedv)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(modfreedv_SOURCES
    freedvmod.cpp
    freedvmodgui.cpp
    freedvmodplugin.cpp
    freedvmodsettings.cpp
)

set(modfreedv_HEADERS
    freedvmod.h
    freedvmodgui.h
    freedvmodplugin.h
    freedvmodsettings.h
)

set(modfreedv_FORMS
    freedvmodgui.ui
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/libfreedv
    ${CODEC2_INCLUDE_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(modfreedv_FORMS_HEADERS ${modfreedv_FORMS})

add_library(modfreedv SHARED
    ${modfreedv_SOURCES}
    ${modfreedv_HEADERS_MOC}
    ${modfreedv_FORMS_HEADERS}
)

target_link_libraries(modfreedv
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    swagger
	freedv    
    ${CODEC2_LIBRARIES}
)

target_link_libraries(modfreedv Qt5::Core Qt5::Widgets)

install(TARGETS modfreedv DESTINATION lib/plugins/channeltx)
