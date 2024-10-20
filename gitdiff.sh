#!/bin/sh
BASEDIR=$(dirname $0)
PLUGINS=$(git diff --name-only ${1}..${2} | grep plugins/ | cut -d'/' -f2,3 | sort -u)
for plugin in $PLUGINS
do
    FILE=$(find $BASEDIR/plugins/$plugin -name "*plugin.cpp")
    sed -i -E "s/QStringLiteral\(\"7\.(.*)\"\)/QStringLiteral\(\"7\.22\.2\"\)/" $FILE
done

