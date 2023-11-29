#!/bin/sh
git diff --name-only ${1}..${2} | grep plugins/ | cut -d'/' -f2,3 | sort -u
