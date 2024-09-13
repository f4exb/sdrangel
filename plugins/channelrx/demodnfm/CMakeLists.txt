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

    set(TARGET_NAME ${PLUGINS_PREFIX}demodnfm)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodnfmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${nfm_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME NFMPlugin ${nfm_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
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
