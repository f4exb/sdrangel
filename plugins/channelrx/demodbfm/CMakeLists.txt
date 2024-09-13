project(bfm)

set(bfm_SOURCES
	bfmdemod.cpp
    bfmdemodsettings.cpp
    bfmdemodsink.cpp
    bfmdemodbaseband.cpp
    bfmdemodreport.cpp
    bfmdemodwebapiadapter.cpp
	bfmplugin.cpp
	rdsdemod.cpp
	rdsdecoder.cpp
	rdsparser.cpp
	rdstmc.cpp
)

set(bfm_HEADERS
	bfmdemod.h
    bfmdemodsettings.h
    bfmdemodsink.h
    bfmdemodbaseband.h
    bfmdemodreport.h
    bfmdemodwebapiadapter.h
	bfmplugin.h
	rdsdemod.h
	rdsdecoder.h
	rdsparser.h
	rdstmc.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(bfm_SOURCES
        ${bfm_SOURCES}
        bfmdemodgui.cpp
        bfmdemodgui.ui
    )
    set(bfm_HEADERS
        ${bfm_HEADERS}
        bfmdemodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodbfm)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodbfmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()


if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${bfm_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME BFMPlugin ${bfm_SOURCES})
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
