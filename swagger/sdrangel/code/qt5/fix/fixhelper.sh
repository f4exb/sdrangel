#!/bin/sh
BASEDIR=$(dirname "$0")
sed -i '1s/^/#include <QDateTime>\n/' ${BASEDIR}/../client/SWGHelpers.cpp
