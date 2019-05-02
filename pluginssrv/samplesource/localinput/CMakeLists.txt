project(localinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/localinput")

set(localinput_SOURCES
    ${PLUGIN_PREFIX}/localinput.cpp
    ${PLUGIN_PREFIX}/localinputsettings.cpp
    ${PLUGIN_PREFIX}/localinputplugin.cpp
)

set(localinput_HEADERS
    ${PLUGIN_PREFIX}/localinput.h
    ${PLUGIN_PREFIX}/localinputsettings.h
    ${PLUGIN_PREFIX}/localinputplugin.h
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputlocalsrv SHARED
    ${localinput_SOURCES}
    ${localinput_HEADERS_MOC}
)

target_include_directories(inputlocalsrv PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

target_link_libraries(inputlocalsrv
    ${QT_LIBRARIES}
    sdrbase
    swagger
)

target_link_libraries(inputlocalsrv Qt5::Core)

install(TARGETS inputlocalsrv DESTINATION lib/pluginssrv/samplesource)
