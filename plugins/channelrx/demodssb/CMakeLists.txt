project(ssb)

set(ssb_SOURCES
	ssbdemod.cpp
    ssbdemodsettings.cpp
    ssbdemodsink.cpp
    ssbdemodbaseband.cpp
    ssbdemodwebapiadapter.cpp
	ssbplugin.cpp
)

set(ssb_HEADERS
	ssbdemod.h
    ssbdemodsettings.h
    ssbdemodsink.h
    ssbdemodbaseband.h
    ssbdemodwebapiadapter.h
	ssbplugin.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(ssb_SOURCES
        ${ssb_SOURCES}
        ssbdemodgui.cpp
        ssbdemodgui.ui
    )
    set(ssb_HEADERS
        ${ssb_HEADERS}
        ssbdemodgui.h
    )
    set(TARGET_NAME demodssb)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodssbsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${ssb_SOURCES}
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
