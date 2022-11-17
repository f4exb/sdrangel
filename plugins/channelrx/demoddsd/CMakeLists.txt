project(dsddemod)

set(dsddemod_SOURCES
    dsddemod.cpp
    dsddemodplugin.cpp
    dsddemodbaudrates.cpp
    dsddemodsettings.cpp
    dsddemodsink.cpp
    dsddemodbaseband.cpp
    dsddemodwebapiadapter.cpp
    dsddecoder.cpp
)

set(dsddemod_HEADERS
    dsddemod.h
    dsddemodplugin.h
    dsddemodbaudrates.h
    dsddemodsettings.h
    dsddemodsink.h
    dsddemodbaseband.h
    dsddemodwebapiadapter.h
    dsddecoder.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBDSDCC_INCLUDE_DIR}
    ${LIBMBE_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(dsddemod_SOURCES
        ${dsddemod_SOURCES}
        dsddemodgui.cpp
        dsdstatustextdialog.cpp
        dsddemodgui.ui
        dsdstatustextdialog.ui
    )
    set(dsddemod_HEADERS
        ${dsddemod_HEADERS}
        dsddemodgui.h
        dsdstatustextdialog.h
    )

    set(TARGET_NAME demoddsd)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demoddsdsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${dsddemod_SOURCES}
)

if(LIBDSDCC_EXTERNAL)
    add_dependencies(${TARGET_NAME} dsdcc mbelib)
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${LIBDSDCC_LIBRARIES}
    ${LIBMBE_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
