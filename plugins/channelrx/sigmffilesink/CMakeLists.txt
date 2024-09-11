project(sigmffilesink)

set(sigmffilesink_SOURCES
    sigmffilesink.cpp
    sigmffilesinkbaseband.cpp
    sigmffilesinksink.cpp
    sigmffilesinksettings.cpp
    sigmffilesinkmessages.cpp
    sigmffilesinkwebapiadapter.cpp
    sigmffilesinkplugin.cpp
)

set(sigmffilesink_HEADERS
	sigmffilesink.h
    sigmffilesinkbaseband.h
    sigmffilesinksink.h
    sigmffilesinksettings.h
    sigmffilesinkmessages.h
    sigmffilesinkwebapiadapter.h
	sigmffilesinkplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(sigmffilesink_SOURCES
        ${sigmffilesink_SOURCES}
        sigmffilesinkgui.cpp
        sigmffilesinkgui.ui
    )
    set(sigmffilesink_HEADERS
        ${sigmffilesink_HEADERS}
        sigmffilesinkgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}sigmffilesink)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}sigmffilesinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${sigmffilesink_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME SigMFSinkPlugin ${sigmffilesink_SOURCES})
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
