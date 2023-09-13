project(datv)


set(datv_SOURCES
    datvdemod.cpp
    datvdemodgui.cpp
    datvdemodplugin.cpp
    datvdemodsettings.cpp
    datvdemodwebapiadapter.cpp
    datvideostream.cpp
    datvudpstream.cpp
    datvideorender.cpp
    datvdemodreport.cpp
    datvdemodsink.cpp
    datvdemodbaseband.cpp
    datvdvbs2ldpcdialog.cpp
    leansdr/dvb.cpp
    leansdr/filtergen.cpp
    leansdr/framework.cpp
    leansdr/math.cpp
    leansdr/sdr.cpp
    datvdemodgui.ui
    datvdvbs2ldpcdialog.ui
)

set(ldpc_SOURCES
    ldpctool/tables_handler.cpp
    ldpctool/ldpcworker.cpp
)

set(datv_HEADERS
    datvdemod.h
    datvdemodgui.h
    datvdemodplugin.h
    datvdemodsettings.h
    datvdemodwebapiadapter.h
    datvideostream.h
    datvudpstream.h
    datvideorender.h
    datvconstellation.h
    datvdvbs2constellation.h
    datvdemodreport.h
    datvdemodsink.h
    datvdemodbaseband.h
    datvdvbs2ldpcdialog.h
    leansdr/dvb.h
    leansdr/dvbs2.h
    leansdr/filtergen.h
    leansdr/framework.h
    leansdr/math.h
    leansdr/sdr.h
)

set(ldpc_HEADERS
    ldpctool/ldpc.h
    ldpctool/dvb_s2_tables.h
    ldpctool/dvb_s2x_tables.h
    ldpctool/dvb_t2_tables.h
    ldpctool/ldpcworker.h
)

include_directories(
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

set(TARGET_NAME demoddatv)
set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})

add_library(${TARGET_NAME} SHARED
    ${datv_SOURCES}
    ${ldpc_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    Qt::Widgets
    Qt::Multimedia
    Qt::MultimediaWidgets
    sdrbase
    sdrgui
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
)

add_executable(ldpctool
    ldpctool/ldpc_tool.cpp
    ldpctool/tables_handler.cpp
)
install(TARGETS ldpctool DESTINATION ${INSTALL_BIN_DIR})

if(DEFINED FFMPEG_DEPENDS)
    add_dependencies(${TARGET_NAME} ${FFMPEG_DEPENDS})
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
