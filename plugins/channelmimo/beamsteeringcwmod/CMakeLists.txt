project(beamsteeringcwmod)

set(beamsteeringcwmod_SOURCES
    beamsteeringcwmod.cpp
    beamsteeringcwmodsettings.cpp
    beamsteeringcwmodbaseband.cpp
    beamsteeringcwmodstreamsource.cpp
    beamsteeringcwmodplugin.cpp
    beamsteeringcwmodwebapiadapter.cpp
)

set(beamsteeringcwmod_HEADERS
    beamsteeringcwmod.h
    beamsteeringcwmodsettings.h
    beamsteeringcwmodbaseband.h
    beamsteeringcwmodstreamsource.h
    beamsteeringcwmodplugin.h
    beamsteeringcwmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIR}
)

if (NOT SERVER_MODE)
    set(beamsteeringcwmod_SOURCES
        ${beamsteeringcwmod_SOURCES}
        beamsteeringcwmodgui.cpp
        beamsteeringcwmodgui.ui
    )
    set(beamsteeringcwmod_HEADERS
        ${beamsteeringcwmod_HEADERS}
        beamsteeringcwmodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}beamsteeringcwmod)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}beamsteeringcwmodsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${beamsteeringcwmod_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME BeamSteeringCWModPlugin ${beamsteeringcwmod_SOURCES})
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
