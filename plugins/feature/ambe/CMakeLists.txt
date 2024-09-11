project(ambe)

set(ambe_SOURCES
	ambe.cpp
    ambesettings.cpp
    ambeplugin.cpp
    ambeengine.cpp
    ambeworker.cpp
    ambewebapiadapter.cpp
)

set(ambe_HEADERS
	ambe.h
    ambesettings.h
    ambeplugin.h
    ambeengine.h
    ambeworker.h
    ambewebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBSERIALDV_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(ambe_SOURCES
        ${ambe_SOURCES}
        ambegui.cpp
        ambegui.ui
    )
    set(ambe_HEADERS
        ${ambe_HEADERS}
	    ambegui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featureambe)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featureambesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${ambe_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AMBEPlugin ${ambe_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${LIBSERIALDV_LIBRARY}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
