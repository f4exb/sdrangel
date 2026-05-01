project(geoscandemod)

set(geoscandemod_SOURCES
    geoscandemod.cpp
    geoscanplugin.cpp
    geoscanGFSK.cpp
    geoscanCorrelator.cpp
)

set(geoscandemod_HEADERS
    geoscandemod.h
    geoscanplugin.h
    geoscanGFSK.h
    geoscanCorrelator.h
    geoscanDCBlocker.h
    geoscanLowPassFilter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/exports
    ${CMAKE_SOURCE_DIR}/sdrbase
    ${CMAKE_SOURCE_DIR}/sdrgui
)

if(NOT SERVER_MODE)
    set(TARGET_NAME ${PLUGINS_PREFIX}demodgeoscan)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodgeoscansrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${geoscandemod_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME GeoscanPlugin ${geoscandemod_SOURCES})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})