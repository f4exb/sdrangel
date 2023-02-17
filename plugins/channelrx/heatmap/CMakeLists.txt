project(heatmap)

set(heatmap_SOURCES
    heatmap.cpp
    heatmapsettings.cpp
    heatmapbaseband.cpp
    heatmapsink.cpp
    heatmapplugin.cpp
    heatmapwebapiadapter.cpp
)

set(heatmap_HEADERS
    heatmap.h
    heatmapsettings.h
    heatmapbaseband.h
    heatmapsink.h
    heatmapplugin.h
    heatmapwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(heatmap_SOURCES
        ${heatmap_SOURCES}
        heatmapgui.cpp
        heatmapgui.ui
    )
    set(heatmap_HEADERS
        ${heatmap_HEADERS}
        heatmapgui.h
    )

    set(TARGET_NAME heatmap)
    set(TARGET_LIB "Qt::Widgets" Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME heatmapsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${heatmap_SOURCES}
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

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
