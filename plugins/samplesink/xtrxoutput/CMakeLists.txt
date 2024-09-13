project(xtrxoutput)

set(xtrxoutput_SOURCES
	xtrxoutput.cpp
	xtrxoutputplugin.cpp
	xtrxoutputsettings.cpp
    xtrxoutputthread.cpp
    xtrxoutputwebapiadapter.cpp
)

set(xtrxoutput_HEADERS
	xtrxoutput.h
	xtrxoutputplugin.h
	xtrxoutputsettings.h
    xtrxoutputthread.h
    xtrxoutputwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBXTRX_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(xtrxoutput_SOURCES
        ${xtrxoutput_SOURCES}
        xtrxoutputgui.cpp
        xtrxoutputgui.ui
    )
    set(xtrxoutput_HEADERS
        ${xtrxoutput_HEADERS}
        xtrxoutputgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}outputxtrx)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}outputxtrxsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${xtrxoutput_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME XTRXOutputPlugin ${xtrxoutput_SOURCES})
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
    ${LIBXTRX_LIBRARY}
    xtrxdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
