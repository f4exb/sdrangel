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

    set(TARGET_NAME ${PLUGINS_PREFIX}featurepertester)
    set(TARGET_LIB Qt::Widgets)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featurepertestersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${pertester_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME PERTesterPlugin ${pertester_SOURCES})
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
