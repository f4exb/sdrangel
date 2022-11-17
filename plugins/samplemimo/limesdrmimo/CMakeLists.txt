project(limesdrmimo)

set(limesdrmimo_SOURCES
    limesdrmimo.cpp
	limesdrmimoplugin.cpp
	limesdrmithread.cpp
	limesdrmothread.cpp
    limesdrmimosettings.cpp
    limesdrmimowebapiadapter.cpp
)

set(limesdrmimo_HEADERS
    limesdrmimo.h
	limesdrmimoplugin.h
	limesdrmithread.h
	limesdrmothread.h
    limesdrmimosettings.h
    limesdrmimowebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIMESUITE_INCLUDE_DIR}
)

if (NOT SERVER_MODE)
    set (limesdrmimo_SOURCES
        ${limesdrmimo_SOURCES}
        limesdrmimogui.cpp
        limesdrmimogui.ui
    )
    set(limesdrmimo_HEADERS
        ${limesdrmimo_HEADERS}
        limesdrmimogui.h
    )
    set(TARGET_NAME mimolimesdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME mimolimesdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${limesdrmimo_SOURCES}
)

if(LIMESUITE_EXTERNAL)
    add_dependencies(${TARGET_NAME} limesuite)
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${LIMESUITE_LIBRARY}
    limesdrdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
