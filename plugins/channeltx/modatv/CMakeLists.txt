project(modatv)

set(modatv_SOURCES
    atvmod.cpp
    atvmodbaseband.cpp
    atvmodreport.cpp
    atvmodsource.cpp
	atvmodplugin.cpp
    atvmodsettings.cpp
    atvmodwebapiadapter.cpp
)

set(modatv_HEADERS
	atvmod.h
    atvmodbaseband.h
    atvmodreport.h
    atvmodsource.h
	atvmodplugin.h
    atvmodsettings.h
    atvmodwebapiadapter.h
)

include_directories(
	${OpenCV_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modatv_SOURCES
        ${modatv_SOURCES}
        atvmodgui.cpp

        atvmodgui.ui
    )
    set(modatv_HEADERS
        ${modatv_HEADERS}
        atvmodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}modatv)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modatvsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modatv_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME ATVModPlugin ${modatv_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        "${OpenCV_LIBS}"
)

if(DEFINED OpenCV_DEPENDS)
    add_dependencies(${TARGET_NAME} ${OpenCV_DEPENDS})
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
