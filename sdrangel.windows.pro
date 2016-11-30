#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = subdirs
SUBDIRS = sdrbase
SUBDIRS += lz4
CONFIG(MINGW64)SUBDIRS += nanomsg
SUBDIRS += fcdhid
SUBDIRS += fcdlib
SUBDIRS += librtlsdr
SUBDIRS += libhackrf
SUBDIRS += libairspy
SUBDIRS += libbladerf
SUBDIRS += mbelib
SUBDIRS += dsdcc
SUBDIRS += serialdv
CONFIG(MINGW64)SUBDIRS += cm256cc
SUBDIRS += plugins/samplesource/filesource
CONFIG(MINGW64)SUBDIRS += plugins/samplesource/sdrdaemon
CONFIG(MINGW64)SUBDIRS += plugins/samplesource/sdrdaemonfec
SUBDIRS += plugins/samplesource/rtlsdr
SUBDIRS += plugins/samplesource/hackrf
SUBDIRS += plugins/samplesource/airspy
SUBDIRS += plugins/samplesource/bladerf
SUBDIRS += plugins/samplesink/filesink
SUBDIRS += plugins/channelrx/chanalyzer
SUBDIRS += plugins/channelrx/demodam
SUBDIRS += plugins/channelrx/demodbfm
SUBDIRS += plugins/channelrx/demoddsd
SUBDIRS += plugins/channelrx/demodlora
SUBDIRS += plugins/channelrx/demodnfm
SUBDIRS += plugins/channelrx/demodssb
SUBDIRS += plugins/channelrx/demodwfm
SUBDIRS += plugins/channelrx/tcpsrc
SUBDIRS += plugins/channelrx/udpsrc
SUBDIRS += plugins/channeltx/modam
SUBDIRS += plugins/channeltx/modnfm

# Main app must be last
CONFIG += ordered
SUBDIRS += app
