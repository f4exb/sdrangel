#--------------------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = dsdcc

CONFIG(MINGW32):LIBDSDCCSRC = "C:\softs\dsdcc"
CONFIG(MINGW64):LIBDSDCCSRC = "C:\softs\dsdcc"
CONFIG(macx):LIBDSDCCSRC = "../../deps/dsdcc"

CONFIG(MINGW32):LIBMBELIBSRC = "C:\softs\mbelib"
CONFIG(MINGW64):LIBMBELIBSRC = "C:\softs\mbelib"
CONFIG(macx):LIBMBELIBSRC = "../../deps/mbelib"

INCLUDEPATH += $$LIBDSDCCSRC
INCLUDEPATH += $$LIBMBELIBSRC

DEFINES += DSD_USE_MBELIB=1

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES = $$LIBDSDCCSRC/descramble.cpp\
$$LIBDSDCCSRC/dmr.cpp\
$$LIBDSDCCSRC/dsd_decoder.cpp\
$$LIBDSDCCSRC/dsd_filters.cpp\
$$LIBDSDCCSRC/dsd_logger.cpp\
$$LIBDSDCCSRC/dsd_mbe.cpp\
$$LIBDSDCCSRC/dsd_opts.cpp\
$$LIBDSDCCSRC/dsd_state.cpp\
$$LIBDSDCCSRC/dsd_symbol.cpp\
$$LIBDSDCCSRC/dstar.cpp\
$$LIBDSDCCSRC/ysf.cpp\
$$LIBDSDCCSRC/nxdn.cpp\
$$LIBDSDCCSRC/nxdnconvolution.cpp\
$$LIBDSDCCSRC/nxdncrc.cpp\
$$LIBDSDCCSRC/nxdnmessage.cpp\
$$LIBDSDCCSRC/dpmr.cpp\
$$LIBDSDCCSRC/p25p1_heuristics.cpp\
$$LIBDSDCCSRC/fec.cpp\
$$LIBDSDCCSRC/crc.cpp\
$$LIBDSDCCSRC/viterbi.cpp\
$$LIBDSDCCSRC/viterbi3.cpp\
$$LIBDSDCCSRC/viterbi5.cpp\
$$LIBDSDCCSRC/pn.cpp\
$$LIBDSDCCSRC/mbefec.cpp\
$$LIBDSDCCSRC/phaselock.cpp\
$$LIBDSDCCSRC/locator.cpp

HEADERS = $$LIBDSDCCSRC/descramble.h\
$$LIBDSDCCSRC/dmr.h\
$$LIBDSDCCSRC/dsd_decoder.h\
$$LIBDSDCCSRC/dsd_filters.h\
$$LIBDSDCCSRC/dsd_logger.h\
$$LIBDSDCCSRC/dsd_mbe.h\
$$LIBDSDCCSRC/dsd_mbelib.h\
$$LIBDSDCCSRC/dsd_opts.h\
$$LIBDSDCCSRC/dsd_state.h\
$$LIBDSDCCSRC/dsd_symbol.h\
$$LIBDSDCCSRC/dstar.h\
$$LIBDSDCCSRC/ysf.h\
$$LIBDSDCCSRC/nxdn.h\
$$LIBDSDCCSRC/nxdnconvolution.h\
$$LIBDSDCCSRC/nxdncrc.h\
$$LIBDSDCCSRC/nxdnmessage.h\
$$LIBDSDCCSRC/dpmr.h\
$$LIBDSDCCSRC/p25p1_heuristics.h\
$$LIBDSDCCSRC/runningmaxmin.h\
$$LIBDSDCCSRC/doublebuffer.h\
$$LIBDSDCCSRC/fec.h\
$$LIBDSDCCSRC/crc.h\
$$LIBDSDCCSRC/viterbi.h\
$$LIBDSDCCSRC/viterbi3.h\
$$LIBDSDCCSRC/viterbi5.h\
$$LIBDSDCCSRC/pn.h\
$$LIBDSDCCSRC/mbefec.h\
$$LIBDSDCCSRC/phaselock.h\
$$LIBDSDCCSRC/locator.h

LIBS += -L../mbelib/$${build_subdir} -lmbelib
