project(filesource)

set(filesource_SOURCES
    filesource.cpp
    filesourcebaseband.cpp
    filesourcereport.cpp
    filesourcesource.cpp
	filesourceplugin.cpp
    filesourcesettings.cpp
    filesourcewebapiadapter.cpp
)

set(filesource_HEADERS
	filesource.h
    filesourcebaseband.h
    filesourcereport.h
    filesourcesource.h
	filesourceplugin.h
    filesourcesettings.h
    filesourcewebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CUSTOM_WINDOWS_INCLUDE}
)

if(NOT SERVER_MODE)
    set(filesource_SOURCES
        ${filesource_SOURCES}
        filesourcegui.cpp
        filesourcegui.ui
    )
    set(filesource_HEADERS
        ${filesource_HEADERS}
        filesourcegui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}filesource)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}filesourcesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${filesource_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME FileSourcePlugin ${filesource_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    Qt::Network
	sdrbase
	${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
