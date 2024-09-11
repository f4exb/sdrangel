project(hackrfinput)

set(hackrfinput_SOURCES
    hackrfinput.cpp
    hackrfinputplugin.cpp
    hackrfinputsettings.cpp
    hackrfinputwebapiadapter.cpp
    hackrfinputthread.cpp
)

set(hackrfinput_HEADERS
    hackrfinput.h
    hackrfinputplugin.h
    hackrfinputsettings.h
    hackrfinputwebapiadapter.h
    hackrfinputthread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBHACKRF_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(hackrfinput_SOURCES
        ${hackrfinput_SOURCES}
        hackrfinputgui.cpp

        hackrfinputgui.ui
    )
    set(hackrfinput_HEADERS
        ${hackrfinput_HEADERS}
        hackrfinputgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputhackrf)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputhackrfsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${hackrfinput_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME HackRFInputPlugin ${hackrfinput_SOURCES})
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
        ${LIBHACKRF_LIBRARIES}
        hackrfdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
