#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libmirisdr

macx {
    INCLUDEPATH += "/opt/local/include"
    INCLUDEPATH += "/usr/local/include"
    SOURCES = 
    HEADERS =
    LIBS += -L/usr/local/lib -lmirisdr
    LIBS += -L/opt/local/lib -lusb-1.0
}   
