project(demodradiosonde)

set(demodradiosonde_SOURCES
    radiosondedemod.cpp
    radiosondedemodsettings.cpp
    radiosondedemodbaseband.cpp
    radiosondedemodsink.cpp
    radiosondedemodplugin.cpp
    radiosondedemodwebapiadapter.cpp
)

set(demodradiosonde_HEADERS
    radiosondedemod.h
    radiosondedemodsettings.h
    radiosondedemodbaseband.h
    radiosondedemodsink.h
    radiosondedemodplugin.h
    radiosondedemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodradiosonde_SOURCES
        ${demodradiosonde_SOURCES}
        radiosondedemodgui.cpp
        radiosondedemodgui.ui
    )
    set(demodradiosonde_HEADERS
        ${demodradiosonde_HEADERS}
        radiosondedemodgui.h
    )

    set(TARGET_NAME demodradiosonde)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodradiosondesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demodradiosonde_SOURCES}
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

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
