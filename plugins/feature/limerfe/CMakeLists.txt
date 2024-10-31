project(limerfe)

set(limerfe_SOURCES
	limerfe.cpp
    limerfesettings.cpp
    limerfeusbcalib.cpp
    limerfeplugin.cpp
    limerfewebapiadapter.cpp
)

set(limerfe_HEADERS
	limerfe.h
    limerfesettings.h
    limerfeusbcalib.h
    limerfeplugin.h
    limerfewebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIMESUITE_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(limerfe_SOURCES
        ${limerfe_SOURCES}
        limerfegui.cpp
        limerfegui.ui
    )
    set(limerfe_HEADERS
        ${limerfe_HEADERS}
	    limerfegui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featurelimerfe)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featurelimerfesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${limerfe_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME LimeRFEPlugin ${limerfe_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${LIMESUITE_LIBRARY}
)
if(ANDROID)
    target_link_libraries(${TARGET_NAME} PRIVATE ${LIBUSB_LIBRARIES} log)
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
