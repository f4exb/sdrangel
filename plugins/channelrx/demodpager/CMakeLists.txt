project(demodpager)

set(demodpager_SOURCES
    pagerdemod.cpp
    pagerdemodsettings.cpp
    pagerdemodbaseband.cpp
    pagerdemodsink.cpp
    pagerdemodplugin.cpp
    pagerdemodwebapiadapter.cpp
)

set(demodpager_HEADERS
    pagerdemod.h
    pagerdemodsettings.h
    pagerdemodbaseband.h
    pagerdemodsink.h
    pagerdemodplugin.h
    pagerdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodpager_SOURCES
        ${demodpager_SOURCES}
        pagerdemodgui.cpp
        pagerdemodgui.ui
        pagerdemodcharsetdialog.cpp
        pagerdemodcharsetdialog.ui
        pagerdemodnotificationdialog.cpp
        pagerdemodnotificationdialog.ui
        pagerdemodfilterdialog.cpp
        pagerdemodfilterdialog.ui
        pagerdemodicons.qrc
    )
    set(demodpager_HEADERS
        ${demodpager_HEADERS}
        pagerdemodgui.h
        pagerdemodcharsetdialog.h
        pagerdemodnotificationdialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodpager)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    if(Qt${QT_DEFAULT_MAJOR_VERSION}TextToSpeech_FOUND)
        list(APPEND TARGET_LIB_GUI Qt::TextToSpeech)
    endif()
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodpagersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodpager_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME PagerDemodPlugin ${demodpager_SOURCES})
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
