project(airspy)

set(airspy_SOURCES
    airspyinput.cpp
    airspyplugin.cpp
    airspysettings.cpp
    airspywebapiadapter.cpp
    airspyworker.cpp
)

set(airspy_HEADERS
    airspyinput.h
    airspyplugin.h
    airspysettings.h
    airspywebapiadapter.h
    airspyworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBAIRSPY_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(airspy_SOURCES
        ${airspy_SOURCES}
        airspygui.cpp
        airspygui.ui
    )
    set(airspy_HEADERS
        ${airspy_HEADERS}
        airspygui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputairspy)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputairspysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${airspy_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AirspyPlugin ${airspy_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(LIBAIRSPY_EXTERNAL AND NOT LINUX)
    add_dependencies(${TARGET_NAME} airspy)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${LIBAIRSPY_LIBRARIES}
    ${LIBUSB_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
