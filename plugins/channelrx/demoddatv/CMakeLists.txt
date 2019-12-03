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
    leansdr/dvb.cpp
    leansdr/filtergen.cpp
    leansdr/framework.cpp
    leansdr/math.cpp
    leansdr/sdr.cpp
    datvdemodgui.ui
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
    leansdr/dvb.h
    leansdr/dvbs2.h
    leansdr/filtergen.h
    leansdr/framework.h
    leansdr/math.h
    leansdr/sdr.h
)

include_directories(
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_library(demoddatv SHARED
    ${datv_SOURCES}
)

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

install(TARGETS demoddatv DESTINATION ${INSTALL_PLUGINS_DIR})
