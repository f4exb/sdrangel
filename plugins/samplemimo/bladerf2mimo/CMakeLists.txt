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
    set(TARGET_NAME mimobladerf2)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME mimobladerf2srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${bladerf2mimo_SOURCES}
)

if(LIBBLADERF_EXTERNAL)
    add_dependencies(${TARGET_NAME} bladerf)
endif()

target_link_libraries(${TARGET_NAME}
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
