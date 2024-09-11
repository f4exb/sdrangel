project(fcdpro)

set(fcdpro_SOURCES
	fcdproinput.cpp
	fcdproplugin.cpp
    fcdprosettings.cpp
    fcdprowebapiadapter.cpp
	fcdprothread.cpp
)

set(fcdpro_HEADERS
	fcdproinput.h
	fcdproplugin.h
    fcdprosettings.h
    fcdprowebapiadapter.h
	fcdprothread.h
)

include_directories(
    ${HIDAPI_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/fcdhid
	${CMAKE_SOURCE_DIR}/fcdlib
)

if(NOT SERVER_MODE)
    set(fcdpro_SOURCES
        ${fcdpro_SOURCES}
        fcdprogui.cpp
        fcdprogui.ui
    )
    set(fcdpro_HEADERS
        ${fcdpro_HEADERS}
        fcdprogui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputfcdpro)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputfcdprosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${fcdpro_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME FCDProPlugin ${fcdpro_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    ${HIDAPI_LIBRARIES}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    fcdhid
    fcdlib
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
