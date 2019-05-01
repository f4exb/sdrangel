project(udpsink)

set(udpsink_SOURCES
	udpsink.cpp
	udpsinkplugin.cpp
	udpsinksettings.cpp
)

set(udpsink_HEADERS
	udpsink.h
	udpsinkplugin.h
	udpsinksettings.h
)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(udpsink_SOURCES
        ${udpsink_SOURCES}
        udpsinkgui.cpp

	udpsinkgui.ui
    )
    set(udpsink_HEADERS
        ${udpsink_HEADERS}
        udpsinkgui.h
    )

    set(TARGET_NAME udpsink)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME udpsinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${udpsink_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
        Qt5::Network
	sdrbase
	${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
