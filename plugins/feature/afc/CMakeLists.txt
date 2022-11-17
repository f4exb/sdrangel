project(afc)

set(afc_SOURCES
	afc.cpp
    afcsettings.cpp
    afcplugin.cpp
    afcworker.cpp
    afcreport.cpp
    afcwebapiadapter.cpp
)

set(afc_HEADERS
	afc.h
    afcsettings.h
    afcplugin.h
    afcworker.h
    afcreport.h
    afcwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(afc_SOURCES
        ${afc_SOURCES}
        afcgui.cpp
        afcgui.ui
    )
    set(afc_HEADERS
        ${afc_HEADERS}
	    afcgui.h
    )

    set(TARGET_NAME featureafc)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featureafcsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${afc_SOURCES}
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
