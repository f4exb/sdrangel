project(demodfreedv)

set(freedv_SOURCES
    freedvdemod.cpp
    freedvdemodbaseband.cpp
    freedvdemodsink.cpp
    freedvdemodsettings.cpp
    freedvdemodwebapiadapter.cpp
	freedvplugin.cpp
)

set(freedv_HEADERS
	freedvdemod.h
    freedvdemodbaseband.h
    freedvdemodsink.h
    freedvdemodsettings.h
    freedvdemodwebapiadapter.h
	freedvplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CODEC2_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(freedv_SOURCES
        ${freedv_SOURCES}
        freedvdemodgui.cpp
	    freedvdemodgui.ui
    )
    set(freedv_HEADERS
        ${freedv_HEADERS}
        freedvdemodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}demodfreedv)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodfreedvsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${freedv_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME FreeDVDemodPlugin ${freedv_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(CODEC2_EXTERNAL)
    add_dependencies(${TARGET_NAME} codec2)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${CODEC2_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
