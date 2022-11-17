project(localsink)

set(localsink_SOURCES
    localsink.cpp
    localsinkbaseband.cpp
    localsinksink.cpp
    localsinksettings.cpp
    localsinkwebapiadapter.cpp
    localsinkworker.cpp
    localsinkplugin.cpp
)

set(localsink_HEADERS
	localsink.h
    localsinkbaseband.h
    localsinksink.h
    localsinksettings.h
    localsinkwebapiadapter.h
	localsinkworker.h
	localsinkplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(localsink_SOURCES
        ${localsink_SOURCES}
        localsinkgui.cpp
        localsinkgui.ui
    )
    set(localsink_HEADERS
        ${localsink_HEADERS}
        localsinkgui.h
    )
    set(TARGET_NAME localsink)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME localsinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${localsink_SOURCES}
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
