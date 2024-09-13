project(bladerf2input)

set(bladerf2input_SOURCES
	bladerf2input.cpp
	bladerf2inputplugin.cpp
    bladerf2inputsettings.cpp
    bladerf2inputwebapiadapter.cpp
	bladerf2inputthread.cpp
)

set(bladerf2input_HEADERS
	bladerf2input.h
	bladerf2inputplugin.h
    bladerf2inputsettings.h
    bladerf2inputwebapiadapter.h
	bladerf2inputthread.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBBLADERF_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(bladerf2input_SOURCES
        ${bladerf2input_SOURCES}
        bladerf2inputgui.cpp

        bladerf2inputgui.ui
    )
    set(bladerf2input_HEADERS
        ${bladerf2input_HEADERS}
        bladerf2inputgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputbladerf2)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputbladerf2srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${bladerf2input_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME Blderf2InputPlugin ${bladerf2input_SOURCES})
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
        bladerf2device
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
