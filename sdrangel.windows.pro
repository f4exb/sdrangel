#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = subdirs
SUBDIRS = sdrbase
SUBDIRS += lz4
SUBDIRS += plugins/samplesource/filesource
SUBDIRS += plugins/samplesource/sdrdaemon
SUBDIRS += plugins/channel/chanalyzer
SUBDIRS += plugins/channel/demodam
SUBDIRS += plugins/channel/demodbfm
SUBDIRS += plugins/channel/demodlora
SUBDIRS += plugins/channel/demodnfm

# Main app must be last
CONFIG += ordered
SUBDIRS += app
