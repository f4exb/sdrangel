#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputairspyhf

CONFIG(MINGW32):LIBAIRSPYHFSRC = "C:\softs\airspyhf"
CONFIG(MSVC):LIBAIRSPYHFSRC = "C:\softs\airspyhf"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
INCLUDEPATH += $$LIBAIRSPYHFSRC
CONFIG(macx):INCLUDEPATH += "/usr/local/include"

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += airspyhfgui.cpp\
  airspyhfinput.cpp\
  airspyhfplugin.cpp\
  airspyhfsettings.cpp\
  airspyhfthread.cpp

HEADERS += airspyhfgui.h\
  airspyhfinput.h\
  airspyhfplugin.h\
  airspyhfsettings.h\
  airspyhfthread.h

FORMS += airspyhfgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
LIBS += -L../../../libairspyhf/$${build_subdir} -llibairspyhf
macx {
    LIBS -= -L../../../libairspyhf/$${build_subdir} -llibairspyhf
    LIBS += -L/usr/local/lib -lairspyhf
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

RESOURCES = ../../../sdrgui/resources/res.qrc
