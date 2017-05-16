#!/bin/bash

test -d app/sdrangel.app/Contents || { echo "Please build first or chdir to proper folder!"; exit 1; }

cd app/sdrangel.app/Contents/lib 
DYLD_LIBRARY_PATH=/opt/local/lib:.; ../MacOS/sdrangel
