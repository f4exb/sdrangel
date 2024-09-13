project(testmi)

set(testmi_SOURCES
	testmi.cpp
	testmiplugin.cpp
	testmiworker.cpp
    testmisettings.cpp
    testmiwebapiadapter.cpp
)

set(testmi_HEADERS
	testmi.h
	testmiplugin.h
	testmiworker.h
    testmisettings.h
    testmiwebapiadapter.h
)

include_directories(
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if (NOT SERVER_MODE)
    set (testmi_SOURCES
        ${testmi_SOURCES}
        testmigui.cpp
        testmigui.ui
    )
    set(testmi_HEADERS
        ${testmi_HEADERS}
        testsourcegui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}mimotestmi)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}mimotestmisrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${testmi_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME TestMIPlugin ${testmi_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
