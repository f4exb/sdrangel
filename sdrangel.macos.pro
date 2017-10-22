#--------------------------------------------------------
#
# Pro file for MacOS builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = subdirs
SUBDIRS = sdrbase
SUBDIRS += sdrgui
SUBDIRS += devices
SUBDIRS += fcdhid
SUBDIRS += fcdlib
SUBDIRS += mbelib
SUBDIRS += dsdcc
SUBDIRS += plugins/samplesource/filesource
SUBDIRS += plugins/samplesource/sdrdaemon
SUBDIRS += plugins/samplesource/rtlsdr
SUBDIRS += plugins/samplesource/hackrfinput
SUBDIRS += plugins/samplesink/filesink
SUBDIRS += plugins/samplesink/hackrfoutput
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
SUBDIRS += plugins/channeltx/modatv
SUBDIRS += plugins/channeltx/modnfm
SUBDIRS += plugins/channeltx/modssb
SUBDIRS += plugins/channeltx/modwfm
SUBDIRS += plugins/channeltx/udpsink

# Main app must be last
CONFIG += ordered
SUBDIRS += app
