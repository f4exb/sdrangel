project(filesinktxt)

set(filesinktxt_SOURCES
    filesinktxt.cpp
    filesinktxtsettings.cpp
    filesinktxtwebapiadapter.cpp
    filesinktxtplugin.cpp
    detection.cpp
)

set(filesinktxt_HEADERS
	filesinktxt.h
    filesinktxtsettings.h
    filesinktxtwebapiadapter.h
	filesinktxtplugin.h
    detection.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(filesinktxt_SOURCES
        ${filesinktxt_SOURCES}
        filesinktxtgui.cpp
        filesinktxtgui.ui
    )
    set(filesinktxt_HEADERS
        ${filesinktxt_HEADERS}
        filesinktxtgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}filesinktxt)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}filesinktxtsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()


if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${filesinktxt_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME FileSinkTxtPlugin ${filesinktxt_SOURCES})
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
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()