project(vorsc)

set(vorsc_SOURCES
    vordemodsc.cpp
    vordemodscsettings.cpp
    vordemodscbaseband.cpp
    vordemodscsink.cpp
    vordemodscplugin.cpp
    vordemodscwebapiadapter.cpp
    vordemodscreport.cpp
)

set(vorsc_HEADERS
    vordemodsc.h
    vordemodscsettings.h
    vordemodscbaseband.h
    vordemodscsink.h
    vordemodscplugin.h
    vordemodscwebapiadapter.h
    vordemodscreport.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(vorsc_SOURCES
        ${vorsc_SOURCES}
        vordemodscgui.cpp
        vordemodscgui.ui
    )
    set(vorsc_HEADERS
        ${vorsc_HEADERS}
        vordemodscgui.h
    )

    set(TARGET_NAME demodvorsc)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodvorscsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${vorsc_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt5::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
