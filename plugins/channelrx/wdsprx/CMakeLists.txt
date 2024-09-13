project(wdsprx)

set(wdsprx_SOURCES
	wdsprx.cpp
    wdsprxsettings.cpp
    wdsprxsink.cpp
    wdsprxbaseband.cpp
    wdsprxwebapiadapter.cpp
	wdsprxplugin.cpp
)

set(wdsprx_HEADERS
	wdsprx.h
    wdsprxsettings.h
    wdsprxsink.h
    wdsprxbaseband.h
    wdsprxwebapiadapter.h
	wdsprxplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/wdsp
    ${FFTW3F_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(wdsprx_SOURCES
        ${wdsprx_SOURCES}
        wdsprxagcdialog.cpp
        wdsprxagcdialog.ui
        wdsprxamdialog.cpp
        wdsprxamdialog.ui
        wdsprxcwpeakdialog.cpp
        wdsprxcwpeakdialog.ui
        wdsprxdnbdialog.cpp
        wdsprxdnbdialog.ui
        wdsprxdnrdialog.cpp
        wdsprxdnrdialog.ui
        wdsprxeqdialog.cpp
        wdsprxeqdialog.ui
        wdsprxfmdialog.cpp
        wdsprxfmdialog.ui
        wdsprxpandialog.cpp
        wdsprxpandialog.ui
        wdsprxsquelchdialog.cpp
        wdsprxsquelchdialog.ui
        wdsprxgui.cpp
        wdsprxgui.ui
    )
    set(wdsprx_HEADERS
        ${wdsprx_HEADERS}
        wdsprxgui.h
        wdsprxagcdialog.h
        wdsprxamdialog.h
        wdsprxcwpeakdialog.h
        wdsprxdnbdialog.h
        wdsprxdnrdialog.h
        wdsprxeqdialog.h
        wdsprxfmdialog.h
        wdsprxpandialog.h
        wdsprxsquelchdialog.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}wdsprx)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}wdsprxsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${wdsprx_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME WDSPRxPlugin ${wdsprx_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
    wdsp
	${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
