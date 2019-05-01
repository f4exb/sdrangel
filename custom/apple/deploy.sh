#!/bin/bash
SDRANGEL_VERSION="4.5.0"
QT_VERSION="5.12.1"
QT_HOME="/Applications/Qt/${QT_VERSION}"

# Run from build directory after build
APP_PATH="app/SDRangel.app"
test -d "${APP_PATH}/Contents" || { echo "Please build first or chdir to proper folder!"; exit 1; }

APP_LIB="${APP_PATH}/Contents/Frameworks"
APP_PLUGINS="${APP_LIB}/plugins"

mkdir -p $APP_PLUGINS

dply_lib()
{
	cp -f $1 $APP_LIB
	echo "DeployLIB: `basename $1` to ${APP_LIB}"
}

dply_plugin()
{
	tmp=`dirname $1`
	tmp=`dirname $tmp`
	plugin_type=`basename $tmp`
	plugin_path="${APP_PLUGINS}/${plugin_type}"
	mkdir -p $plugin_path
	cp -f $1 $plugin_path
	echo "DeployPLUGIN: `basename $1` to ${plugin_path}"
}

add_dmg_entry()
{
	cp -fv $1 "${DMG_MPATH}/${2}"
}

# 1st-pass: Gather libs & plugins
for i in `find . -name '*.dylib' -type f -not -path "./${APP_PATH}/*"`; do 
	if [[ $i == *"plugins/"* ]]; then
		dply_plugin $i
	else
		dply_lib $i
	fi
done

# Missing ones
dply_lib /opt/install/LimeSuite/lib/libLimeSuite.18.10-1.dylib
dply_lib /opt/local/lib/libnanomsg.5.0.0.dylib
dply_lib /usr/local/lib/libmirisdr.4.dylib 
dply_lib /usr/local/lib/libSoapySDR.0.7.dylib 

# 2nd-pass: Symlink libs
cd $APP_LIB
for i in `find . -name '*.1.0.0.dylib' -type f -not -path "./plugins/*"`; do
	ln -sf $i "${i/.1.0.0.dylib/.1.0.dylib}"
	ln -sf $i "${i/.1.0.0.dylib/.1.dylib}"
	ln -sf $i "${i/.1.0.0.dylib/.dylib}"
done
cd ../../..

LIB_PATH="SDRangel.app/Contents/Frameworks"
CUR_PATH=`pwd`
cd $LIB_PATH
# Symbolic links
ln -sf "libnanomsg.5.0.0.dylib" "libnanomsg.5.dylib"

# Deploy DMG
cd $CUR_PATH
pwd
"${QT_HOME}/clang_64/bin/macdeployqt" ./SDRangel.app \
	-always-overwrite \
	-dmg \
	-libpath=${LIB_PATH} \
	-verbose=1

# Add other files to DMG
DMG_SRC="SDRangel.dmg"
DMG_RW_SRC=${DMG_SRC/.dmg/.rw.dmg}
DMG_TMP_A="/tmp/_dmg-attach"
hdiutil pmap $DMG_SRC
hdiutil convert $DMG_SRC -format UDRW -o $DMG_RW_SRC
hdiutil resize -limits $DMG_RW_SRC
hdiutil attach $DMG_RW_SRC > $DMG_TMP_A
DMG_MPATH=`cat $DMG_TMP_A | grep Volumes | awk '{ print $3 }'`

# Append files into filesystem
add_dmg_entry ../../../libiio/build/libiio-0.14.g17b73d3.pkg
add_dmg_entry ../../sdrangel/ReadmeMacOS.md

hdiutil detach $DMG_MPATH
# Publish version
VERIMAGIC="-v${SDRANGEL_VERSION}_`date +%Y%m%d_%H%M%S`-Qt${QT_VERSION}"
DMG_DEPLOY=${DMG_SRC/.dmg/${VERIMAGIC}.dmg}
hdiutil convert $DMG_RW_SRC -format UDBZ -o $DMG_DEPLOY
rm -f $DMG_RW_SRC

echo "DeployedDMG: ${DMG_DEPLOY}"
exit 0
