project(demodrtty)

set(demodrtty_SOURCES
    rttydemod.cpp
    rttydemodsettings.cpp
    rttydemodbaseband.cpp
    rttydemodsink.cpp
    rttydemodplugin.cpp
    rttydemodwebapiadapter.cpp
)

set(demodrtty_HEADERS
    rttydemod.h
    rttydemodsettings.h
    rttydemodbaseband.h
    rttydemodsink.h
    rttydemodplugin.h
    rttydemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodrtty_SOURCES
        ${demodrtty_SOURCES}
        rttydemodgui.cpp
        rttydemodgui.ui
    )
    set(demodrtty_HEADERS
        ${demodrtty_HEADERS}
        rttydemodgui.h
    )

    set(TARGET_NAME demodrtty)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodrttysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demodrtty_SOURCES}
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
