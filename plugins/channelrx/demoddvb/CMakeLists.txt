project(demoddvb)

set(demoddvb_SOURCES
    dvbdemod.cpp
    dvbdemodsettings.cpp
    dvbdemodbaseband.cpp
    dvbdemodsink.cpp
    dvbdemodplugin.cpp
    dvbdemodtsfifo.cpp
    dvbdemodpsi.cpp
    dvbdemodsections.cpp
    dvbdemodfreesat.cpp
    dvbdemodfreesattables.cpp
    dvbdemodtext.cpp
    dvbdemodepg.cpp
    dvbdemodrecorder.cpp
    dvbdemodudp.cpp
    dvbdemodwebapiadapter.cpp
)

set(demoddvb_HEADERS
    dvbdemod.h
    dvbdemodsettings.h
    dvbdemodbaseband.h
    dvbdemodsink.h
    dvbdemodplugin.h
    dvbdemodtsfifo.h
    dvbdemodpsi.h
    dvbdemodsections.h
    dvbdemodfreesat.h
    dvbdemodtext.h
    dvbdemodepg.h
    dvbdemodrecorder.h
    dvbdemodudp.h
    dvbdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}
)

if(NOT SERVER_MODE)
    # The service is decoded and displayed with FFmpeg. Without a GUI, the transport stream can be sent over UDP
    set(demoddvb_SOURCES
        ${demoddvb_SOURCES}
        dvbdemodgui.cpp
        dvbdemodgui.ui
        dvbdemodvideoplayer.cpp
        dvbdemodscope.cpp
        dvbdemodepgdialog.cpp
        dvbdemodepggrid.cpp
        dvbdemodguideoverlay.cpp
        dvbdemod.qrc
    )
    set(demoddvb_HEADERS
        ${demoddvb_HEADERS}
        dvbdemodgui.h
        dvbdemodvideoplayer.h
        dvbdemodscope.h
        dvbdemodepgdialog.h
        dvbdemodepggrid.h
        dvbdemodguideoverlay.h
    )
    include_directories(
        ${AVCODEC_INCLUDE_DIRS}
        ${AVFORMAT_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWSCALE_INCLUDE_DIRS}
        ${SWRESAMPLE_INCLUDE_DIRS}
    )

    # Deinterlacing uses libavfilter, if available
    if(NOT AVFILTER_LIBRARIES)
        find_library(DVBDEMOD_AVFILTER_LIBRARY NAMES avfilter)
        find_path(DVBDEMOD_AVFILTER_INCLUDE_DIR libavfilter/avfilter.h HINTS ${AVCODEC_INCLUDE_DIRS})
        if(DVBDEMOD_AVFILTER_LIBRARY AND DVBDEMOD_AVFILTER_INCLUDE_DIR)
            set(AVFILTER_LIBRARIES ${DVBDEMOD_AVFILTER_LIBRARY})
            set(AVFILTER_INCLUDE_DIRS ${DVBDEMOD_AVFILTER_INCLUDE_DIR})
        endif()
    endif()
    if(AVFILTER_LIBRARIES)
        include_directories(${AVFILTER_INCLUDE_DIRS})
        add_definitions(-DDVBDEMOD_DEINTERLACE)
    else()
        message(STATUS "demoddvb: libavfilter not found, so no deinterlacing")
    endif()

    set(TARGET_NAME ${PLUGINS_PREFIX}demoddvb)
    set(TARGET_LIB Qt::Widgets ${AVCODEC_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVUTIL_LIBRARIES} ${SWSCALE_LIBRARIES} ${SWRESAMPLE_LIBRARIES} ${AVFILTER_LIBRARIES})
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demoddvbsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demoddvb_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DVBDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demoddvb_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    Qt::Network
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    modemdvb
)

if(WIN32)
    target_link_libraries(${TARGET_NAME} PRIVATE winmm) # The video player sets the timer resolution
endif()

if(APPLE AND NOT SERVER_MODE)
    target_link_libraries(${TARGET_NAME} PRIVATE "-framework CoreVideo") # VideoToolbox frames' IOSurfaces
endif()

if(DEFINED FFMPEG_DEPENDS)
    add_dependencies(${TARGET_NAME} ${FFMPEG_DEPENDS})
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
