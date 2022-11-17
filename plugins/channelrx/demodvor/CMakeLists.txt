project(vor)

set(vor_SOURCES
    vordemod.cpp
    vordemodsettings.cpp
    vordemodbaseband.cpp
    vordemodsink.cpp
    vordemodplugin.cpp
    vordemodwebapiadapter.cpp
    vordemodreport.cpp
)

set(vor_HEADERS
    vordemod.h
    vordemodsettings.h
    vordemodbaseband.h
    vordemodsink.h
    vordemodplugin.h
    vordemodwebapiadapter.h
    vordemodreport.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(vor_SOURCES
        ${vor_SOURCES}
        vordemodgui.cpp
        vordemodgui.ui
    )
    set(vor_HEADERS
        ${vor_HEADERS}
        vordemodgui.h
    )

    set(TARGET_NAME demodvor)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodvorsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${vor_SOURCES}
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
