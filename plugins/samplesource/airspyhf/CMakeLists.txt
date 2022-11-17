project(airspyhf)

set(airspyhf_SOURCES
    airspyhfinput.cpp
    airspyhfplugin.cpp
    airspyhfsettings.cpp
    airspyhfwebapiadapter.cpp
    airspyhfworker.cpp
)

set(airspyhf_HEADERS
    airspyhfinput.h
    airspyhfplugin.h
    airspyhfsettings.h
    airspyhfwebapiadapter.h
    airspyhfworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBAIRSPYHF_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(airspyhf_SOURCES
        ${airspyhf_SOURCES}
        airspyhfgui.cpp

        airspyhfgui.ui
    )
    set(airspyhf_HEADERS
        ${airspyhf_HEADERS}
        airspyhfgui.h
    )

    set(TARGET_NAME inputairspyhf)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputairspyhfsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${airspyhf_SOURCES}
)

if(LIBAIRSPYHF_EXTERNAL)
    add_dependencies(${TARGET_NAME} airspyhf)
endif()

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBAIRSPYHF_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
