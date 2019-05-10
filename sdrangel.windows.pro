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
CONFIG(!MSVC):SUBDIRS += fcdhid
CONFIG(!MSVC):SUBDIRS += fcdlib
SUBDIRS += libairspy
SUBDIRS += libairspyhf
CONFIG(!MSVC)SUBDIRS += libbladerf
CONFIG(!MSVC)SUBDIRS += libhackrf
CONFIG(!MSVC):SUBDIRS += libiio
CONFIG(!MSVC):SUBDIRS += liblimesuite
CONFIG(!MSVC):SUBDIRS += libperseus
SUBDIRS += librtlsdr
SUBDIRS += devices
SUBDIRS += mbelib
SUBDIRS += dsdcc
CONFIG(MSVC):SUBDIRS += cm256cc
SUBDIRS += plugins/samplesource/airspy
SUBDIRS += plugins/samplesource/airspyhf
SUBDIRS += plugins/samplesource/bladerf1input
SUBDIRS += plugins/samplesource/bladerf2input
SUBDIRS += plugins/samplesource/filesource
SUBDIRS += plugins/samplesource/hackrfinput
SUBDIRS += plugins/samplesource/limesdrinput
SUBDIRS += plugins/samplesource/plutosdrinput
SUBDIRS += plugins/samplesource/localinput
CONFIG(MSVC):SUBDIRS += plugins/samplesource/remoteinput
SUBDIRS += plugins/samplesource/rtlsdr
CONFIG(MSVC):SUBDIRS += plugins/samplesource/soapysdrinput
SUBDIRS += plugins/samplesource/testsource
SUBDIRS += plugins/samplesink/filesink
SUBDIRS += plugins/samplesink/bladerf1output
SUBDIRS += plugins/samplesink/bladerf2output
SUBDIRS += plugins/samplesink/hackrfoutput
SUBDIRS += plugins/samplesink/limesdroutput
SUBDIRS += plugins/samplesink/plutosdroutput
SUBDIRS += plugins/samplesink/localoutput
CONFIG(MSVC):SUBDIRS += plugins/samplesink/remoteoutput
CONFIG(MSVC):SUBDIRS += plugins/samplesink/soapysdroutput
SUBDIRS += plugins/channelrx/chanalyzer
SUBDIRS += plugins/channelrx/demodam
SUBDIRS += plugins/channelrx/demodatv
SUBDIRS += plugins/channelrx/demodbfm
SUBDIRS += plugins/channelrx/demoddsd
CONFIG(MSVC):SUBDIRS += plugins/channelrx/demoddatv
CONFIG(!MSVC):SUBDIRS += plugins/channelrx/demodlora
SUBDIRS += plugins/channelrx/demodnfm
SUBDIRS += plugins/channelrx/demodssb
SUBDIRS += plugins/channelrx/demodwfm
SUBDIRS += plugins/channelrx/udpsink
SUBDIRS += plugins/channelrx/localsink
SUBDIRS += plugins/channelrx/freqtracker
SUBDIRS += plugins/channeltx/modam
SUBDIRS += plugins/channeltx/modnfm
SUBDIRS += plugins/channeltx/modssb
SUBDIRS += plugins/channeltx/modwfm
SUBDIRS += plugins/channeltx/udpsource
SUBDIRS += plugins/channeltx/localsource

# Main app must be last
SUBDIRS += app
