project(demodils)

set(demodils_SOURCES
    ilsdemod.cpp
    ilsdemodsettings.cpp
    ilsdemodbaseband.cpp
    ilsdemodsink.cpp
    ilsdemodplugin.cpp
    ilsdemodwebapiadapter.cpp
)

set(demodils_HEADERS
    ilsdemod.h
    ilsdemodsettings.h
    ilsdemodbaseband.h
    ilsdemodsink.h
    ilsdemodplugin.h
    ilsdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodils_SOURCES
        ${demodils_SOURCES}
        ilsdemodgui.cpp
        ilsdemodgui.ui
    )
    set(demodils_HEADERS
        ${demodils_HEADERS}
        ilsdemodgui.h
    )

    set(TARGET_NAME demodils)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodilssrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demodils_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
