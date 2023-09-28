project(freqscanner)

set(freqscanner_SOURCES
    freqscanner.cpp
    freqscannersettings.cpp
    freqscannerbaseband.cpp
    freqscannersink.cpp
    freqscannerplugin.cpp
    freqscannerwebapiadapter.cpp
)

set(freqscanner_HEADERS
    freqscanner.h
    freqscannersettings.h
    freqscannerbaseband.h
    freqscannersink.h
    freqscannerplugin.h
    freqscannerwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(freqscanner_SOURCES
        ${freqscanner_SOURCES}
        freqscannergui.cpp
        freqscannergui.ui
        freqscanneraddrangedialog.cpp
        freqscanneraddrangedialog.ui
    )
    set(freqscanner_HEADERS
        ${freqscanner_HEADERS}
        freqscannergui.h
        freqscanneraddrangedialog.h
    )

    set(TARGET_NAME freqscanner)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME freqscannersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${freqscanner_SOURCES}
)

target_link_libraries(${TARGET_NAME}
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
