project(filesink)

set(filesink_SOURCES
	filesinkoutput.cpp
	filesinkplugin.cpp
	filesinksettings.cpp
	filesinkthread.cpp
)

set(filesink_HEADERS
	filesinkoutput.h
	filesinkplugin.h
	filesinksettings.h
	filesinkthread.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(filesink_SOURCES
        ${filesink_SOURCES}
        filesinkgui.cpp

        filesinkgui.ui
    )
    set(filesink_HEADERS
        ${filesink_HEADERS}
        filesinkgui.h
    )

    set(TARGET_NAME outputfilesink)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputfilesinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${filesink_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
