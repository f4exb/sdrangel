#-------------------------------------------------
#
# Pro file for Windows MSVC builds with Qt Creator
#
#-------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui multimedia opengl

TARGET = inputsoapysdrinput

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../devices
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
INCLUDEPATH += "C:\Programs\PothosSDR\include"

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += soapysdrinput.cpp\
    soapysdrinputgui.cpp\
    soapysdrinputplugin.cpp\
    soapysdrinputsettings.cpp\
    soapysdrinputthread.cpp

HEADERS += soapysdrinput.h\
    soapysdrinputgui.h\
    soapysdrinputplugin.h\
    soapysdrinputsettings.h\
    soapysdrinputthread.h

FORMS += soapysdrinputgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
LIBS += -LC:\Programs\PothosSDR\bin -LC:\Programs\PothosSDR\lib -lSoapySDR
