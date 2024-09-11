project(usrpinput)

set(usrpinput_SOURCES
    usrpinput.cpp
    usrpinputplugin.cpp
    usrpinputsettings.cpp
    usrpinputwebapiadapter.cpp
    usrpinputthread.cpp
)

set(usrpinput_HEADERS
    usrpinput.h
    usrpinputplugin.h
    usrpinputsettings.h
    usrpinputwebapiadapter.h
    usrpinputthread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${UHD_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(usrpinput_SOURCES
        ${usrpinput_SOURCES}
        usrpinputgui.cpp
        usrpinputgui.ui
    )
    set(usrpinput_HEADERS
        ${usrpinput_HEADERS}
        usrpinputgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputusrp)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputusrpsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${usrpinput_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME USRPInputPlugin ${usrpinput_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(UHD_EXTERNAL)
    add_dependencies(${TARGET_NAME} uhd)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
    ${UHD_LIBRARIES}
    usrpdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
