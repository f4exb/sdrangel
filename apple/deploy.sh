#!/bin/bash

# Run from build directory after build
APP_PATH="app/sdrangel.app"
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

# 1st-pass: Gather libs & plugins
for i in `find . -name '*.dylib' -type f -not -path "./${APP_PATH}/*"`; do 
	if [[ $i == *"plugins/"* ]]; then
		dply_plugin $i
	else
		dply_lib $i
	fi
done

# 2nd-pass: Symlink libs
cd $APP_LIB
for i in `find . -name '*.1.0.0.dylib' -type f -not -path "./plugins/*"`; do
	ln -sf $i "${i/.1.0.0.dylib/.1.0.dylib}"
	ln -sf $i "${i/.1.0.0.dylib/.1.dylib}"
	ln -sf $i "${i/.1.0.0.dylib/.dylib}"
done
cd ../../..
pwd

# Deploy DMG
/Applications/Qt/5.12.0/clang_64/bin/macdeployqt ./sdrangel.app \
	-always-overwrite \
	-dmg \
	-libpath=sdrangel.app/Contents/Frameworks \
	-verbose=1

