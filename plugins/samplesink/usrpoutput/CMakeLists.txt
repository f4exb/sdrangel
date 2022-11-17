project(usrpoutput)

set(usrpoutput_SOURCES
    usrpoutput.cpp
    usrpoutputplugin.cpp
    usrpoutputsettings.cpp
    usrpoutputwebapiadapter.cpp
    usrpoutputthread.cpp
)

set(usrpoutput_HEADERS
    usrpoutput.h
    usrpoutputplugin.h
    usrpoutputsettings.h
    usrpoutputwebapiadapter.h
    usrpoutputthread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${UHD_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(usrpoutput_SOURCES
        ${usrpoutput_SOURCES}
        usrpoutputgui.cpp
        usrpoutputgui.ui
    )
    set(usrpoutput_HEADERS
        ${usrpoutput_HEADERS}
        usrpoutputgui.h
    )

    set(TARGET_NAME outputusrp)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputusrpsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${usrpoutput_SOURCES}
)

if(UHD_EXTERNAL)
    add_dependencies(${TARGET_NAME} uhd)
endif()

target_link_libraries(${TARGET_NAME}
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
