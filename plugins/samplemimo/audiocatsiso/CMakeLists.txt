project(audiocatsiso)

set(audiocatsiso_SOURCES
    audiocatinputworker.cpp
    audiocatoutputworker.cpp
	audiocatsiso.cpp
	audiocatsisoplugin.cpp
    audiocatsisosettings.cpp
    audiocatsisowebapiadapter.cpp
)

set(audiocatsiso_HEADERS
    audiocatinputworker.h
    audiocatoutputworker.h
    audiocatsiso.h
	audiocatsisoplugin.h
    audiocatsisosettings.h
    audiocatsisowebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
)

if (NOT SERVER_MODE)
    set (audiocatsiso_SOURCES
        ${audiocatsiso_SOURCES}
        audiocatsisogui.cpp
        audiocatsisogui.ui
    )
    set(audiocatsiso_HEADERS
        ${audiocatsiso_HEADERS}
        audiocatsisogui.h
    )
    set(TARGET_NAME mimoaudiocatsiso)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME mimoaudiocatsisosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${audiocatsiso_SOURCES}
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
