#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = subdirs

SUBDIRS = serialdv
SUBDIRS += httpserver
SUBDIRS += logging
SUBDIRS += swagger
SUBDIRS += sdrbase
SUBDIRS += sdrgui
CONFIG(MINGW64)SUBDIRS += nanomsg
SUBDIRS += fcdhid
SUBDIRS += fcdlib
SUBDIRS += librtlsdr
SUBDIRS += libhackrf
SUBDIRS += libairspy
SUBDIRS += libbladerf
SUBDIRS += libsqlite3
SUBDIRS += liblimesuite
SUBDIRS += libiio
SUBDIRS += devices
SUBDIRS += mbelib
SUBDIRS += dsdcc
CONFIG(MINGW64)SUBDIRS += cm256cc
SUBDIRS += plugins/samplesource/filesource
CONFIG(MINGW64)SUBDIRS += plugins/samplesource/sdrdaemonsource
SUBDIRS += plugins/samplesource/rtlsdr
SUBDIRS += plugins/samplesource/hackrfinput
SUBDIRS += plugins/samplesource/airspy
SUBDIRS += plugins/samplesource/bladerfinput
SUBDIRS += plugins/samplesource/limesdrinput
SUBDIRS += plugins/samplesource/plutosdrinput
SUBDIRS += plugins/samplesink/filesink
SUBDIRS += plugins/samplesink/bladerfoutput
SUBDIRS += plugins/samplesink/hackrfoutput
SUBDIRS += plugins/samplesink/limesdroutput
SUBDIRS += plugins/samplesink/plutosdroutput
SUBDIRS += plugins/channelrx/chanalyzer
SUBDIRS += plugins/channelrx/chanalyzerng
SUBDIRS += plugins/channelrx/demodam
SUBDIRS += plugins/channelrx/demodatv
SUBDIRS += plugins/channelrx/demodbfm
SUBDIRS += plugins/channelrx/demoddsd
SUBDIRS += plugins/channelrx/demodlora
SUBDIRS += plugins/channelrx/demodnfm
SUBDIRS += plugins/channelrx/demodssb
SUBDIRS += plugins/channelrx/demodwfm
SUBDIRS += plugins/channelrx/tcpsrc
SUBDIRS += plugins/channelrx/udpsrc
SUBDIRS += plugins/channeltx/modam
CONFIG(macx)SUBDIRS += plugins/channeltx/modatv
SUBDIRS += plugins/channeltx/modnfm
SUBDIRS += plugins/channeltx/modssb
SUBDIRS += plugins/channeltx/modwfm
SUBDIRS += plugins/channeltx/udpsink

# Main app must be last
CONFIG += ordered
SUBDIRS += app
