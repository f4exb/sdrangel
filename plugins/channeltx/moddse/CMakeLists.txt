project(moddse)

set(DSDDEMOD_DIR ${CMAKE_SOURCE_DIR}/plugins/channelrx/demoddsd)

set(moddse_SOURCES
    dsemod.cpp
    dsemodbaseband.cpp
    dsemodsource.cpp
    dsemodaudiofile.cpp
    dsemodprocessor.cpp
    dsemodprocessorapi.cpp
    dsemodgatewaymonitor.cpp
    ${DSDDEMOD_DIR}/dsdvocodermbelib.cpp
    dsemodtxsession.cpp
    dsemodpacketbuilder.cpp
    dsemodprotocol.cpp
    dsemodfifo.cpp
    dsemodvocoder.cpp
    dsemodvocodernull.cpp
    dsemodvocoderambefeature.cpp
    dsemodvocodermbelib.cpp
    dsemodvocoderopus.cpp
    dsemodvocodervocos.cpp
    dsemodvocodercodec2.cpp
    dsemodangel61.cpp
    dsemodchannelaccess.cpp
    dsemodlivedtmf.cpp
    dsemodm17.cpp
    dsemoddstar.cpp
    dsemoddmr.cpp
    dsemoddpmr.cpp
    dsemodp25.cpp
    dsemodp25p2.cpp
    dsemodysf.cpp
    dsemodnxdn.cpp
    dsemodshaper.cpp
    dsemodplugin.cpp
    dsemodsettings.cpp
    dsemodwebapiadapter.cpp
)

set(moddse_HEADERS
    dsemodchannelaccess.h
    dsemod.h
    dsemodbaseband.h
    dsemodsource.h
    dsemodaudiofile.h
    dsemodprocessor.h
    dsemodgatewaymonitor.h
    dsemodtxsession.h
    dsemodpacketbuilder.h
    dsemodprotocol.h
    dsemodfifo.h
    dsemodvocoder.h
    dsemodvocodernull.h
    dsemodvocoderambefeature.h
    dsemodvocodermbelib.h
    dsemodvocoderopus.h
    dsemodvocodervocos.h
    dsemodvocodercodec2.h
    dsemodangel61.h
    dsemodlivedtmf.h
    dsemodangel61data.h
    dsemodm17.h
    dsemodprotocol.h
    dsemoddstar.h
    dsemoddmr.h
    dsemoddpmr.h
    dsemodp25.h
    dsemodp25p2.h
    dsemodysf.h
    dsemodnxdn.h
    dsemodshaper.h
    dsemodplugin.h
    dsemodsettings.h
    dsemodwebapiadapter.h
)

include_directories(
    ${CODEC2_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(moddse_SOURCES
        ${moddse_SOURCES}
        dsemodgui.cpp
        dsemodaprssymboldialog.cpp
        dsemodgui.ui
    )
    set(moddse_HEADERS
        ${moddse_HEADERS}
        dsemodgui.h
        dsemodaprssymboldialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}moddse)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}moddsesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${moddse_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DSEModPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${moddse_SOURCES})
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
    ${CODEC2_LIBRARIES}
)
target_include_directories(${TARGET_NAME} PRIVATE ${DSDDEMOD_DIR})

if(CODEC2_EXTERNAL)
    add_dependencies(${TARGET_NAME} codec2)
endif()

if(MBEVOCODER_FOUND)
    if(MBEVOCODER_INCLUDE_DIRS)
        target_include_directories(${TARGET_NAME} BEFORE PRIVATE ${MBEVOCODER_INCLUDE_DIRS})
    endif()
    target_compile_definitions(${TARGET_NAME} PRIVATE DSEMOD_HAS_MBELIB DSDDEMOD_HAS_MBELIB MBE_STATIC)
    target_link_libraries(${TARGET_NAME} PRIVATE ${MBEVOCODER_LIBRARY})
    if(MBEVOCODER_EXTERNAL)
        add_dependencies(${TARGET_NAME} mbevocoder)
    endif()
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
