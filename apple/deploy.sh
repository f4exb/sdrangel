#!/bin/bash

# Run from build directory
test -d "app/sdrangel.app/Contents" || { echo "Please build first or chdir to proper folder!"; exit 1; }

APP_LIB="app/sdrangel.app/Contents/lib"
APP_PLUGINS="${APP_LIB}/plugins"

mkdir -p "${APP_PLUGINS}/channeltx"
mkdir -p "${APP_PLUGINS}/channelrx"
mkdir -p "${APP_PLUGINS}/samplesink"
mkdir -p "${APP_PLUGINS}/samplesource"

cp -v fcdhid/libfcdhid.dylib $APP_LIB
cp -v fcdlib/libfcdlib.dylib $APP_LIB
cp -v mbelib/libmbelib.dylib $APP_LIB
cp -v sdrbase/libsdrbase.dylib $APP_LIB
cp -v sdrbase/libsdrgui.dylib $APP_LIB
cp -v devices/libdevices.dylib $APP_LIB
cp -v dsdcc/libdsdcc.dylib $APP_LIB

for f in `find plugins/channelrx/ -name '*.dylib'`; do cp -v $f "${APP_PLUGINS}/channelrx/"; done
for f in `find plugins/channeltx/ -name '*.dylib'`; do cp -v $f "${APP_PLUGINS}/channeltx/"; done
for f in `find plugins/samplesink/ -name '*.dylib'`; do cp -v $f "${APP_PLUGINS}/samplesink/"; done
for f in `find plugins/samplesource/ -name '*.dylib'`; do cp -v $f "${APP_PLUGINS}/samplesource/"; done

cd $APP_LIB
cp /opt/local/lib/libnanomsg.5.0.0.dylib .
ln -s libdsdcc.dylib libdsdcc.1.dylib
ln -s libdevices.dylib libdevices.1.dylib
ln -s libsdrbase.dylib libsdrbase.1.dylib
ln -s libmbelib.dylib libmbelib.1.dylib
