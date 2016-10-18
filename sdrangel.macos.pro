#--------------------------------------------------------
#
# Pro file for MacOS builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = subdirs
SUBDIRS = sdrbase
SUBDIRS += lz4
SUBDIRS += librtlsdr
SUBDIRS += libhackrf
SUBDIRS += libairspy
SUBDIRS += plugins/samplesource/filesource
SUBDIRS += plugins/samplesource/sdrdaemon
SUBDIRS += plugins/samplesource/rtlsdr
SUBDIRS += plugins/samplesource/hackrf
SUBDIRS += plugins/samplesource/airspy
SUBDIRS += plugins/channelrx/chanalyzer
SUBDIRS += plugins/channelrx/demodam
SUBDIRS += plugins/channelrx/demodbfm
SUBDIRS += plugins/channelrx/demodlora
SUBDIRS += plugins/channelrx/demodnfm
SUBDIRS += plugins/channelrx/demodssb
SUBDIRS += plugins/channelrx/demodwfm
SUBDIRS += plugins/channelrx/tcpsrc
SUBDIRS += plugins/channelrx/udpsrc

# Main app must be last
CONFIG += ordered
SUBDIRS += app
