project(localsink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(localsink_SOURCES
	localsink.cpp
	localsinkgui.cpp
	localsinksettings.cpp
	localsinkthread.cpp
	localsinkplugin.cpp
)

set(localsink_HEADERS
	localsink.h
	localsinkgui.h
	localsinksettings.h
	localsinkthread.h
	localsinkplugin.h
)

set(localsink_FORMS
	localsinkgui.ui
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(localsink_FORMS_HEADERS ${localsink_FORMS})

add_library(localsink SHARED
	${localsink_SOURCES}
	${localsink_HEADERS_MOC}
	${localsink_FORMS_HEADERS}
)

if (BUILD_DEBIAN)
target_include_directories(localsink PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)
else (BUILD_DEBIAN)
target_include_directories(localsink PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)
endif (BUILD_DEBIAN)

if (BUILD_DEBIAN)
target_link_libraries(localsink
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(localsink
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(localsink Qt5::Core Qt5::Widgets)

install(TARGETS localsink DESTINATION lib/plugins/channelrx)
