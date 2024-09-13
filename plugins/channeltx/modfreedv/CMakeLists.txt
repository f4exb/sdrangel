project(modfreedv)

set(modfreedv_SOURCES
    freedvmod.cpp
    freedvmodbaseband.cpp
    freedvmodsource.cpp
    freedvmodplugin.cpp
    freedvmodsettings.cpp
    freedvmodwebapiadapter.cpp
)

set(modfreedv_HEADERS
    freedvmod.h
    freedvmodbaseband.h
    freedvmodsource.h
    freedvmodplugin.h
    freedvmodsettings.h
    freedvmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CODEC2_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(modfreedv_SOURCES
        ${modfreedv_SOURCES}
        freedvmodgui.cpp
        freedvmodgui.ui
    )
    set(modfreedv_HEADERS
        ${modfreedv_HEADERS}
        freedvmodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}modfreedv)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modfreedvsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modfreedv_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME FreeDVModPlugin ${modfreedv_SOURCES})
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
