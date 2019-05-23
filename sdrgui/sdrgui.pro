#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core gui multimedia opengl
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = lib
TARGET = sdrgui
INCLUDEPATH += $$PWD
INCLUDEPATH += ../exports
INCLUDEPATH += ../sdrbase
INCLUDEPATH += ../logging
INCLUDEPATH += ../httpserver
INCLUDEPATH += ../swagger/sdrangel/code/qt5/client

DEFINES += USE_KISSFFT=1
win32 {
    DEFINES += __WINDOWS__=1
}
DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

QMAKE_CXXFLAGS += -std=c++11

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MSVC):DEFINES += sdrgui_EXPORTS

CONFIG(ANDROID):INCLUDEPATH += /opt/softs/boost_1_60_0

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(MSVC):INCLUDEPATH += "C:\softs\boost_1_66_0"

CONFIG(macx):INCLUDEPATH += "../../../boost_1_69_0"

SOURCES += mainwindow.cpp\
        device/deviceuiset.cpp\
        dsp/spectrumscopecombovis.cpp\
        dsp/scopevis.cpp\
        dsp/scopevisxy.cpp\
        dsp/spectrumvis.cpp\
        gui/aboutdialog.cpp\
        gui/addpresetdialog.cpp\
        gui/basicchannelsettingsdialog.cpp\
        gui/basicdevicesettingsdialog.cpp\
        gui/buttonswitch.cpp\
        gui/channelwindow.cpp\
        gui/clickablelabel.cpp\
        gui/colormapper.cpp\
        gui/commanditem.cpp\
        gui/commandkeyreceiver.cpp\
        gui/commandoutputdialog.cpp\
        gui/crightclickenabler.cpp\
        gui/cwkeyergui.cpp\
        gui/devicestreamselectiondialog.cpp\
        gui/editcommanddialog.cpp\
        gui/externalclockbutton.cpp\
        gui/externalclockdialog.cpp\
        gui/glscope.cpp\
        gui/glscopegui.cpp\
        gui/glshadersimple.cpp\
        gui/glshadertextured.cpp\
        gui/glshadertvarray.cpp\
        gui/glspectrum.cpp\
        gui/glspectrumgui.cpp\
        gui/indicator.cpp\
        gui/levelmeter.cpp\
        gui/loggingdialog.cpp\
        gui/pluginsdialog.cpp\
        gui/audiodialog.cpp\
        gui/audioselectdialog.cpp\
        gui/presetitem.cpp\
        gui/rollupwidget.cpp\
        gui/samplingdevicecontrol.cpp\
        gui/samplingdevicedialog.cpp\
        gui/sdrangelsplash.cpp\
        gui/mypositiondialog.cpp\
        gui/scaleengine.cpp\
        gui/transverterbutton.cpp\
        gui/transverterdialog.cpp\
        gui/tickedslider.cpp\
        gui/tvscreen.cpp\
        gui/valuedial.cpp\
        gui/valuedialz.cpp\
        soapygui/arginfogui.cpp\
        soapygui/complexfactorgui.cpp\
        soapygui/discreterangegui.cpp\
        soapygui/dynamicargsettinggui.cpp\
        soapygui/dynamicitemsettinggui.cpp\
        soapygui/intervalrangegui.cpp\
        soapygui/intervalslidergui.cpp\
        soapygui/itemsettinggui.cpp\
        soapygui/stringrangegui.cpp\
        webapi/webapiadaptergui.cpp

HEADERS  += mainwindow.h\
        device/deviceuiset.h\
        dsp/spectrumscopecombovis.h\
        dsp/scopevis.h\
        dsp/scopevisxy.h\
        dsp/spectrumvis.h\
        gui/aboutdialog.h\
        gui/addpresetdialog.h\
        gui/audiodialog.h\
        gui/audioselectdialog.h\
        gui/basicchannelsettingsdialog.h\
        gui/basicdevicesettingsdialog.h\
        gui/buttonswitch.h\
        gui/channelwindow.h\
        gui/clickablelabel.h\
        gui/colormapper.h\
        gui/commanditem.h\
        gui/commandkeyreceiver.h\
        gui/commandoutputdialog.h\
        gui/crightclickenabler.h\
        gui/cwkeyergui.h\
        gui/devicestreamselectiondialog.h\
        gui/editcommanddialog.h\
        gui/externalclockbutton.h\
        gui/externalclockdialog.h\
        gui/glscope.h\
        gui/glscopegui.h\
        gui/glshadersimple.h\
        gui/glshadertextured.h\
        gui/glshadertvarray.h\
        gui/glspectrum.h\
        gui/glspectrumgui.h\
        gui/indicator.h\
        gui/levelmeter.h\
        gui/loggingdialog.h\
        gui/physicalunit.h\
        gui/pluginsdialog.h\
        gui/presetitem.h\
        gui/rollupwidget.h\
        gui/samplingdevicecontrol.h\
        gui/samplingdevicedialog.h\
        gui/sdrangelsplash.h\
        gui/mypositiondialog.h\
        gui/scaleengine.h\
        gui/tickedslider.h\
        gui/transverterbutton.h\
        gui/transverterdialog.h\
        gui/tvscreen.h\
        gui/valuedial.h\
        gui/valuedialz.h\
        soapygui/arginfogui.h\
        soapygui/complexfactorgui.h\
        soapygui/discreterangegui.h\
        soapygui/dynamicargsettinggui.h\
        soapygui/dynamicitemsettinggui.h\
        soapygui/intervalrangegui.h\
        soapygui/intervalslidergui.h\
        soapygui/itemsettinggui.h\
        soapygui/stringrangegui.h\
        webapi/webapiadaptergui.h

FORMS    += mainwindow.ui\
        gui/scopewindow.ui\
        gui/addpresetdialog.ui\
        gui/basicchannelsettingsdialog.ui\
        gui/basicdevicesettingsdialog.ui\
        gui/commandoutputdialog.ui\
        gui/cwkeyergui.ui\
        gui/editcommanddialog.ui\
        gui/externalclockdialog.ui\
        gui/audiodialog.ui\
        gui/audioselectdialog.ui\
        gui/glscopegui.ui\
        gui/aboutdialog.ui\
        gui/pluginsdialog.ui\
        gui/samplingdevicecontrol.ui\
        gui/samplingdevicedialog.ui\
        gui/myposdialog.ui\
        gui/loggingdialog.ui\
        gui/glspectrumgui.ui\
        gui/transverterdialog.ui\
        soapygui/arginfogui.ui\
        soapygui/complexfactorgui.ui\
        soapygui/discreterangegui.ui\
        soapygui/intervalrangegui.ui\
        soapygui/intervalslidergui.ui\
        mainwindow.ui

LIBS += -L../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../logging/$${build_subdir} -llogging
LIBS += -L../swagger/$${build_subdir} -lswagger

macx {
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

RESOURCES = resources/res.qrc

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
