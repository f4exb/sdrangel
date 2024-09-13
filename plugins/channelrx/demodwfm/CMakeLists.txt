project(wfm)

set(wfm_SOURCES
	wfmdemod.cpp
    wfmdemodsettings.cpp
    wfmdemodsink.cpp
    wfmdemodbaseband.cpp
    wfmdemodwebapiadapter.cpp
	wfmplugin.cpp
)

set(wfm_HEADERS
	wfmdemod.h
    wfmdemodsettings.h
    wfmdemodsink.h
    wfmdemodbaseband.h
    wfmdemodwebapiadapter.h
	wfmplugin.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(wfm_SOURCES
        ${wfm_SOURCES}
        wfmdemodgui.cpp
	    wfmdemodgui.ui
    )
    set(wfm_HEADERS
        ${wfm_HEADERS}
        wfmdemodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodwfm)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodwfmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${wfm_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME WFMPlugin ${wfm_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
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
