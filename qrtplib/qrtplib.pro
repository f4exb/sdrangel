#--------------------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core network

TEMPLATE = lib
TARGET = qrtplib

INCLUDEPATH += $$PWD
INCLUDEPATH += ../sdrbase

QMAKE_CXXFLAGS += -std=c++11

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

# Enable very detailed debug messages when compiling the debug version
CONFIG(debug, debug|release) {
    DEFINES += SUPERVERBOSE
}

HEADERS += $$PWD/rtcpapppacket.h \
    $$PWD/rtcpbyepacket.h \
    $$PWD/rtcpcompoundpacket.h \
    $$PWD/rtcpcompoundpacketbuilder.h \
    $$PWD/rtcppacket.h \
    $$PWD/rtcppacketbuilder.h \
    $$PWD/rtcprrpacket.h \
    $$PWD/rtcpscheduler.h \
    $$PWD/rtcpsdesinfo.h \
    $$PWD/rtcpsdespacket.h \
    $$PWD/rtcpsrpacket.h \
    $$PWD/rtcpunknownpacket.h \
    $$PWD/rtpaddress.h \
    $$PWD/rtpcollisionlist.h \
    $$PWD/rtpconfig.h \
    $$PWD/rtpdefines.h \
    $$PWD/rtpendian.h \
    $$PWD/rtperrors.h \
    $$PWD/rtpinternalsourcedata.h \
    $$PWD/rtppacket.h \
    $$PWD/rtppacketbuilder.h \
    $$PWD/rtprandom.h \
    $$PWD/rtprandomrand48.h \
    $$PWD/rtprandomrands.h \
    $$PWD/rtprandomurandom.h \
    $$PWD/rtprawpacket.h \
    $$PWD/rtpsession.h \
    $$PWD/rtpsessionparams.h \
    $$PWD/rtpsessionsources.h \
    $$PWD/rtpsourcedata.h \
    $$PWD/rtpsources.h \
    $$PWD/rtpstructs.h \
    $$PWD/rtptimeutilities.h \
    $$PWD/rtptransmitter.h \
    $$PWD/rtptypes_win.h \
    $$PWD/rtptypes.h \
    $$PWD/rtpudptransmitter.h \
    $$PWD/rtpsocketutil.h \
    $$PWD/rtpselect.h


SOURCES += $$PWD/rtcpapppacket.cpp \
    $$PWD/rtcpbyepacket.cpp \
    $$PWD/rtcpcompoundpacket.cpp \
    $$PWD/rtcpcompoundpacketbuilder.cpp \
    $$PWD/rtcppacketbuilder.cpp \
    $$PWD/rtcprrpacket.cpp \
    $$PWD/rtcpscheduler.cpp \
    $$PWD/rtcpsdesinfo.cpp \
    $$PWD/rtcpsdespacket.cpp \
    $$PWD/rtcpsrpacket.cpp \
    $$PWD/rtpaddress.cpp \
    $$PWD/rtpcollisionlist.cpp \
    $$PWD/rtperrors.cpp \
    $$PWD/rtpinternalsourcedata.cpp \
    $$PWD/rtppacket.cpp \
    $$PWD/rtppacketbuilder.cpp \
    $$PWD/rtprandom.cpp \
    $$PWD/rtprandomrand48.cpp \
    $$PWD/rtprandomrands.cpp \
    $$PWD/rtprandomurandom.cpp \
    $$PWD/rtpsession.cpp \
    $$PWD/rtpsessionparams.cpp \
    $$PWD/rtpsessionsources.cpp \
    $$PWD/rtpsourcedata.cpp \
    $$PWD/rtpsources.cpp \
    $$PWD/rtptimeutilities.cpp \
    $$PWD/rtpudptransmitter.cpp
