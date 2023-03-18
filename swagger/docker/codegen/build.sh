#!/bin/sh

OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Get options:
show_help() {
  cat << EOF
  Usage: ${0##*/} [-b branch] [-c label] [-t tag] [-h]
  Build SDRangel image.
  -b name    Branch name (default sdrangel)
  -c label   Arbitrary clone label. Clone again if different from the last label (default current timestamp)
  -t tag     Docker image tag. Use git tag or commit hash (default latest)
  -f file    Specify a Dockerfile (default is Dockerfile in current directory i.e. '.')
  -h         Print this help.
EOF
}

branch_name="sdrangel"
clone_label=$(date)
image_tag="latest"
uid=1000
docker_file="."

while getopts "h?b:c:t:j:f:" opt; do
    case "$opt" in
    h|\?)
        show_help
        exit 0
        ;;
    b)  branch_name=${OPTARG}
        ;;
    c)  clone_label=${OPTARG}
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

IMAGE_NAME=sdrangel/swagger/codegen:${image_tag}
DOCKER_BUILDKIT=1 docker build \
    --build-arg branch=${branch_name} \
    --build-arg clone_label="${clone_label}" \
    --build-arg uid=${uid} \
    --target codegen \
    -t ${IMAGE_NAME} ${docker_file}
