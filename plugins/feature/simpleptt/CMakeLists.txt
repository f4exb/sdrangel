project(simpleptt)

set(simpleptt_SOURCES
	simpleptt.cpp
    simplepttsettings.cpp
    simplepttplugin.cpp
    simplepttworker.cpp
    simplepttreport.cpp
    simplepttwebapiadapter.cpp
    simplepttcommand.cpp
    simplepttmessages.cpp
)

set(simpleptt_HEADERS
	simpleptt.h
    simplepttsettings.h
    simplepttplugin.h
    simplepttworker.h
    simplepttreport.h
    simplepttwebapiadapter.h
    simplepttcommand.h
    simplepttmessages.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(simpleptt_SOURCES
        ${simpleptt_SOURCES}
        simplepttgui.cpp
        simplepttgui.ui
    )
    set(simpleptt_HEADERS
        ${simpleptt_HEADERS}
	    simplepttgui.h
    )

    set(TARGET_NAME featuresimpleptt)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featuresimplepttsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${simpleptt_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
