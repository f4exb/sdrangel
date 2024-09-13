project(modwfm)

set(modwfm_SOURCES
    wfmmod.cpp
    wfmmodbaseband.cpp
    wfmmodsource.cpp
	wfmmodplugin.cpp
    wfmmodsettings.cpp
    wfmmodwebapiadapter.cpp
)

set(modwfm_HEADERS
	wfmmod.h
    wfmmodbaseband.h
    wfmmodsource.h
	wfmmodplugin.h
    wfmmodsettings.h
    wfmmodwebapiadapter.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modwfm_SOURCES
        ${modwfm_SOURCES}
        wfmmodgui.cpp

        wfmmodgui.ui
    )
    set(modwfm_HEADERS
        ${modwfm_HEADERS}
        wfmmodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}modwfm)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modwfmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modwfm_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME WFMModPlugin ${modwfm_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
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
