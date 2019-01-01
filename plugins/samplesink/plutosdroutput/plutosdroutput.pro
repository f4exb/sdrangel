#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = outputplutosdr

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11
macx:QMAKE_LFLAGS += -F/Library/Frameworks

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
INCLUDEPATH += ../../../devices

MINGW32 {
    LIBIIOSRC = "C:\softs\libiio"
    INCLUDEPATH += ../../../libiio/includemw
    INCLUDEPATH += $$LIBIIOSRC
}

MSVC {
    INCLUDEPATH += "C:\Program Files\PothosSDR\include"
}

macx {
    INCLUDEPATH += "/Library/Frameworks/iio.framework/Headers/"
}

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += plutosdroutputgui.cpp\
  plutosdroutput.cpp\
  plutosdroutputplugin.cpp\
  plutosdroutputsettings.cpp\
  plutosdroutputthread.cpp

HEADERS += plutosdroutputgui.h\
  plutosdroutput.h\
  plutosdroutputplugin.h\
  plutosdroutputsettings.h\
  plutosdroutputthread.h

FORMS += plutosdroutputgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
LIBS += -L../../../devices/$${build_subdir} -ldevices

MINGW32 {
    LIBS += -L../../../libiio/$${build_subdir} -llibiio
}

MSVC {
    LIBS += -L"C:\Program Files\PothosSDR\bin" -L"C:\Program Files\PothosSDR\lib" -llibiio
}

macx {
    LIBS += -framework iio
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

RESOURCES = ../../../sdrgui/resources/res.qrc
