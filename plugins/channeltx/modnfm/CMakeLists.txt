project(modnfm)

set(modnfm_SOURCES
	nfmmod.cpp
	nfmmodplugin.cpp
	nfmmodsettings.cpp
)

set(modnfm_HEADERS
	nfmmod.h
	nfmmodplugin.h
	nfmmodsettings.h
)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modnfm_SOURCES
        ${modnfm_SOURCES}
        nfmmodgui.cpp

        nfmmodgui.ui
    )
    set(modnfm_HEADERS
        ${modnfm_HEADERS}
        nfmmodgui.h
    )

    set(TARGET_NAME modnfm)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER "plugins")
else()
    set(TARGET_NAME modnfmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER "pluginssrv")
endif()

add_library(${TARGET_NAME} SHARED
	${modnfm_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_LIB_DIR}/${INSTALL_FOLDER}/channeltx)
