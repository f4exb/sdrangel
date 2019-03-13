#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui multimedia multimediawidgets widgets opengl qml

TARGET = demoddatv

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MSVC):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(MSVC):INCLUDEPATH += "C:\softs\ffmpeg-20190308-9645147-win64-dev\include"
CONFIG(macx):INCLUDEPATH += "../../../../../boost_1_69_0"

SOURCES += datvdemod.cpp\
    datvdemodgui.cpp\
    datvdemodplugin.cpp\
    datvideostream.cpp \
    datvideorender.cpp

HEADERS += datvdemod.h\
    datvdemodgui.h\
    datvdemodplugin.h\
    leansdr/convolutional.h \
    leansdr/dsp.h \
    leansdr/dvb.h \
    leansdr/filtergen.h \
    leansdr/framework.h \
    leansdr/generic.h \
    leansdr/hdlc.h \
    leansdr/iess.h \
    leansdr/math.h \
    leansdr/rs.h \
    leansdr/sdr.h \
    leansdr/viterbi.h \
    datvconstellation.h \
    datvvideoplayer.h \
    datvideostream.h \
    datvideorender.h

FORMS += datvdemodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
#LIBS += -lavutil -lswscale -lavdevice -lavformat -lavcodec -lswresample
LIBS += -L"C:\softs\ffmpeg-20190308-9645147-win64-dev\lib" -lavutil -lswscale -lavdevice -lavformat -lavcodec -lswresample

RESOURCES = ../../../sdrgui/resources/res.qrc
