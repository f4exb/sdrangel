project(remotesource)

set(remotesource_SOURCES
    remotesource.cpp
    remotesourcebaseband.cpp
    remotesourcesource.cpp
	remotesourceworker.cpp
	remotesourceplugin.cpp
    remotesourcesettings.cpp
    remotesourcewebapiadapter.cpp
)

set(remotesource_HEADERS
    remotesource.h
    remotesourcebaseband.h
    remotesourcesource.h
	remotesourceworker.h
	remotesourceplugin.h
    remotesourcesettings.h
    remotesourcewebapiadapter.cpp
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
    ${CUSTOM_WINDOWS_INCLUDE}
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(remotesource_SOURCES
        ${remotesource_SOURCES}
        remotesourcegui.cpp
        remotesourcegui.ui
    )
    set(remotesource_HEADERS
        ${remotesource_HEADERS}
        remotesourcegui.h
    )

    set(TARGET_NAME remotesource)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME remotesourcesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${remotesource_SOURCES}
)

if(CM256CC_EXTERNAL)
    add_dependencies(${TARGET_NAME} cm256cc)
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
    Qt::Network
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${CM256CC_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
