project(sdrplay)

set(sdrplay_SOURCES
    sdrplayinput.cpp
    sdrplayplugin.cpp
    sdrplaysettings.cpp
    sdrplaythread.cpp
)

set(sdrplay_HEADERS
    sdrplayinput.h
    sdrplayplugin.h
    sdrplaysettings.h
    sdrplaythread.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBMIRISDR_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(sdrplay_SOURCES
        ${sdrplay_SOURCES}
        sdrplaygui.cpp

        sdrplaygui.ui
    )
    set(sdrplay_HEADERS
        ${sdrplay_HEADERS}
        sdrplaygui.h
    )

    set(TARGET_NAME inputsdrplay)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER "plugins")
else()
    set(TARGET_NAME inputsdrplaysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER "pluginssrv")
endif()

add_library(${TARGET_NAME} SHARED
	${sdrplay_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBMIRISDR_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_LIB_DIR}/${INSTALL_FOLDER}/samplesource)
