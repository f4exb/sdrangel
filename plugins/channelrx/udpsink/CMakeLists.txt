project(udpsink)

set(udpsink_SOURCES
    udpsink.cpp
    udpsinksink.cpp
    udpsinkbaseband.cpp
	udpsinkplugin.cpp
    udpsinksettings.cpp
    udpsinkwebapiadapter.cpp
)

set(udpsink_HEADERS
    udpsink.h
    udpsinksink.h
    udpsinkbaseband.h
	udpsinkplugin.h
    udpsinksettings.h
    udpsinkwebapiadapter.h
)

include_directories(
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
    set(TARGET_LIB "Qt::Widgets")
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
        Qt::Core
        ${TARGET_LIB}
        Qt::Network
	sdrbase
	${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
