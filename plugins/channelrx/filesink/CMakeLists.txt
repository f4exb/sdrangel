project(filesink)

set(filesink_SOURCES
    filesink.cpp
    filesinkbaseband.cpp
    filesinksink.cpp
    filesinksettings.cpp
    filesinkmessages.cpp
    filesinkwebapiadapter.cpp
    filesinkplugin.cpp
)

set(filesink_HEADERS
	filesink.h
    filesinkbaseband.h
    filesinksink.h
    filesinksettings.h
    filesinkmessages.h
    filesinkwebapiadapter.h
	filesinkplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(filesink_SOURCES
        ${filesink_SOURCES}
        filesinkgui.cpp
        filesinkgui.ui
    )
    set(filesink_HEADERS
        ${filesink_HEADERS}
        filesinkgui.h
    )
    set(TARGET_NAME filesink)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME filesinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${filesink_SOURCES}
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
