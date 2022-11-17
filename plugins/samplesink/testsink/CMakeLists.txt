project(testsink)

set(testsink_SOURCES
	testsinkoutput.cpp
	testsinkplugin.cpp
	testsinksettings.cpp
	testsinkworker.cpp
)

set(testsink_HEADERS
	testsinkoutput.h
	testsinkplugin.h
	testsinksettings.h
	testsinkworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(testsink_SOURCES
        ${testsink_SOURCES}
        testsinkgui.cpp
        testsinkgui.ui
    )
    set(testsink_HEADERS
        ${testsink_HEADERS}
        testsinkgui.h
    )

    set(TARGET_NAME outputtestsink)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputtestsinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${testsink_SOURCES}
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
