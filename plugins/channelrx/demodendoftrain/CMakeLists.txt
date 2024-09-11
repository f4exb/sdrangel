project(demodendoftrain)

set(demodendoftrain_SOURCES
    endoftraindemod.cpp
    endoftraindemodsettings.cpp
    endoftraindemodbaseband.cpp
    endoftraindemodsink.cpp
    endoftraindemodplugin.cpp
    endoftraindemodwebapiadapter.cpp
    endoftrainpacket.cpp
)

set(demodendoftrain_HEADERS
    endoftraindemod.h
    endoftraindemodsettings.h
    endoftraindemodbaseband.h
    endoftraindemodsink.h
    endoftraindemodplugin.h
    endoftraindemodwebapiadapter.h
    endoftrainpacket.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodendoftrain_SOURCES
        ${demodendoftrain_SOURCES}
        endoftraindemodgui.cpp
        endoftraindemodgui.ui
    )
    set(demodendoftrain_HEADERS
        ${demodendoftrain_HEADERS}
        endoftraindemodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodendoftrain)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodendoftrainsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodendoftrain_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME EndOfTrainDemodPlugin ${demodendoftrain_SOURCES})
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
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
