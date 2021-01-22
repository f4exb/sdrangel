project(adsb)

set(adsb_SOURCES
    adsbdemod.cpp
    adsbdemodsettings.cpp
    adsbdemodwebapiadapter.cpp
    adsbplugin.cpp
    adsbdemodsink.cpp
    adsbdemodsinkworker.cpp
    adsbdemodbaseband.cpp
    adsbdemodreport.cpp
    adsbdemodworker.cpp
)

set(adsb_HEADERS
    adsbdemod.h
    adsbdemodsettings.h
    adsbdemodwebapiadapter.h
    adsbplugin.h
    adsbdemodsink.h
    absddemodsinkworker.h
    adsbdemodbaseband.h
    adsbdemodreport.h
    adsbdemodworker.h
    adsbdemodstats.h
    adsb.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(adsb_SOURCES
        ${adsb_SOURCES}
        adsbdemodgui.cpp
        adsbdemodgui.ui
        adsbdemodfeeddialog.cpp
        adsbdemodfeeddialog.ui
        adsbdemoddisplaydialog.cpp
        adsbdemoddisplaydialog.ui
        csv.cpp
        airlinelogos.qrc
        flags.qrc
        map.qrc
        icons.qrc
    )
    set(adsb_HEADERS
        ${adsb_HEADERS}
        adsbdemodgui.h
        adsbdemodfeeddialog.h
        adsbdemoddisplaydialog.h
        ourairports.h
        osndb.h
        csv.h
    )

    set(TARGET_NAME demodadsb)
    set(TARGET_LIB Qt5::Widgets Qt5::Quick Qt5::QuickWidgets Qt5::Positioning)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodadsbsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${adsb_SOURCES}
)

if (NOT WIN32)
    link_directories(${Boost_LIBRARY_DIRS})
endif()

target_link_libraries(${TARGET_NAME}
    Boost::disable_autolinking
    Qt5::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

