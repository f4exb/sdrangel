project(bladerf2mimo)

set(bladerf2mimo_SOURCES
    bladerf2mimo.cpp
	bladerf2mimoplugin.cpp
	bladerf2mithread.cpp
	bladerf2mothread.cpp
    bladerf2mimosettings.cpp
    bladerf2mimowebapiadapter.cpp
)

set(bladerf2mimo_HEADERS
    bladerf2mimo.h
	bladerf2mimoplugin.h
	bladerf2mithread.h
	bladerf2mothread.h
    bladerf2mimosettings.h
    bladerf2mimowebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBBLADERF_INCLUDE_DIRS}
)

if (NOT SERVER_MODE)
    set (bladerf2mimo_SOURCES
        ${bladerf2mimo_SOURCES}
        bladerf2mimogui.cpp
        bladerf2mimogui.ui
    )
    set(bladerf2mimo_HEADERS
        ${bladerf2mimo_HEADERS}
        bladerf2mimogui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}mimobladerf2)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}mimobladerf2srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${bladerf2mimo_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME BladeRF2MIMOPlugin ${bladerf2mimo_SOURCES})
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
