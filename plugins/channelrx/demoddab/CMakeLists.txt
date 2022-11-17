project(demoddab)

add_definitions(-DDABLIN_AAC_FAAD2)

set(demoddab_SOURCES
    dabdemod.cpp
    dabdemodsettings.cpp
    dabdemodbaseband.cpp
    dabdemodsink.cpp
    dabdemodplugin.cpp
    dabdemodwebapiadapter.cpp
    dabdemoddevice.cpp
)

set(demoddab_HEADERS
    dabdemod.h
    dabdemodsettings.h
    dabdemodbaseband.h
    dabdemodsink.h
    dabdemodplugin.h
    dabdemodwebapiadapter.h
    dabdemoddevice.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${DAB_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(demoddab_SOURCES
        ${demoddab_SOURCES}
        dabdemodgui.cpp
        dabdemodgui.ui
    )
    set(demoddab_HEADERS
        ${demoddab_HEADERS}
        dabdemodgui.h
    )

    set(TARGET_NAME demoddab)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demoddabsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demoddab_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    ${DAB_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
