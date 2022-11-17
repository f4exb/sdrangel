project(demodapt)

set(demodapt_SOURCES
    aptdemod.cpp
    aptdemodsettings.cpp
    aptdemodbaseband.cpp
    aptdemodsink.cpp
    aptdemodimageworker.cpp
    aptdemodplugin.cpp
    aptdemodwebapiadapter.cpp
)

set(demodapt_HEADERS
    aptdemod.h
    aptdemodsettings.h
    aptdemodbaseband.h
    aptdemodsink.h
    aptdemodimageworker.h
    aptdemodplugin.h
    aptdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${APT_INCLUDE_DIR}
    ${SGP4_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(demodapt_SOURCES
        ${demodapt_SOURCES}
        aptdemodgui.cpp
        aptdemodgui.ui
        aptdemodsettingsdialog.cpp
        aptdemodsettingsdialog.ui
        aptdemodselectdialog.cpp
        aptdemodselectdialog.ui
        icons.qrc
    )
    set(demodapt_HEADERS
        ${demodapt_HEADERS}
        aptdemodgui.h
        aptdemodsettingsdialog.h
        aptdemodselectdialog.h
    )

    set(TARGET_NAME demodapt)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodaptsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demodapt_SOURCES}
)

if(APT_EXTERNAL)
    add_dependencies(${TARGET_NAME} apt)
endif()

if(SGP4_EXTERNAL)
    add_dependencies(${TARGET_NAME} sgp4)
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    ${APT_LIBRARIES}
    ${SGP4_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
