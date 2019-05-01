project(filesource)

set(filesource_SOURCES
	filesourceinput.cpp
	filesourceplugin.cpp
	filesourcethread.cpp
	filesourcesettings.cpp
)

set(filesource_HEADERS
	filesourceinput.h
	filesourceplugin.h
	filesourcethread.h
	filesourcesettings.h
)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(filesource_SOURCES
        ${filesource_SOURCES}
        filesourcegui.cpp

        filesourcegui.ui
    )
    set(filesource_HEADERS
        ${filesource_HEADERS}
        filesourcegui.h
    )

    set(TARGET_NAME inputfilesource)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER "plugins")
else()
    set(TARGET_NAME inputfilesourcesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER "pluginssrv")
endif()

add_library(${TARGET_NAME} SHARED
	${filesource_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_LIB_DIR}/${INSTALL_FOLDER}/samplesource)
