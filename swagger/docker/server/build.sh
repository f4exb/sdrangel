#!/bin/sh

OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Get options:
show_help() {
  cat << EOF
  Usage: ${0##*/} [-b name] [-c label] [-t version] [-h]
  Build SDRangel image.
  -t tag     Docker image tag version (default latest)
  -f file    Specify a Dockerfile (default is Dockerfile in current directory i.e. '.')
  -h         Print this help.
EOF
}

image_name="sdrangel/swagger/server"
image_tag="latest"
uid=1000
docker_file="."

while getopts "h?t:f:" opt; do
    case "$opt" in
    h|\?)
        show_help
        exit 0
        ;;
    t)  image_tag=${OPTARG}
        ;;
    f)  docker_file="-f ${OPTARG} ."
        ;;
    esac
done

shift $((OPTIND-1))

[ "${1:-}" = "--" ] && shift
# End of get options

IMAGE_NAME=${image_name}:${image_tag}
DOCKER_BUILDKIT=1 docker build \
    --build-arg uid=${uid} \
    --target codegen_server \
    -t ${IMAGE_NAME} ${docker_file}
