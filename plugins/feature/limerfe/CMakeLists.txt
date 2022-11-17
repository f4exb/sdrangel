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

    set(TARGET_NAME featurelimerfe)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featurelimerfesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${limerfe_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    ${LIMESUITE_LIBRARY}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
