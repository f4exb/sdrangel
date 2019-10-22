project(beamsteeringcwsource)

set(beamsteeringcwsource_SOURCES
    beamsteeringcwsourceplugin.cpp
    beamsteeringcwsource.cpp
    beamsteeringcwsourcesettings.cpp
    beamsteeringcwsourceworker.cpp
    beamsteeringcwsourcewebapiadapter.cpp
)

set(beamsteeringcwsource_HEADERS
    beamsteeringcwsourceplugin.h
    beamsteeringcwsource.h
    beamsteeringcwsourcesettings.h
    beamsteeringcwsourceworker.h
    beamsteeringcwsourcewebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIR}
)

if (NOT SERVER_MODE)
    set(beamsteeringcwsource_SOURCES
        ${beamsteeringcwsource_SOURCES}
        beamsteeringcwsourcegui.cpp
        beamsteeringcwsourcegui.ui
    )
    set(beamsteeringcwsource_HEADERS
        ${beamsteeringcwsource_HEADERS}
        beamsteeringcwsourcegui.h
    )

    set(TARGET_NAME beamsteeringcwsource)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME beamsteeringcwsourcesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${beamsteeringcwsource_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt5::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
