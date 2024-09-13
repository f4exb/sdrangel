project(demodrtty)

set(demodrtty_SOURCES
    rttydemod.cpp
    rttydemodsettings.cpp
    rttydemodbaseband.cpp
    rttydemodsink.cpp
    rttydemodplugin.cpp
    rttydemodwebapiadapter.cpp
)

set(demodrtty_HEADERS
    rttydemod.h
    rttydemodsettings.h
    rttydemodbaseband.h
    rttydemodsink.h
    rttydemodplugin.h
    rttydemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodrtty_SOURCES
        ${demodrtty_SOURCES}
        rttydemodgui.cpp
        rttydemodgui.ui
    )
    set(demodrtty_HEADERS
        ${demodrtty_HEADERS}
        rttydemodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodrtty)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodrttysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodrtty_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RttyDemodPlugin ${demodrtty_SOURCES})
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
