project(localsource)

set(localsource_SOURCES
    localsource.cpp
    localsourcebaseband.cpp
    localsourcesource.cpp
	localsourceworker.cpp
	localsourceplugin.cpp
    localsourcesettings.cpp
    localsourcewebapiadapter.cpp
)

set(localsource_HEADERS
    localsource.h
    localsourcebaseband.h
    localsourcesource.h
	localsourceworker.h
	localsourceplugin.h
    localsourcesettings.h
    localsourcewebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(localsource_SOURCES
        ${localsource_SOURCES}
        localsourcegui.cpp

        localsourcegui.ui
    )
    set(localsource_HEADERS
        ${localsource_HEADERS}
        localsourcegui.h
    )

    set(TARGET_NAME localsource)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME localsourcesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${localsource_SOURCES}
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
