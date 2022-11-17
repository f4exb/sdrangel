project(nfm)

set(nfm_SOURCES
    dcsdetector.cpp
	nfmdemod.cpp
    nfmdemodsettings.cpp
    nfmdemodwebapiadapter.cpp
    nfmplugin.cpp
    nfmdemodreport.cpp
    nfmdemodsink.cpp
    nfmdemodbaseband.cpp
)

set(nfm_HEADERS
    dcsdetector.h
	nfmdemod.h
    nfmdemodsettings.h
    nfmdemodwebapiadapter.h
    nfmplugin.h
    nfmdemodreport.h
    nfmdemodsink.h
    nfmdemodbaseband.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(nfm_SOURCES
        ${nfm_SOURCES}
        nfmdemodgui.cpp

	nfmdemodgui.ui
    )
    set(nfm_HEADERS
        ${nfm_HEADERS}
        nfmdemodgui.h
    )

    set(TARGET_NAME demodnfm)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodnfmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${nfm_SOURCES}
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
