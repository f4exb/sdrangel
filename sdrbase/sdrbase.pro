#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core multimedia
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = lib
TARGET = sdrbase
INCLUDEPATH += $$PWD
INCLUDEPATH += ../exports
INCLUDEPATH += ../httpserver
INCLUDEPATH += ../qrtplib
INCLUDEPATH += ../swagger/sdrangel/code/qt5/client

DEFINES += USE_KISSFFT=1
win32 {
    DEFINES += __WINDOWS__=1
    DEFINES += DSD_USE_SERIALDV=1
}
DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

QMAKE_CXXFLAGS += -std=c++11

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(ANDROID):INCLUDEPATH += /opt/softs/boost_1_60_0

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\boost_1_66_0"

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\serialDV"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\serialDV"

CONFIG(macx):INCLUDEPATH += "../../../boost_1_64_0"

win32 {
    HEADERS += \
        dsp/dvserialengine.h \
        dsp/dvserialworker.h
    SOURCES += \
        dsp/dvserialengine.cpp \
        dsp/dvserialworker.cpp
}

SOURCES += audio/audiodevicemanager.cpp\
        audio/audiocompressor.cpp\
        audio/audiofifo.cpp\
        audio/audiooutput.cpp\
        audio/audioinput.cpp\
        audio/audionetsink.cpp\
        channel/channelsinkapi.cpp\
        channel/channelsourceapi.cpp\
        commands/command.cpp\
        device/devicesourceapi.cpp\
        device/devicesinkapi.cpp\
        device/deviceenumerator.cpp\
        dsp/afsquelch.cpp\
        dsp/agc.cpp\
        dsp/downchannelizer.cpp\
        dsp/upchannelizer.cpp\
        dsp/channelmarker.cpp\
        dsp/ctcssdetector.cpp\
        dsp/cwkeyer.cpp\
        dsp/cwkeyersettings.cpp\
        dsp/decimatorsfi.cpp\
        dsp/dspcommands.cpp\
        dsp/dspengine.cpp\
        dsp/dspdevicesourceengine.cpp\
        dsp/dspdevicesinkengine.cpp\
        dsp/fftengine.cpp\
        dsp/kissengine.cpp\
        dsp/fftcorr.cpp\
        dsp/fftfilt.cpp\
        dsp/fftwindow.cpp\
        dsp/filterrc.cpp\
        dsp/filtermbe.cpp\
        dsp/filerecord.cpp\
        dsp/freqlockcomplex.cpp\
        dsp/interpolator.cpp\
        dsp/hbfiltertraits.cpp\
        dsp/lowpass.cpp\
        dsp/nco.cpp\
        dsp/ncof.cpp\
        dsp/phaselock.cpp\
        dsp/phaselockcomplex.cpp\
        dsp/projector.cpp\
        dsp/recursivefilters.cpp\
        dsp/samplesinkfifo.cpp\
        dsp/samplesourcefifo.cpp\
        dsp/samplesinkfifodoublebuffered.cpp\
        dsp/basebandsamplesink.cpp\
        dsp/basebandsamplesource.cpp\
        dsp/nullsink.cpp\
        dsp/threadedbasebandsamplesink.cpp\
        dsp/threadedbasebandsamplesource.cpp\
        dsp/wfir.cpp\
        dsp/devicesamplesource.cpp\
        dsp/devicesamplesink.cpp\
        settings/preferences.cpp\
        settings/preset.cpp\
        settings/mainsettings.cpp\
        util/CRC64.cpp\
        util/db.cpp\
        util/message.cpp\
        util/messagequeue.cpp\
        util/prettyprint.cpp\
        util/rtpsink.cpp\
        util/syncmessenger.cpp\
        util/samplesourceserializer.cpp\
        util/simpleserializer.cpp\
        util/uid.cpp\
        plugin/plugininterface.cpp\
        plugin/pluginapi.cpp\        
        plugin/pluginmanager.cpp\
        webapi/webapiadapterinterface.cpp\
        webapi/webapirequestmapper.cpp\
        webapi/webapiserver.cpp\
        mainparser.cpp

HEADERS  += audio/audiodevicemanager.h\
        audio/audiocompressor.h\
        audio/audiofifo.h\
        audio/audiooutput.h\
        audio/audioinput.h\
        audio/audionetsink.h\
        channel/channelsinkapi.h\
        channel/channelsourceapi.h\ 
        commands/command.h\       
        device/devicesourceapi.h\
        device/devicesinkapi.h\
        device/deviceenumerator.h\
        dsp/afsquelch.h\
        dsp/decimatorsfi.h\
        dsp/downchannelizer.h\
        dsp/upchannelizer.h\
        dsp/channelmarker.h\
        dsp/cwkeyer.h\
        dsp/cwkeyersettings.h\
        dsp/complex.h\
        dsp/decimators.h\
        dsp/interpolators.h\
        dsp/dspcommands.h\
        dsp/dspengine.h\
        dsp/dspdevicesourceengine.h\
        dsp/dspdevicesinkengine.h\
        dsp/dsptypes.h\
        dsp/fftcorr.h\
        dsp/fftengine.h\
        dsp/fftfilt.h\
        dsp/fftwengine.h\
        dsp/fftwindow.h\
        dsp/filterrc.h\
        dsp/filtermbe.h\
        dsp/filerecord.h\
        dsp/freqlockcomplex.h\
        dsp/gfft.h\
        dsp/hbfiltertraits.h\
        dsp/iirfilter.h\
        dsp/interpolator.h\
        dsp/inthalfbandfilter.h\
        dsp/inthalfbandfilterdb.h\
        dsp/inthalfbandfiltereo1.h\
        dsp/inthalfbandfiltereo1i.h\
        dsp/inthalfbandfilterst.h\
        dsp/inthalfbandfiltersti.h\
        dsp/kissfft.h\
        dsp/kissengine.h\
        dsp/lowpass.h\
        dsp/misc.h\
        dsp/movingaverage.h\
        dsp/nco.h\
        dsp/ncof.h\
        dsp/phasediscri.h\
        dsp/phaselock.h\
        dsp/phaselockcomplex.h\
        dsp/projector.h\
        dsp/recursivefilters.h\
        dsp/samplesinkfifo.h\
        dsp/samplesourcefifo.h\
        dsp/samplesinkfifodoublebuffered.h\
        dsp/samplesinkfifodecimator.h\
        dsp/basebandsamplesink.h\
        dsp/basebandsamplesource.h\
        dsp/nullsink.h\
        dsp/threadedbasebandsamplesink.h\
        dsp/threadedbasebandsamplesource.h\
        dsp/wfir.h\
        dsp/devicesamplesource.h\
        dsp/devicesamplesink.h\
        plugin/plugininstancegui.h\
        plugin/plugininterface.h\   
        plugin/pluginapi.h\   
        plugin/pluginmanager.h\   
        settings/preferences.h\
        settings/preset.h\
        settings/mainsettings.h\
        util/CRC64.h\
        util/db.h\
        util/message.h\
        util/messagequeue.h\
        util/prettyprint.h\
        util/rtpsink.h\
        util/syncmessenger.h\
        util/samplesourceserializer.h\
        util/simpleserializer.h\
        util/uid.h\
        webapi/webapiadapterinterface.h\
        webapi/webapirequestmapper.h\
        webapi/webapiserver.h\
        mainparser.h

!macx:LIBS += -L../serialdv/$${build_subdir} -lserialdv
LIBS += -L../httpserver/$${build_subdir} -lhttpserver
LIBS += -L../qrtplib/$${build_subdir} -lqrtplib
LIBS += -L../swagger/$${build_subdir} -lswagger

RCC_BINARY_SOURCES += resources/res.qrc

asset_builder.commands = $$[QT_HOST_BINS]/rcc -binary ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT} -no-compress
asset_builder.depend_command = $$[QT_HOST_BINS]/rcc -list $$QMAKE_RESOURCE_FLAGS ${QMAKE_FILE_IN}
asset_builder.input = RCC_BINARY_SOURCES
asset_builder.output = $$OUT_PWD/$$DESTDIR/${QMAKE_FILE_IN_BASE}.qrb
asset_builder.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += asset_builder

OTHER_FILES += $$RCC_BINARY_SOURCES

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
