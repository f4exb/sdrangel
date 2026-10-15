project(demodselcal)

set(demodselcal_SOURCES
    selcaldemod.cpp
    selcaldemodsettings.cpp
    selcaldemodbaseband.cpp
    selcaldemodsink.cpp
    selcalcarriertracker.cpp
    selcaldecoder.cpp
    selcalmessages.cpp
    selcaldemodplugin.cpp
    selcaldemodwebapiadapter.cpp
)

set(demodselcal_HEADERS
    selcaldemod.h
    selcaldemodsettings.h
    selcaldemodbaseband.h
    selcaldemodsink.h
    selcalcarriertracker.h
    selcaldecoder.h
    selcalmessages.h
    selcaldemodplugin.h
    selcaldemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodselcal_SOURCES
        ${demodselcal_SOURCES}
        selcaldemodgui.cpp
        selcaldemodgui.ui
        )
    set(demodselcal_HEADERS
        ${demodselcal_HEADERS}
        selcaldemodgui.h
        )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodselcal)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodselcalsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodselcal_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME SELCALDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demodselcal_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
