project(remoteinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/remoteinput")

if (HAS_SSSE3)
    message(STATUS "RemoteInput: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "RemoteInput: use Neon SIMD" )
else()
    message(STATUS "RemoteInput: Unsupported architecture")
    return()
endif()

set(remoteinput_SOURCES
    ${PLUGIN_PREFIX}/remoteinputbuffer.cpp
    ${PLUGIN_PREFIX}/remoteinputudphandler.cpp
    ${PLUGIN_PREFIX}/remoteinput.cpp
    ${PLUGIN_PREFIX}/remoteinputsettings.cpp
    ${PLUGIN_PREFIX}/remoteinputplugin.cpp
)

set(remoteinput_HEADERS
    ${PLUGIN_PREFIX}/remoteinputbuffer.h
    ${PLUGIN_PREFIX}/remoteinputudphandler.h
    ${PLUGIN_PREFIX}/remoteinput.h
    ${PLUGIN_PREFIX}/remoteinputsettings.h
    ${PLUGIN_PREFIX}/remoteinputplugin.h
)


add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputremotesrv SHARED
    ${remoteinput_SOURCES}
    ${remoteinput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_include_directories(inputremotesrv PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBCM256CCSRC}/..
)
else (BUILD_DEBIAN)
target_include_directories(inputremotesrv PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(inputremotesrv
    ${QT_LIBRARIES}
    cm256cc
    sdrbase
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputremotesrv
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    sdrbase
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(inputremotesrv Qt5::Core)

install(TARGETS inputremotesrv DESTINATION lib/pluginssrv/samplesource)
