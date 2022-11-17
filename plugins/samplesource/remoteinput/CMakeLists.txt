project(remoteinput)

set(remoteinput_SOURCES
    remoteinputbuffer.cpp
    remoteinputudphandler.cpp
    remoteinput.cpp
    remoteinputsettings.cpp
    remoteinputwebapiadapter.cpp
    remoteinputplugin.cpp
)

set(remoteinput_HEADERS
    remoteinputbuffer.h
    remoteinputudphandler.h
    remoteinput.h
    remoteinputsettings.h
    remoteinputwebapiadapter.h
    remoteinputplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(remoteinput_SOURCES
        ${remoteinput_SOURCES}
        remoteinputgui.cpp
        remoteinputgui.ui
    )
    set(remoteinput_HEADERS
        ${remoteinput_HEADERS}
        remoteinputgui.h
    )

    set(TARGET_NAME inputremote)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputremotesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${remoteinput_SOURCES}
)

if(CM256CC_EXTERNAL)
    add_dependencies(${TARGET_NAME} cm256cc)
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
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
