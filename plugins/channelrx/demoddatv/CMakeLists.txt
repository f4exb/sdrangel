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
)

include_directories(
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if (LINUX)
    add_library(demoddatv SHARED
        ${datv_SOURCES}
        ${ldpc_SOURCES}
    )
else()
    add_library(demoddatv SHARED
        ${datv_SOURCES}
    )
endif()

target_link_libraries(demoddatv
    Qt5::Core
    Qt5::Widgets
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    sdrbase
    sdrgui
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
)

if (LINUX)
    add_executable(ldpctool
        ldpctool/ldpc_tool.cpp
        ldpctool/tables_handler.cpp
    )
    install(TARGETS ldpctool DESTINATION ${INSTALL_BIN_DIR})
endif()

install(TARGETS demoddatv DESTINATION ${INSTALL_PLUGINS_DIR})
