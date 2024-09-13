project(bladerf1input)

set(bladerf1input_SOURCES
	bladerf1input.cpp
	bladerf1inputplugin.cpp
    bladerf1inputsettings.cpp
    bladerf1inputwebapiadapter.cpp
	bladerf1inputthread.cpp
)

set(bladerf1input_HEADERS
	bladerf1input.h
	bladerf1inputplugin.h
    bladerf1inputsettings.h
    bladerf1inputwebapiadapter.h
	bladerf1inputthread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBBLADERF_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(bladerf1input_SOURCES
        ${bladerf1input_SOURCES}
        bladerf1inputgui.cpp
        bladerf1inputgui.ui
    )
    set(bladerf1input_HEADERS
        ${bladerf1input_HEADERS}
        bladerf1inputgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputbladerf1)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputbladerf1srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${bladerf1input_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME Blderf1InputPlugin ${bladerf1input_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(LIBBLADERF_EXTERNAL)
    add_dependencies(${TARGET_NAME} bladerf)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${LIBBLADERF_LIBRARIES}
    bladerf1device
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
