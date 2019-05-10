project(localsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(localsource_SOURCES
	localsource.cpp
	localsourcethread.cpp
	localsourcegui.cpp
	localsourceplugin.cpp
	localsourcesettings.cpp
)

set(localsource_HEADERS
	localsource.h
	localsourcethread.h
	localsourcegui.h
	localsourceplugin.h
	localsourcesettings.h
)

set(localsource_FORMS
	localsourcegui.ui
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(localsource_FORMS_HEADERS ${localsource_FORMS})

add_library(localsource SHARED
	${localsource_SOURCES}
	${localsource_HEADERS_MOC}
	${localsource_FORMS_HEADERS}
)

target_include_directories(localsource PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

target_link_libraries(localsource
    ${QT_LIBRARIES}
    sdrbase
    sdrgui
    swagger
)

target_link_libraries(localsource Qt5::Core Qt5::Widgets Qt5::Network)

install(TARGETS localsource DESTINATION lib/plugins/channeltx)
