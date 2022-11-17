project(remotesink)

set(remotesink_SOURCES
    remotesink.cpp
    remotesinkbaseband.cpp
    remotesinksink.cpp
    remotesinksettings.cpp
    remotesinkwebapiadapter.cpp
    remotesinksender.cpp
    remotesinkfifo.cpp
	remotesinkplugin.cpp
)

set(remotesink_HEADERS
	remotesink.h
    remotesinkbaseband.h
    remotesinksink.h
    remotesinksettings.h
    remotesinkwebapiadapter.h
    remotesinksender.h
    remotesinkfifo.h
	remotesinkplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CM256CC_INCLUDE_DIR}
    ${CUSTOM_WINDOWS_INCLUDE}
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(remotesink_SOURCES
        ${remotesink_SOURCES}
        remotesinkgui.cpp
	    remotesinkgui.ui
    )
    set(remotesink_HEADERS
        ${remotesink_HEADERS}
        remotesinkgui.h
    )
    set(TARGET_NAME remotesink)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME remotesinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${remotesink_SOURCES}
)

if(CM256CC_EXTERNAL)
    add_dependencies(${TARGET_NAME} cm256cc)
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${CM256CC_LIBRARIES}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
