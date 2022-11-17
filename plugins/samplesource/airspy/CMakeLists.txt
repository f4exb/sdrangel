project(airspy)

set(airspy_SOURCES
    airspyinput.cpp
    airspyplugin.cpp
    airspysettings.cpp
    airspywebapiadapter.cpp
    airspyworker.cpp
)

set(airspy_HEADERS
    airspyinput.h
    airspyplugin.h
    airspysettings.h
    airspywebapiadapter.h
    airspyworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBAIRSPY_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(airspy_SOURCES
        ${airspy_SOURCES}
        airspygui.cpp
        airspygui.ui
    )
    set(airspy_HEADERS
        ${airspy_HEADERS}
        airspygui.h
    )

    set(TARGET_NAME inputairspy)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputairspysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${airspy_SOURCES}
)

if(LIBAIRSPY_EXTERNAL AND NOT LINUX)
    add_dependencies(${TARGET_NAME} airspy)
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${LIBAIRSPY_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
