project(sdrdaemonsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/sdrdaemonsource")

if (HAS_SSSE3)
    message(STATUS "SDRdaemonSource: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "SDRdaemonSource: use Neon SIMD" )
else()
    message(STATUS "SDRdaemonSource: Unsupported architecture")
    return()
endif()

set(sdrdaemonsource_SOURCES
    ${PLUGIN_PREFIX}/sdrdaemonsourcebuffer.cpp
    ${PLUGIN_PREFIX}/sdrdaemonsourceinput.cpp
    ${PLUGIN_PREFIX}/sdrdaemonsourcesettings.cpp
    ${PLUGIN_PREFIX}/sdrdaemonsourceplugin.cpp
    ${PLUGIN_PREFIX}/sdrdaemonsourceudphandler.cpp
)

set(sdrdaemonsource_HEADERS
    ${PLUGIN_PREFIX}/sdrdaemonsourcebuffer.h
    ${PLUGIN_PREFIX}/sdrdaemonsourceinput.h
    ${PLUGIN_PREFIX}/sdrdaemonsourcesettings.h
    ${PLUGIN_PREFIX}/sdrdaemonsourceplugin.h
    ${PLUGIN_PREFIX}/sdrdaemonsourceudphandler.h
)


add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputsdrdaemonsourcesrv SHARED
    ${sdrdaemonsource_SOURCES}
    ${sdrdaemonsource_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_include_directories(inputsdrdaemonsourcesrv PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBCM256CCSRC}
    ${LIBNANOMSG_INCLUDE_DIR}
)
else (BUILD_DEBIAN)
target_include_directories(inputsdrdaemonsourcesrv PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
    ${LIBNANOMSG_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(inputsdrdaemonsourcesrv
    ${QT_LIBRARIES}
    cm256cc
    ${LIBNANOMSG_LIBRARIES}
    sdrbase
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputsdrdaemonsourcesrv
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    ${LIBNANOMSG_LIBRARIES}
    sdrbase
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(inputsdrdaemonsourcesrv Qt5::Core)

install(TARGETS inputsdrdaemonsourcesrv DESTINATION lib/pluginssrv/samplesource)
