project(modnfm)

set(modnfm_SOURCES
    nfmmod.cpp
    nfmmodbaseband.cpp
    nfmmoddcs.cpp
    nfmmodsource.cpp
	nfmmodplugin.cpp
    nfmmodsettings.cpp
    nfmmodwebapiadapter.cpp
)

set(modnfm_HEADERS
	nfmmod.h
    nfmmodbaseband.h
    nfmmoddcs.h
    nfmmodsource.h
	nfmmodplugin.h
    nfmmodsettings.h
    nfmmodwebapiadapter.h
)

include_directories(
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
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modnfmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${modnfm_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
