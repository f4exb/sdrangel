#!/bin/sh

get_abs_dir() {
  # $1 : relative filename
  echo "$(cd "$(dirname "$1")" && pwd)"
}

SDRANGEL_RESOURCES="$(get_abs_dir "$0")/../Resources"
exec "$SDRANGEL_RESOURCES/sdrangel"

