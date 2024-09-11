project(doa2)

set(doa2_SOURCES
    doa2.cpp
    doa2corr.cpp
    doa2settings.cpp
    doa2baseband.cpp
    doa2streamsink.cpp
    doa2plugin.cpp
    doa2webapiadapter.cpp
)

set(doa2_HEADERS
    doa2.h
    doa2corr.h
    doa2settings.h
    doa2baseband.h
    doa2streamsink.h
    doa2plugin.h
    doa2webapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIR}
)

if (NOT SERVER_MODE)
    set(doa2_SOURCES
        ${doa2_SOURCES}
        doa2gui.cpp
        doa2compass.cpp
        doa2gui.ui
    )
    set(doa2_HEADERS
        ${doa2_HEADERS}
        doa2gui.h
        doa2compass.cpp
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}doa2)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}doa2srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${doa2_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DOA2Plugin ${doa2_SOURCES})
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
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
