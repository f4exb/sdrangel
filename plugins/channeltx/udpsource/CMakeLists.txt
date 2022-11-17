project(udpsource)

set(udpsource_SOURCES
    udpsource.cpp
    udpsourcebaseband.cpp
    udpsourcesource.cpp
	udpsourceplugin.cpp
	udpsourceudphandler.cpp
	udpsourcemsg.cpp
    udpsourcesettings.cpp
    udpsourcewebapiadapter.cpp
)

set(udpsource_HEADERS
	udpsource.h
    udpsourcebaseband.h
    udpsourcesource.h
	udpsourceplugin.h
	udpsourceudphandler.h
	udpsourcemsg.h
    udpsourcesettings.h
    udpsourcewebapiadapter.h
)

include_directories(
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(udpsource_SOURCES
        ${udpsource_SOURCES}
        udpsourcegui.cpp

        udpsourcegui.ui
    )
    set(udpsource_HEADERS
        ${udpsource_HEADERS}
        udpsourcegui.h
    )

    set(TARGET_NAME udpsource)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME udpsourcesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${udpsource_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
        Qt::Network
	sdrbase
	${TARGET_LIB_GUI}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
