#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libsoapysdr

macx {
    INCLUDEPATH += "/usr/local/include"
    INCLUDEPATH += "/opt/local/include"
    SOURCES = 
    HEADERS =
    LIBS += -L/usr/local/lib -lSoapySDR
    LIBS += -L/opt/local/lib -lusb-1.0
}   
