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
        adsbdemodnotificationdialog.cpp
        adsbdemodnotificationdialog.ui
        adsbosmtemplateserver.cpp
        adsbdemodmap.qrc
        adsbdemodicons.qrc
        airlinelogos.qrc
        flags.qrc
    )
    set(adsb_HEADERS
        ${adsb_HEADERS}
        adsbdemodgui.h
        adsbdemodfeeddialog.h
        adsbdemoddisplaydialog.h
        adsbdemodnotificationdialog.h
        adsbosmtemplateserver.h
        ourairports.h
        osndb.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodadsb)
    set(TARGET_LIB Qt::Widgets Qt::Quick Qt::QuickWidgets Qt::Positioning)
    if(Qt${QT_DEFAULT_MAJOR_VERSION}Location_FOUND)
        list(APPEND TARGET_LIB Qt::Location)
    endif()
    if(Qt${QT_DEFAULT_MAJOR_VERSION}TextToSpeech_FOUND)
        list(APPEND TARGET_LIB Qt::TextToSpeech)
    endif()
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodadsbsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${adsb_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME ADSBPlugin ${adsb_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if (NOT WIN32)
    link_directories(${Boost_LIBRARY_DIRS})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Boost::disable_autolinking
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if(WIN32)
    # Run deployqt for QtGraphicalEffects, which isn't used in other plugins
    include(DeployQt)
    windeployqt(${TARGET_NAME} ${SDRANGEL_BINARY_BIN_DIR} ${PROJECT_SOURCE_DIR}/map)
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
