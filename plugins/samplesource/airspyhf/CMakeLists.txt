project(airspyhf)

set(airspyhf_SOURCES
    airspyhfinput.cpp
    airspyhfplugin.cpp
    airspyhfsettings.cpp
    airspyhfwebapiadapter.cpp
    airspyhfworker.cpp
)

set(airspyhf_HEADERS
    airspyhfinput.h
    airspyhfplugin.h
    airspyhfsettings.h
    airspyhfwebapiadapter.h
    airspyhfworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBAIRSPYHF_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(airspyhf_SOURCES
        ${airspyhf_SOURCES}
        airspyhfgui.cpp

        airspyhfgui.ui
    )
    set(airspyhf_HEADERS
        ${airspyhf_HEADERS}
        airspyhfgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputairspyhf)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputairspyhfsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${airspyhf_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AirspyHFPlugin ${airspyhf_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(LIBAIRSPYHF_EXTERNAL)
    add_dependencies(${TARGET_NAME} airspyhf)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBAIRSPYHF_LIBRARIES}
        ${LIBUSB_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
