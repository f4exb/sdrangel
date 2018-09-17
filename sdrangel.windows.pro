#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = serialdv
SUBDIRS += httpserver
SUBDIRS += logging
SUBDIRS += qrtplib
SUBDIRS += swagger
SUBDIRS += sdrbase
SUBDIRS += sdrgui
SUBDIRS += fcdhid
SUBDIRS += fcdlib
SUBDIRS += libairspy
SUBDIRS += libairspyhf
SUBDIRS += libbladerf
SUBDIRS += libhackrf
SUBDIRS += libiio
SUBDIRS += liblimesuite
SUBDIRS += libperseus
SUBDIRS += librtlsdr
SUBDIRS += devices
SUBDIRS += mbelib
SUBDIRS += dsdcc
#SUBDIRS += cm256cc
SUBDIRS += plugins/samplesource/airspy
SUBDIRS += plugins/samplesource/airspyhf
SUBDIRS += plugins/samplesource/bladerfinput
SUBDIRS += plugins/samplesource/filesource
SUBDIRS += plugins/samplesource/hackrfinput
SUBDIRS += plugins/samplesource/limesdrinput
SUBDIRS += plugins/samplesource/plutosdrinput
#SUBDIRS += plugins/samplesource/sdrdaemonsource
SUBDIRS += plugins/samplesource/rtlsdr
SUBDIRS += plugins/samplesource/testsource
SUBDIRS += plugins/samplesink/filesink
SUBDIRS += plugins/samplesink/bladerfoutput
SUBDIRS += plugins/samplesink/hackrfoutput
SUBDIRS += plugins/samplesink/limesdroutput
SUBDIRS += plugins/samplesink/plutosdroutput
#SUBDIRS += plugins/samplesink/sdrdaemonsink
SUBDIRS += plugins/channelrx/chanalyzer
SUBDIRS += plugins/channelrx/demodam
SUBDIRS += plugins/channelrx/demodatv
SUBDIRS += plugins/channelrx/demodbfm
SUBDIRS += plugins/channelrx/demoddsd
SUBDIRS += plugins/channelrx/demodlora
SUBDIRS += plugins/channelrx/demodnfm
SUBDIRS += plugins/channelrx/demodssb
SUBDIRS += plugins/channelrx/demodwfm
SUBDIRS += plugins/channelrx/udpsink
SUBDIRS += plugins/channeltx/modam
SUBDIRS += plugins/channeltx/modnfm
SUBDIRS += plugins/channeltx/modssb
SUBDIRS += plugins/channeltx/modwfm
SUBDIRS += plugins/channeltx/udpsource

# Main app must be last
SUBDIRS += app
