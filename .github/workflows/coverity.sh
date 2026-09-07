#!/bin/bash
# Loosely based off https://github.com/the-tcpdump-group/libpcap/blob/master/.ci-coverity-scan-build.sh
# which is based off a script coverity released https://scan.coverity.com/download

if [ -z "${BASH_VERSION:-}" ]; then
    echo "ERROR: This script must be run with bash"
    exit 1
fi

set_output()
{
  if [ -n "${GITHUB_OUTPUT:-}" ]; then
    echo "$1" >> "$GITHUB_OUTPUT"
  else
    echo "$1"
  fi
}

set -euo pipefail

if [[ $- == *x* ]]; then
    echo "ERROR: bash xtrace (-x) is enabled and that might expose COVERITY_SCAN_TOKEN" >&2
    exit 1
fi

PRECHECK=false
if [ "${1:-}" = "--precheck" ]; then
    PRECHECK=true
fi

# Environment check
if [ -z "${COVERITY_SCAN_PROJECT_NAME:-}" ] ||
   [ -z "${COVERITY_SCAN_PROJECT_ID:-}" ] ||
   [ -z "${COVERITY_SCAN_NOTIFICATION_EMAIL:-}" ] ||
   [ -z "${COVERITY_SCAN_TOKEN:-}" ]; then
  printf "\033[33;1mNote: COVERITY_SCAN_PROJECT_NAME, COVERITY_SCAN_PROJECT_ID, COVERITY_SCAN_TOKEN are available on the Project Settings page on scan.coverity.com\033[0m\n"

  [ -z "${COVERITY_SCAN_PROJECT_NAME:-}" ] && echo "ERROR: COVERITY_SCAN_PROJECT_NAME must be set"
  [ -z "${COVERITY_SCAN_PROJECT_ID:-}" ] && echo "ERROR: COVERITY_SCAN_PROJECT_ID must be set"
  [ -z "${COVERITY_SCAN_NOTIFICATION_EMAIL:-}" ] && echo "ERROR: COVERITY_SCAN_NOTIFICATION_EMAIL must be set"
  [ -z "${COVERITY_SCAN_TOKEN:-}" ] && echo "ERROR: COVERITY_SCAN_TOKEN must be set"

  exit 1
fi

REQUIRED_TOOLS=("curl" "git" "sed" "paste" "tar" "ruby" "md5sum" "jq")
for tool in "${REQUIRED_TOOLS[@]}"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: Required system tool '$tool' is not installed or available in PATH." >&2
    exit 1
  fi
done

PLATFORM=$(uname)
TOOL_ARCHIVE=/tmp/cov-analysis-${PLATFORM}.tgz
TOOL_URL=https://scan.coverity.com/download/linux64
TOOL_BASE=/tmp/coverity-scan-analysis
SCAN_URL="https://scan.coverity.com"
JOBS=$(( $(nproc) - 2 ))
[ "$JOBS" -lt 1 ] && JOBS=1

# Safely locate the repository root directory relative to this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CMAKE_FILE="$REPO_ROOT/CMakeLists.txt"
if [ ! -f "$CMAKE_FILE" ]; then
  echo "ERROR: CAN NOT FIND CMakeLists.txt"
  exit 1
fi

SHA=$(git -C "$REPO_ROOT" rev-parse --short HEAD)
VERSION_SHA=$(sed -nE \
  's/^set\(sdrangel_VERSION_(MAJOR|MINOR|PATCH) "([^"]+)".*/\2/p' \
  "$CMAKE_FILE" | paste -sd.)"-g${SHA}"

COVERITY_VERSION=$(curl -s "https://scan.coverity.com/projects/${COVERITY_SCAN_PROJECT_NAME//\//-}" |
  grep  "Version:" | \
  sed -e 's/<[^>]*>//g' -e 's/Version://' -e 's/^[ \t]*//' -e 's/[ \t]*$//')

echo "Coverity Remote Version: '$COVERITY_VERSION'"
echo "Local Code Version:      '$VERSION_SHA'"

if [ "$COVERITY_VERSION" = "$VERSION_SHA" ]; then
  printf "\033[33;1mCoverity matches current branch head, no need to analyze.\033[0m\n"
  if [ "$PRECHECK" = true ]; then
    set_output "run=false"
  fi
  exit 0
fi
printf "\033[33;1mCoverity is out of date. Progressing to analyze and upload\033[0m\n"

# Verify upload is permitted
AUTH_RES=$(curl -s --form project="$COVERITY_SCAN_PROJECT_NAME" --form token="$COVERITY_SCAN_TOKEN" $SCAN_URL/api/upload_permitted)
if [ "$AUTH_RES" = "Access denied" ]; then
  printf "\033[33;1mCoverity Scan API access denied. Check COVERITY_SCAN_PROJECT_NAME and COVERITY_SCAN_TOKEN.\033[0m\n"
  if [ "$PRECHECK" = true ]; then
    set_output "run=false"
  fi
  exit 1
fi

AUTH=$(echo "$AUTH_RES" | ruby -e "require 'rubygems'; require 'json'; puts JSON[STDIN.read]['upload_permitted']")
if [ "$AUTH" = "true" ]; then
  printf "\033[33;1mCoverity Scan analysis authorized per quota.\033[0m\n"
  if [ "$PRECHECK" = true ]; then
    set_output "run=true"
    exit 0
  fi
else
  WHEN=$(echo "$AUTH_RES" | ruby -e "require 'rubygems'; require 'json'; puts JSON[STDIN.read]['next_upload_permitted_at']")
  printf "\033[33;1mCoverity Scan analysis NOT authorized until %s.\033[0m\n" "$WHEN"
  if [ "$PRECHECK" = true ]; then
    set_output "run=false"
  fi
  exit 0
fi

if command -v cov-build >/dev/null 2>&1; then
  printf "\033[33;1mUsing existing Coverity Build Tool\033[0m\n    "
  command -v cov-build
else
  if [ ! -d $TOOL_BASE ]; then
    # Download Coverity Scan Analysis Tool
    if [ ! -e "$TOOL_ARCHIVE" ]; then
      printf "\033[33;1mDownloading Coverity Scan Analysis Tool...\033[0m\n"
      curl --data-urlencode "project=$COVERITY_SCAN_PROJECT_NAME" \
         --data-urlencode "token=$COVERITY_SCAN_TOKEN" \
         "$TOOL_URL" -o "$TOOL_ARCHIVE"
      stat "$TOOL_ARCHIVE"

      curl --data-urlencode "project=$COVERITY_SCAN_PROJECT_NAME" \
         --data-urlencode "token=$COVERITY_SCAN_TOKEN" \
         --data-urlencode "md5=1" \
         "$TOOL_URL" -o "${TOOL_ARCHIVE}.md5"

      stat "${TOOL_ARCHIVE}.md5"
      cat "${TOOL_ARCHIVE}.md5"
      md5sum "$TOOL_ARCHIVE"
      echo "$(cat "${TOOL_ARCHIVE}.md5")" "$TOOL_ARCHIVE" | md5sum -c
    fi

    # Extract Coverity Scan Analysis Tool
    printf "\033[33;1mExtracting Coverity Scan Analysis Tool...\033[0m\n"
    mkdir -p $TOOL_BASE
    tar xzf "$TOOL_ARCHIVE" -C "$TOOL_BASE"
  fi

  TOOL_DIR=$(find $TOOL_BASE -type d -name 'cov-analysis*')
  export PATH="$TOOL_DIR/bin:$PATH"
fi

COVERITY_TOOL_VERSION=$(
    cov-build version 2>&1 |
    sed -n '1p' |
    sed -E 's/.* version ([^ ]+) .*/\1/' ||
    true
)

if [ -z "$COVERITY_TOOL_VERSION" ]; then
    echo "ERROR: Unable to determine Coverity Build Capture version." >&2
    exit 1
fi

printf "\033[33;1mCoverity Build Capture version: %s\033[0m\n" "$COVERITY_TOOL_VERSION"

# Configure
RESULTS_DIR="$REPO_ROOT/cov-int"
BUILD_DIR="$REPO_ROOT/build-coverity"
RESULTS_ARCHIVE=analysis-results.tgz
cd "$REPO_ROOT"
if [ ! -f "${REPO_ROOT}/${RESULTS_ARCHIVE}" ] ; then
  rm -rf "$RESULTS_DIR"
  rm -rf "$BUILD_DIR"

  cmake -S "$REPO_ROOT" \
      -B "$BUILD_DIR" \
      -G Ninja \
      -DENABLE_LTO=OFF -DENABLE_CCACHE=OFF

  # Build
  printf "\033[33;1mRunning Coverity Scan Analysis Tool...\033[0m\n"
  cd "$BUILD_DIR"
  cov-build --dir "$RESULTS_DIR" ninja -C "$BUILD_DIR" -j "$JOBS"
  if grep -q "No files were emitted." "$RESULTS_DIR/build-log.txt"; then
    echo "ERROR: Coverity did not emit any files." >&2
    exit 1
  fi
  # Collects change data for source files from the SCM.
  cov-import-scm --dir "$RESULTS_DIR" --scm git --log "$RESULTS_DIR/scm_log.txt" 2>&1

  # Upload results
  printf "\033[33;1mTarring Coverity Scan Analysis results...\033[0m\n"
  tar czf "$RESULTS_ARCHIVE" -C "$REPO_ROOT" cov-int
fi

if ! tar tf "${REPO_ROOT}/${RESULTS_ARCHIVE}" | grep '^cov-int/build-log.txt$' >/dev/null; then
  echo "ERROR: Coverity archive ${REPO_ROOT}/${RESULTS_ARCHIVE} does not contain cov-int/build-log.txt" >&2
  exit 1
fi

RESULTS_SIZE=$(stat -c '%s' "$RESULTS_ARCHIVE")
RESULTS_SIZE_MB=$((RESULTS_SIZE / 1024 / 1024))
printf "\033[33;1mCoverity results archive: %d MB\033[0m\n" "$RESULTS_SIZE_MB"

# Verify Coverity Scan script test mode
if [ "${coverity_scan_script_test_mode:-false}" = true ]; then
  printf "\033[33;1mCoverity Scan configured in script test mode. Exit.\033[0m\n"
  exit 0
fi

if [ "$RESULTS_SIZE" -gt "$((500 * 1024 * 1024))" ]; then
  printf "\033[33;1mUploading Large Coverity Scan Analysis results...\033[0m\n"
  # Step 1: Initialize build
  INIT_RESPONSE=$(curl \
      --fail \
      --show-error \
      --request POST \
      --data-urlencode "version=$VERSION_SHA" \
      --data-urlencode "description=Automated Build: $VERSION_SHA" \
      --data-urlencode "email=$COVERITY_SCAN_NOTIFICATION_EMAIL" \
      --data-urlencode "token=$COVERITY_SCAN_TOKEN" \
      --data-urlencode "file_name=$(basename "$RESULTS_ARCHIVE")" \
      "$SCAN_URL/projects/$COVERITY_SCAN_PROJECT_ID/builds/init")

  UPLOAD_URL=$(echo "$INIT_RESPONSE" | jq -r '.url')
  BUILD_ID=$(echo "$INIT_RESPONSE" | jq -r '.build_id')

  if [ -z "$UPLOAD_URL" ] || [ "$UPLOAD_URL" = "null" ]; then
      echo "ERROR: Coverity did not return an upload URL." >&2
      exit 1
  fi

  if [ -z "$BUILD_ID" ] || [ "$BUILD_ID" = "null" ]; then
      echo "ERROR: Coverity did not return a build ID." >&2
      exit 1
  fi

  # Step 2: Upload to cloud
  curl \
      --fail \
      --show-error \
      --request PUT \
      --header 'Content-Type: application/json' \
      --upload-file "$RESULTS_ARCHIVE" \
      "$UPLOAD_URL"

  # Step 3: Enqueue build
  curl \
      --fail \
      --show-error \
      --request PUT \
      --data-urlencode "token=$COVERITY_SCAN_TOKEN" \
      "$SCAN_URL/projects/$COVERITY_SCAN_PROJECT_ID/builds/$BUILD_ID/enqueue"
else
  printf "\033[33;1mUploading Coverity Scan Analysis results...\033[0m\n"
  UPLOAD_URL="https://scan.coverity.com/builds"
  response=$(curl \
    --silent --write-out "\n%{http_code}\n" \
    --form project="$COVERITY_SCAN_PROJECT_NAME" \
    --form token="$COVERITY_SCAN_TOKEN" \
    --form email=blackhole@blackhole.io \
    --form file=@$RESULTS_ARCHIVE \
    --form version="$VERSION_SHA" \
    --form description="Automated Build: $VERSION_SHA" \
    $UPLOAD_URL)
  status_code=$(echo "$response" | sed -n '$p')
  if [ "$status_code" != "200" ] && [ "$status_code" != "201" ]; then
    TEXT=$(echo "$response" | sed '$d')
    printf "\033[33;1mCoverity Scan upload failed with HTTP status code '%s': %s.\033[0m\n" "$status_code" "$TEXT"
    exit 1
  fi
fi
