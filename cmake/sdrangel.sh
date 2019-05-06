#!/bin/sh

SDRANGEL_BUNDLE="`echo "$0" | sed -e 's/\/Contents\/MacOS\/.*//'`"
SDRANGEL_RESOURCES="$SDRANGEL_BUNDLE/Contents/Resources"

echo "running $0"
echo "SDRANGEL_BUNDLE: $SDRANGEL_BUNDLE"

export "DYLD_LIBRARY_PATH=$SDRANGEL_RESOURCES/lib"
export "PATH=$SDRANGEL_RESOURCES/bin:$PATH"

exec "$SDRANGEL_RESOURCES/bin/sdrangel"
