project(fcdproplus)

set(fcdproplus_SOURCES
	fcdproplusinput.cpp
	fcdproplusplugin.cpp
    fcdproplussettings.cpp
    fcdpropluswebapiadapter.cpp
	fcdproplusthread.cpp
)

set(fcdproplus_HEADERS
	fcdproplusinput.h
	fcdproplusplugin.h
    fcdproplussettings.h
    fcdpropluswebapiadapter.h
	fcdproplusthread.h
)

include_directories(
    ${HIDAPI_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/fcdhid
	${CMAKE_SOURCE_DIR}/fcdlib
)

if(NOT SERVER_MODE)
    set(fcdproplus_SOURCES
        ${fcdproplus_SOURCES}
        fcdproplusgui.cpp
        fcdproplusgui.ui
    )
    set(fcdproplus_HEADERS
        ${fcdproplus_HEADERS}
        fcdproplusgui.h
    )

    set(TARGET_NAME inputfcdproplus)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputfcdproplussrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${fcdproplus_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    ${HIDAPI_LIBRARIES}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    fcdhid
    fcdlib
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
