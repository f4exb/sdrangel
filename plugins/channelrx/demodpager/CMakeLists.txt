project(demodpager)

set(demodpager_SOURCES
    pagerdemod.cpp
    pagerdemodsettings.cpp
    pagerdemodbaseband.cpp
    pagerdemodsink.cpp
    pagerdemodplugin.cpp
    pagerdemodwebapiadapter.cpp
)

set(demodpager_HEADERS
    pagerdemod.h
    pagerdemodsettings.h
    pagerdemodbaseband.h
    pagerdemodsink.h
    pagerdemodplugin.h
    pagerdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodpager_SOURCES
        ${demodpager_SOURCES}
        pagerdemodgui.cpp
        pagerdemodgui.ui
        pagerdemodcharsetdialog.cpp
        pagerdemodcharsetdialog.ui
    )
    set(demodpager_HEADERS
        ${demodpager_HEADERS}
        pagerdemodgui.h
        pagerdemodcharsetdialog.h
    )

    set(TARGET_NAME demodpager)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodpagersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demodpager_SOURCES}
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
