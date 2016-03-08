#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = subdirs
SUBDIRS = sdrbase
SUBDIRS += plugins/samplesource/filesource

# Main app must be last
CONFIG += ordered
SUBDIRS += app
