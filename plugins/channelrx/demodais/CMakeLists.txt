project(demodais)

set(demodais_SOURCES
    aisdemod.cpp
    aisdemodsettings.cpp
    aisdemodbaseband.cpp
    aisdemodsink.cpp
    aisdemodplugin.cpp
    aisdemodwebapiadapter.cpp
)

set(demodais_HEADERS
    aisdemod.h
    aisdemodsettings.h
    aisdemodbaseband.h
    aisdemodsink.h
    aisdemodplugin.h
    aisdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodais_SOURCES
        ${demodais_SOURCES}
        aisdemodgui.cpp
        aisdemodgui.ui
    )
    set(demodais_HEADERS
        ${demodais_HEADERS}
        aisdemodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodais)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodaissrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodais_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AISDemodPlugin ${demodais_SOURCES})
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

