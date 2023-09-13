project(moddatv)

set(moddatv_SOURCES
    datvmod.cpp
    datvmodbaseband.cpp
    datvmodreport.cpp
    datvmodsource.cpp
    datvmodplugin.cpp
    datvmodsettings.cpp
    datvmodwebapiadapter.cpp
    dvb-s/dvb-s.cpp
    dvb-s2/DVB2.cpp
    dvb-s2/DVBS2.cpp
    dvb-s2/dvb2_bbheader.cpp
    dvb-s2/dvb2_bch.cpp
    dvb-s2/dvb2_ldpc_encode.cpp
    dvb-s2/dvb2_ldpc_tables.cpp
    dvb-s2/dvb2_scrambler.cpp
    dvb-s2/dvbs2_interleave.cpp
    dvb-s2/dvbs2_modulator.cpp
    dvb-s2/dvbs2_physical.cpp
    dvb-s2/dvbs2_scrambler.cpp
    dvb-s2/dvbs2_tables.cpp
)

set(moddatv_HEADERS
    datvmod.h
    datvmodbaseband.h
    datvmodreport.h
    datvmodsource.h
    datvmodplugin.h
    datvmodsettings.h
    datvmodwebapiadapter.h
    dvb-s/dvb-s.h
    dvb-s/dvb-s.h
    dvb-s2/DVB2.h
    dvb-s2/DVBS2.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(moddatv_SOURCES
        ${moddatv_SOURCES}
        datvmodgui.cpp
        datvmodgui.ui
    )
    set(moddatv_HEADERS
        ${moddatv_HEADERS}
        datvmodgui.h
    )

    set(TARGET_NAME moddatv)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME moddatvsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${moddatv_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Boost::disable_autolinking
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
)

if(DEFINED FFMPEG_DEPENDS)
    add_dependencies(${TARGET_NAME} ${FFMPEG_DEPENDS})
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
