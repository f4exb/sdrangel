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

    set(TARGET_NAME beamsteeringcwmod)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME beamsteeringcwmodsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${beamsteeringcwmod_SOURCES}
)

target_link_libraries(${TARGET_NAME}
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
