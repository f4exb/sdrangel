project(daemonsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(daemonsource_SOURCES
	daemonsource.cpp
	daemonsourcethread.cpp
	daemonsourcegui.cpp
	daemonsourceplugin.cpp
	daemonsourcesettings.cpp
)

set(daemonsource_HEADERS
	daemonsource.h
	daemonsourcethread.h
	daemonsourcegui.h
	daemonsourceplugin.h
	daemonsourcesettings.h
)

set(daemonsource_FORMS
	daemonsourcegui.ui
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(daemonsource_FORMS_HEADERS ${daemonsource_FORMS})

add_library(daemonsource SHARED
	${daemonsource_SOURCES}
	${daemonsource_HEADERS_MOC}
	${daemonsource_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_include_directories(daemonsource PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBCM256CCSRC}
)
else (BUILD_DEBIAN)
target_include_directories(daemonsource PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(daemonsource
    ${QT_LIBRARIES}
    cm256cc
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(daemonsource
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(daemonsource Qt5::Core Qt5::Widgets Qt5::Network)

install(TARGETS daemonsource DESTINATION lib/plugins/channeltx)
