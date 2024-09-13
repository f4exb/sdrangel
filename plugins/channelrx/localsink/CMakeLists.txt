project(localsink)

set(localsink_SOURCES
    localsink.cpp
    localsinkbaseband.cpp
    localsinksink.cpp
    localsinksettings.cpp
    localsinkwebapiadapter.cpp
    localsinkworker.cpp
    localsinkplugin.cpp
)

set(localsink_HEADERS
	localsink.h
    localsinkbaseband.h
    localsinksink.h
    localsinksettings.h
    localsinkwebapiadapter.h
	localsinkworker.h
	localsinkplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(localsink_SOURCES
        ${localsink_SOURCES}
        localsinkgui.cpp
        localsinkgui.ui
    )
    set(localsink_HEADERS
        ${localsink_HEADERS}
        localsinkgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}localsink)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}localsinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${localsink_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME LocalSinkPlugin ${localsink_SOURCES})
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
