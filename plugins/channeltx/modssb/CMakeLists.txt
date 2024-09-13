project(modssb)

set(modssb_SOURCES
    ssbmod.cpp
    ssbmodbaseband.cpp
    ssbmodsource.cpp
    ssbmodplugin.cpp
    ssbmodsettings.cpp
    ssbmodwebapiadapter.cpp
)

set(modssb_HEADERS
    ssbmod.h
    ssbmodbaseband.h
    ssbmodsource.h
    ssbmodplugin.h
    ssbmodsettings.h
    ssbmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modssb_SOURCES
        ${modssb_SOURCES}
        ssbmodgui.cpp

        ssbmodgui.ui
    )
    set(modssb_HEADERS
        ${modssb_HEADERS}
        ssbmodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}modssb)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modssbsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modssb_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME SSBModPlugin ${modssb_SOURCES})
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
