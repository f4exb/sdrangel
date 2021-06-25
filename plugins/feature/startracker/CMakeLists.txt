project(startracker)

set(startracker_SOURCES
    startracker.cpp
    startrackersettings.cpp
    startrackerplugin.cpp
    startrackerworker.cpp
    startrackerwebapiadapter.cpp
)

set(startracker_HEADERS
    startracker.h
    startrackersettings.h
    startrackerplugin.h
    startrackerreport.h
    startrackerworker.h
    startrackerwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(startracker_SOURCES
        ${startracker_SOURCES}
        startrackergui.cpp
        startrackergui.ui
        startrackersettingsdialog.cpp
        startrackersettingsdialog.ui
        startracker.qrc
        startracker150mhz.qrc
        startracker408mhz.qrc
        startracker1420mhz.qrc
        startrackermilkyway.qrc
    )
    set(startracker_HEADERS
        ${startracker_HEADERS}
        startrackergui.h
        startrackersettingsdialog.h
    )

    set(TARGET_NAME featurestartracker)
    set(TARGET_LIB Qt5::Widgets Qt5::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featurestartrackersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${startracker_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt5::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
