#!/bin/sh

OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Get options:
show_help() {
  cat << EOF
  Usage: ${0##*/} [-t tag] [-T tag] [-D] [-h]
  Run http-server and codegen in a Docker compose stack.
  -D         use this option to bring down the compose stack (default is to bring up).
  -t tag     Docker codegen image tag (default latest)
  -T tag     Docker server image tag (default latest)
  -b path    SDRangel source code root path (default /opt/build/sdrangel)
  -c name    Compose stack name (default sdrangelswg)
  -h         Print this help.
EOF
}

image_tag_codegen="latest"
image_tag_server="latest"
sdrangel_codebase="/opt/build/sdrangel"
stack_name="-p sdrangelswg"
action="up -d"

while getopts "h?Dt:T:b:c:" opt; do
    case "$opt" in
    h|\?)
        show_help
        exit 0
        ;;
    D)  action="down"
        ;;
    t)  image_tag_codegen=${OPTARG}
        ;;
    T)  image_tag_server=${OPTARG}
        ;;
    b)  sdrangel_codebase=${OPTARG}
        ;;
    c)  stack_name="-p ${OPTARG}"
        ;;
    esac
done

shift $((OPTIND-1))

[ "${1:-}" = "--" ] && shift
# End of get options

export USER_UID=$(id -u)
export IMAGE_CODEGEN_VERSION=${image_tag_codegen}
export IMAGE_SERVER_VERSION=${image_tag_server}
export SDRANGEL_BASE=${sdrangel_codebase}

docker compose -f compose.yml ${stack_name} ${action}
