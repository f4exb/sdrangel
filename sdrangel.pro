#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT       += core gui multimedia opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = sdrangel
TEMPLATE = app
INCLUDEPATH += $$PWD/include

DEFINES += USE_KISSFFT=1

CONFIG(ANDROID):INCLUDEPATH += /opt/build/Boost-for-Android-Prebuilt/boost_1_53_0/include
CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_55_0"

SOURCES += main.cpp\
        sdrbase/mainwindow.cpp\
        sdrbase/audio/audiodeviceinfo.cpp\
        sdrbase/audio/audiofifo.cpp\
        sdrbase/audio/audiooutput.cpp\
        sdrbase/dsp/afsquelch.cpp\
        sdrbase/dsp/agc.cpp\
        sdrbase/dsp/channelizer.cpp\
        sdrbase/dsp/channelmarker.cpp\
        sdrbase/dsp/ctcssdetector.cpp\
        sdrbase/dsp/dspcommands.cpp\
        sdrbase/dsp/dspengine.cpp\
        sdrbase/dsp/dspdeviceengine.cpp\
        sdrbase/dsp/fftengine.cpp\
        sdrbase/dsp/kissengine.cpp\
        sdrbase/dsp/fftfilt.cxx\
        sdrbase/dsp/fftwindow.cpp\
        sdrbase/dsp/filterrc.cpp\
        sdrbase/dsp/filesink.cpp\
        sdrbase/dsp/interpolator.cpp\
        sdrbase/dsp/inthalfbandfilter.cpp\
        sdrbase/dsp/lowpass.cpp\
        sdrbase/dsp/movingaverage.cpp\
        sdrbase/dsp/nco.cpp\
        sdrbase/dsp/pidcontroller.cpp\
        sdrbase/dsp/phaselock.cpp\
        sdrbase/dsp/samplefifo.cpp\
        sdrbase/dsp/samplesink.cpp\
        sdrbase/dsp/nullsink.cpp\
        sdrbase/dsp/spectrumscopecombovis.cpp\
        sdrbase/dsp/scopevis.cpp\
        sdrbase/dsp/spectrumvis.cpp\
        sdrbase/dsp/threadedsamplesink.cpp\
        sdrbase/gui/aboutdialog.cpp\
        sdrbase/gui/addpresetdialog.cpp\
        sdrbase/gui/basicchannelsettingswidget.cpp\
        sdrbase/gui/buttonswitch.cpp\
        sdrbase/gui/channelwindow.cpp\
        sdrbase/gui/colormapper.cpp\
        sdrbase/gui/glscope.cpp\
        sdrbase/gui/glscopegui.cpp\
        sdrbase/gui/glshadersimple.cpp\
        sdrbase/gui/glshadertextured.cpp\        
        sdrbase/gui/glspectrum.cpp\
        sdrbase/gui/glspectrumgui.cpp\
        sdrbase/gui/indicator.cpp\
        sdrbase/gui/pluginsdialog.cpp\
        sdrbase/gui/preferencesdialog.cpp\
        sdrbase/gui/presetitem.cpp\
        sdrbase/gui/rollupwidget.cpp\
        sdrbase/gui/scale.cpp\
        sdrbase/gui/scaleengine.cpp\
        sdrbase/gui/valuedial.cpp\
        sdrbase/dsp/samplesource.cpp\
        sdrbase/plugin/pluginapi.cpp\
        sdrbase/plugin/plugininterface.cpp\
        sdrbase/plugin/pluginmanager.cpp\
        sdrbase/settings/preferences.cpp\
        sdrbase/settings/preset.cpp\
        sdrbase/settings/mainsettings.cpp\
        sdrbase/util/CRC64.cpp\
        sdrbase/util/db.cpp\
        sdrbase/util/message.cpp\
        sdrbase/util/messagequeue.cpp\
        sdrbase/util/prettyprint.cpp\
        sdrbase/util/syncmessenger.cpp\
        sdrbase/util/samplesourceserializer.cpp\
        sdrbase/util/simpleserializer.cpp

HEADERS  += include/mainwindow.h\
        include/audio/audiodeviceinfo.h\
        include/audio/audiofifo.h\
        include/audio/audiooutput.h\
        include/dsp/afsquelch.h\
        include/dsp/channelizer.h\
        include/dsp/channelmarker.h\
        include/dsp/complex.h\
        include/dsp/decimators.h\
        include/dsp/dspcommands.h\
        include/dsp/dspengine.h\
        include/dsp/dspdeviceengine.h\
        include/dsp/dsptypes.h\
        include/dsp/fftengine.h\
        include/dsp/fftfilt.h\
        include/dsp/fftwengine.h\
        include/dsp/fftwindow.h\
        include/dsp/filterrc.h\
        include/dsp/filesink.h\
        include/dsp/gfft.h\
        include/dsp/interpolator.h\
        include/dsp/inthalfbandfilter.h\
        include/dsp/kissfft.h\
        include/dsp/kissengine.h\
        include/dsp/lowpass.h\
        include/dsp/misc.h\
        include/dsp/movingaverage.h\
        include/dsp/nco.h\
        include/dsp/phasediscri.h\
        include/dsp/phaselock.h\
        sdrbase/dsp/pidcontroller.h\
        include/dsp/samplefifo.h\
        include/dsp/samplesink.h\
        include/dsp/nullsink.h\
        include/dsp/scopevis.h\
        include/dsp/spectrumvis.h\
        include/dsp/threadedsamplesink.h\
        include/gui/aboutdialog.h\
        include/gui/addpresetdialog.h\
        include/gui/basicchannelsettingswidget.h\
        include/gui/buttonswitch.h\
        include/gui/channelwindow.h\
        include/gui/colormapper.h\
        include/gui/glscope.h\
        include/gui/glscopegui.h\
        include/gui/glshadersimple.h\
        include/gui/glshadertextured.h\        
        include/gui/glspectrum.h\
        include/gui/glspectrumgui.h\
        include/gui/indicator.h\
        include/gui/physicalunit.h\
        include/gui/pluginsdialog.h\
        include/gui/preferencesdialog.h\
        include/gui/presetitem.h\
        include/gui/rollupwidget.h\
        include/gui/scale.h\
        include/gui/scaleengine.h\
        include/gui/valuedial.h\
        include/dsp/samplesource.h\
        include/plugin/pluginapi.h\
        include/plugin/plugingui.h\
        include/plugin/plugininterface.h\
        include/plugin/pluginmanager.h\
        include/settings/preferences.h\
        include/settings/preset.h\
        include/settings/mainsettings.h\
        include/util/CRC64.h\
        include/util/db.h\
        include/util/export.h\
        include/util/message.h\
        include/util/messagequeue.h\
        include/util/prettyprint.h\
        include/util/syncmessenger.h\
        include/util/samplesourceserializer.h\
        include/util/simpleserializer.h

FORMS    += sdrbase/mainwindow.ui\
        sdrbase/gui/scopewindow.ui\
        sdrbase/gui/addpresetdialog.ui\
        sdrbase/gui/basicchannelsettingswidget.ui\
        sdrbase/gui/preferencesdialog.ui\
        sdrbase/gui/glscopegui.ui\
        sdrbase/gui/aboutdialog.ui\
        sdrbase/gui/pluginsdialog.ui\
        sdrbase/gui/glspectrumgui.ui\
        sdrbase/mainwindow.ui\
        plugins/samplesource/sdrdaemon/sdrdaemongui.ui\
        plugins/samplesource/filesource/filesourcegui.ui\
        plugins/channel/nfm/nfmdemodgui.ui\
        plugins/channel/tcpsrc/tcpsrcgui.ui\
        plugins/channel/chanalyzer/chanalyzergui.ui\
        plugins/channel/udpsrc/udpsrcgui.ui\
        plugins/channel/lora/lorademodgui.ui\
        plugins/channel/wfm/wfmdemodgui.ui\
        plugins/channel/ssb/ssbdemodgui.ui\
        plugins/channel/bfm/bfmdemodgui.ui\
        plugins/channel/am/amdemodgui.ui

RESOURCES = sdrbase/resources/res.qrc

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
