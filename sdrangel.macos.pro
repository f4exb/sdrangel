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

SUBDIRS += fcdhid
SUBDIRS += fcdlib
SUBDIRS += mbelib
SUBDIRS += dsdcc
SUBDIRS += cm256cc

SUBDIRS += libairspy
SUBDIRS += libairspyhf
SUBDIRS += libbladerf
SUBDIRS += libhackrf
SUBDIRS += liblimesuite
#SUBDIRS += libmirisdr
SUBDIRS += libperseus
SUBDIRS += librtlsdr
SUBDIRS += libsoapysdr

SUBDIRS += devices

SUBDIRS += plugins/samplesource/airspy
SUBDIRS += plugins/samplesource/airspyhf
SUBDIRS += plugins/samplesource/bladerf1input
SUBDIRS += plugins/samplesource/bladerf2input
SUBDIRS += plugins/samplesource/fcdpro
SUBDIRS += plugins/samplesource/fcdproplus
SUBDIRS += plugins/samplesource/filesource
SUBDIRS += plugins/samplesource/hackrfinput
SUBDIRS += plugins/samplesource/limesdrinput
SUBDIRS += plugins/samplesource/perseus
SUBDIRS += plugins/samplesource/plutosdrinput
SUBDIRS += plugins/samplesource/rtlsdr
SUBDIRS += plugins/samplesource/sdrdaemonsource
SUBDIRS += plugins/samplesource/sdrplay
SUBDIRS += plugins/samplesource/soapysdrinput
SUBDIRS += plugins/samplesource/testsource

SUBDIRS += plugins/samplesink/bladerf1output
SUBDIRS += plugins/samplesink/bladerf2output
SUBDIRS += plugins/samplesink/filesink
SUBDIRS += plugins/samplesink/hackrfoutput
SUBDIRS += plugins/samplesink/limesdroutput
SUBDIRS += plugins/samplesink/plutosdroutput
SUBDIRS += plugins/samplesink/sdrdaemonsink
SUBDIRS += plugins/samplesink/soapysdroutput

SUBDIRS += plugins/channelrx/chanalyzer
SUBDIRS += plugins/channelrx/daemonsink
SUBDIRS += plugins/channelrx/demodam
#SUBDIRS += plugins/channelrx/demodatv
SUBDIRS += plugins/channelrx/demodbfm
SUBDIRS += plugins/channelrx/demodatv
SUBDIRS += plugins/channelrx/demoddsd
SUBDIRS += plugins/channelrx/demodlora
SUBDIRS += plugins/channelrx/demodnfm
SUBDIRS += plugins/channelrx/demodssb
SUBDIRS += plugins/channelrx/demodwfm
SUBDIRS += plugins/channelrx/udpsink

SUBDIRS += plugins/channeltx/daemonsource
SUBDIRS += plugins/channeltx/modam
SUBDIRS += plugins/channeltx/modatv
SUBDIRS += plugins/channeltx/modnfm
SUBDIRS += plugins/channeltx/modssb
SUBDIRS += plugins/channeltx/modwfm
SUBDIRS += plugins/channeltx/udpsource

# Main app must be last
CONFIG += ordered
SUBDIRS += app
