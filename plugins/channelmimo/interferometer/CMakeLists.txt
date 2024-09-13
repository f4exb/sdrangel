project(interferometer)

set(interferometer_SOURCES
    interferometer.cpp
    interferometercorr.cpp
    interferometersettings.cpp
    interferometerbaseband.cpp
    interferometerstreamsink.cpp
    interferometerplugin.cpp
    interferometerwebapiadapter.cpp
)

set(interferometer_HEADERS
    interferometer.h
    interferometercorr.h
    interferometersettings.h
    interferometerbaseband.h
    interferometerstreamsink.h
    interferometerplugin.h
    interferometerwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIR}
)

if (NOT SERVER_MODE)
    set(interferometer_SOURCES
        ${interferometer_SOURCES}
        interferometergui.cpp
        interferometergui.ui
    )
    set(interferometer_HEADERS
        ${interferometer_HEADERS}
        interferometergui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}interferometer)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}interferometersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${interferometer_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME InterferometerPlugin ${interferometer_SOURCES})
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
