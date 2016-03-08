#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia

TARGET = inputfilesource
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(release): build_subdir = release
CONFIG(debug): build_subdir = debug

SOURCES += filesourcegui.cpp\
	filesourceinput.cpp\
	filesourceplugin.cpp\
	filesourcethread.cpp

HEADERS += filesourcegui.h\
	filesourceinput.h\
	filesourceplugin.h\
	filesourcethread.h

FORMS += filesourcegui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc
