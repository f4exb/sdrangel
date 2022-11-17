project(pertester)

set(pertester_SOURCES
    pertester.cpp
    pertestersettings.cpp
    pertesterplugin.cpp
    pertesterworker.cpp
    pertesterwebapiadapter.cpp
)

set(pertester_HEADERS
    pertester.h
    pertestersettings.h
    pertesterplugin.h
    pertesterreport.h
    pertesterworker.h
    pertesterwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(pertester_SOURCES
        ${pertester_SOURCES}
        pertestergui.cpp
        pertestergui.ui
    )
    set(pertester_HEADERS
        ${pertester_HEADERS}
        pertestergui.h
    )

    set(TARGET_NAME featurepertester)
    set(TARGET_LIB Qt::Widgets)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featurepertestersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${pertester_SOURCES}
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
