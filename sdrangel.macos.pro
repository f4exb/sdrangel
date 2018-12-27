#--------------------------------------------------------
#
# Pro file for MacOS builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = subdirs
SUBDIRS = qrtplib
SUBDIRS += httpserver
SUBDIRS += swagger
SUBDIRS += logging
SUBDIRS += sdrbase
SUBDIRS += sdrgui
SUBDIRS += devices
SUBDIRS += fcdhid
SUBDIRS += fcdlib
SUBDIRS += mbelib
SUBDIRS += dsdcc
SUBDIRS += cm256cc
#SUBDIRS += liblimesuite
#SUBDIRS += librtlsdr
SUBDIRS += plugins/samplesource/filesource
SUBDIRS += plugins/samplesource/sdrdaemonsource
SUBDIRS += plugins/samplesource/hackrfinput
SUBDIRS += plugins/samplesource/rtlsdr
SUBDIRS += plugins/samplesink/filesink
SUBDIRS += plugins/samplesink/sdrdaemonsink
SUBDIRS += plugins/samplesink/hackrfoutput
SUBDIRS += plugins/channelrx/chanalyzer
#SUBDIRS += plugins/channelrx/daemonsink
SUBDIRS += plugins/channelrx/demodam
SUBDIRS += plugins/channelrx/demodatv
SUBDIRS += plugins/channelrx/demodbfm
SUBDIRS += plugins/channelrx/demoddsd
SUBDIRS += plugins/channelrx/demodlora
SUBDIRS += plugins/channelrx/demodnfm
SUBDIRS += plugins/channelrx/demodssb
SUBDIRS += plugins/channelrx/demodwfm
SUBDIRS += plugins/channelrx/udpsink
#SUBDIRS += plugins/channeltx/daemonsource
SUBDIRS += plugins/channeltx/modam
SUBDIRS += plugins/channeltx/modatv
SUBDIRS += plugins/channeltx/modnfm
SUBDIRS += plugins/channeltx/modssb
SUBDIRS += plugins/channeltx/modwfm
SUBDIRS += plugins/channeltx/udpsource

# Main app must be last
CONFIG += ordered
SUBDIRS += app
