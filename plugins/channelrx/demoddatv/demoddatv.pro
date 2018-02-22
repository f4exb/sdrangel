#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui multimedia multimediawidgets widgets opengl

TARGET = demoddatv

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(MINGW64):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(macx):INCLUDEPATH += "../../../../../boost_1_64_0"

SOURCES += datvdemod.cpp\
	datvdemodgui.cpp\
	datvdemodplugin.cpp\
        datvscreen.cpp \
    glshaderarray.cpp \
    datvideostream.cpp \
    datvideorender.cpp

HEADERS += datvdemod.h\
	datvdemodgui.h\
	datvdemodplugin.h\
        datvscreen.h \
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
    glshaderarray.h \
    datvvideoplayer.h \
    datvideostream.h \
    datvideorender.h

FORMS += datvdemodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -lavutil -lswscale -lavdevice -lavformat -lavcodec -lswresample

RESOURCES = ../../../sdrbase/resources/res.qrc
