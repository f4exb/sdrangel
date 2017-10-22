project(sdrdaemonsource)

if (HAS_SSSE3)
    message(STATUS "SDRdaemonSource: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "SDRdaemonSource: use Neon SIMD" )
else()
    message(STATUS "SDRdaemonSource: Unsupported architecture")
    return()
endif()

set(sdrdaemonsource_SOURCES
    sdrdaemonsourcebuffer.cpp
    sdrdaemonsourcegui.cpp
    sdrdaemonsourceinput.cpp
    sdrdaemonsourcesettings.cpp
    sdrdaemonsourceplugin.cpp
    sdrdaemonsourceudphandler.cpp
)

set(sdrdaemonsource_HEADERS
    sdrdaemonsourcebuffer.h
    sdrdaemonsourcegui.h
    sdrdaemonsourceinput.h
    sdrdaemonsourcesettings.h
    sdrdaemonsourceplugin.h
    sdrdaemonsourceudphandler.h
)

set(sdrdaemonsource_FORMS
    sdrdaemonsourcegui.ui
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(sdrdaemonsource_HEADERS_MOC ${sdrdaemonsource_HEADERS})
qt5_wrap_ui(sdrdaemonsource_FORMS_HEADERS ${sdrdaemonsource_FORMS})

add_library(inputsdrdaemonsource SHARED
    ${sdrdaemonsource_SOURCES}
    ${sdrdaemonsource_HEADERS_MOC}
    ${sdrdaemonsource_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_include_directories(inputsdrdaemonsource PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBCM256CCSRC}
    ${LIBNANOMSG_INCLUDE_DIR}
)
else (BUILD_DEBIAN)
target_include_directories(inputsdrdaemonsource PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CM256CC_INCLUDE_DIR}
    ${LIBNANOMSG_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(inputsdrdaemonsource
    ${QT_LIBRARIES}
    cm256cc
    ${LIBNANOMSG_LIBRARIES}
    sdrbase
    sdrgui
)
else (BUILD_DEBIAN)
target_link_libraries(inputsdrdaemonsource
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    ${LIBNANOMSG_LIBRARIES}
    sdrbase
    sdrgui
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputsdrdaemonsource Core Widgets)

install(TARGETS inputsdrdaemonsource DESTINATION lib/plugins/samplesource)
