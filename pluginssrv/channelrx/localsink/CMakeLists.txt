project(localsink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/localsink")

set(localsink_SOURCES
    ${PLUGIN_PREFIX}/localsink.cpp
	${PLUGIN_PREFIX}/localsinksettings.cpp
	${PLUGIN_PREFIX}/localsinkthread.cpp
	${PLUGIN_PREFIX}/localsinkplugin.cpp
)

set(localsink_HEADERS
    ${PLUGIN_PREFIX}/localsink.h
	${PLUGIN_PREFIX}/localsinksettings.h
	${PLUGIN_PREFIX}/localsinkthread.h
	${PLUGIN_PREFIX}/localsinkplugin.h
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(localsinksrv SHARED
	${localsink_SOURCES}
	${localsink_HEADERS_MOC}
)

target_include_directories(localsinksrv PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

target_link_libraries(localsinksrv
    ${QT_LIBRARIES}
    sdrbase
    swagger
)

target_link_libraries(localsinksrv Qt5::Core)

install(TARGETS localsinksrv DESTINATION lib/pluginssrv/channelrx)
