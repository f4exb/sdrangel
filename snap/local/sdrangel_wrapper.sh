#!/bin/bash
###########################################################
# sdrangel init script
# Based on script from kde-neon extension (https://snapcraft.io/docs/kde-neon-extension),
# but we use layout in snapcraft.yaml for qt stuff, instead of env vars and qt.conf, as that didn't seem to work
###########################################################

# shellcheck disable=SC2034
START=$(date +%s.%N)

# ensure_dir_exists calls `mkdir -p` if the given path is not a directory.
# This speeds up execution time by avoiding unnecessary calls to mkdir.
#
# Usage: ensure_dir_exists <path> [<mkdir-options>]...
#
function ensure_dir_exists() {
  [ -d "$1" ] ||  mkdir -p "$@"
}

declare -A PIDS
function async_exec() {
  "$@" &
  PIDS[$!]=$*
}
function wait_for_async_execs() {
  for pid in "${!PIDS[@]}"
  do
    wait "$pid" && continue || echo "ERROR: ${PIDS[$pid]} exited abnormally with status $?"
  done
}

# shellcheck source=/dev/null
source "$SNAP_USER_DATA/.last_revision" 2>/dev/null || true
if [ "$SNAP_DESKTOP_LAST_REVISION" = "$SNAP_REVISION" ]; then
  needs_update=false
else
  needs_update=true
fi

# Set $REALHOME to the users real home directory
REALHOME=$(getent passwd $UID | cut -d ':' -f 6)

# Set config folder to local path
export XDG_CONFIG_HOME="$SNAP_USER_DATA/.config"
ensure_dir_exists "$XDG_CONFIG_HOME"
chmod 700 "$XDG_CONFIG_HOME"

# If there is no user user-dirs, don't check for the checksum
if [[ -f "$REALHOME/.config/user-dirs.dirs" ]]; then
  # If the user has modified their user-dirs settings, force an update
  if [[ -f "$XDG_CONFIG_HOME/user-dirs.dirs.md5sum" ]]; then
    if [[ "$(md5sum < "$REALHOME/.config/user-dirs.dirs")" != "$(cat "$XDG_CONFIG_HOME/user-dirs.dirs.md5sum")" ||
          ( -f "$XDG_CONFIG_HOME/user-dirs.locale.md5sum" &&
            "$(md5sum < "$REALHOME/.config/user-dirs.locale")" != "$(cat "$XDG_CONFIG_HOME/user-dirs.locale.md5sum")" ) ]]; then
      needs_update=true
    fi
  else
    # shellcheck disable=SC2034
    needs_update=true
  fi
fi

if [ "$SNAP_ARCH" = "amd64" ]; then
  ARCH="x86_64-linux-gnu"
elif [ "$SNAP_ARCH" = "armhf" ]; then
  ARCH="arm-linux-gnueabihf"
elif [ "$SNAP_ARCH" = "arm64" ]; then
  ARCH="aarch64-linux-gnu"
elif [ "$SNAP_ARCH" = "ppc64el" ]; then
  ARCH="powerpc64le-linux-gnu"
else
  ARCH="$SNAP_ARCH-linux-gnu"
fi

export SNAP_LAUNCHER_ARCH_TRIPLET="$ARCH"

# Don't LD_PRELOAD bindtextdomain for classic snaps
if ! grep -qs "^\s*confinement:\s*classic\s*" "$SNAP/meta/snap.yaml"; then
  if [ -f "$SNAP/lib/$ARCH/bindtextdomain.so" ]; then
    export LD_PRELOAD="$LD_PRELOAD:$SNAP/\$LIB/bindtextdomain.so"
  elif [ -f "$SNAP/gnome-platform/lib/$ARCH/bindtextdomain.so" ]; then
    export LD_PRELOAD="$LD_PRELOAD:$SNAP/gnome-platform/\$LIB/bindtextdomain.so"
  fi
fi
###########################################################
# Launcher common exports for any desktop app
# This is not used with the gnome extension for
# core22 and later, please see
# https://github.com/snapcore/snapcraft-desktop-integration
###########################################################

# Note: We avoid using `eval` because we don't want to expand variable names
#       in paths. For example: LD_LIBRARY_PATH paths might contain `$LIB`.
function prepend_dir() {
  local -n var="$1"
  local dir="$2"
  # We can't check if the dir exists when the dir contains variables
  if [[ "$dir" == *"\$"*  || -d "$dir" ]]; then
    export "${!var}=${dir}${var:+:$var}"
  fi
}

function append_dir() {
  local -n var="$1"
  local dir="$2"
  # We can't check if the dir exists when the dir contains variables
  if [[ "$dir" == *"\$"*  || -d "$dir" ]]; then
    export "${!var}=${var:+$var:}${dir}"
  fi
}

function can_open_file() {
  [ -f "$1" ] && [ -r "$1" ]
}

function update_xdg_dirs_values() {
  local save_initial_values=false
  local XDG_DIRS="DOCUMENTS DESKTOP DOWNLOAD MUSIC PICTURES VIDEOS PUBLICSHARE TEMPLATES"
  unset XDG_SPECIAL_DIRS_PATHS

  if [ -f "${XDG_CONFIG_HOME:-$HOME/.config}/user-dirs.dirs" ]; then
    # shellcheck source=/dev/null
    source "${XDG_CONFIG_HOME:-$HOME/.config}/user-dirs.dirs"
  fi

  if [ -z ${XDG_SPECIAL_DIRS+x} ]; then
    save_initial_values=true
  fi

  for d in $XDG_DIRS; do
    var="XDG_${d}_DIR"
    value="${!var}"

    if [ "$save_initial_values" = true ]; then
      XDG_SPECIAL_DIRS+=("$var")
      XDG_SPECIAL_DIRS_INITIAL_PATHS+=("$value")
    fi

    XDG_SPECIAL_DIRS_PATHS+=("$value")
  done
}

function is_subpath() {
  dir="$(realpath "$1")"
  parent="$(realpath "$2")"
  [ "${dir##"${parent}"/}" != "${dir}" ] && return 0 || return 1
}

# We can't use this snap, as it's missing QtCharts, Gamepad and TextToSpeech
#if ! snapctl is-connected "kf5-5-108-qt-5-15-10-core22"; then
#  echo "ERROR: not connected to the kf5-5-108-qt-5-15-10-core22 content interface."
#  exit 1
#fi

SNAP_DESKTOP_RUNTIME=$SNAP

append_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/lib/$ARCH"
append_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH"
append_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib"
append_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/lib"
append_dir PATH "$SNAP_DESKTOP_RUNTIME/usr/bin"

# XKB config
export XKB_CONFIG_ROOT="$SNAP_DESKTOP_RUNTIME/usr/share/X11/xkb"

# Give XOpenIM a chance to locate locale data.
# This is required for text input to work in SDL2 games.
export XLOCALEDIR="$SNAP_DESKTOP_RUNTIME/usr/share/X11/locale"

# Set XCursors path
export XCURSOR_PATH="$SNAP_DESKTOP_RUNTIME/usr/share/icons"
prepend_dir XCURSOR_PATH "$SNAP/data-dir/icons"

# Mesa Libs for OpenGL support
append_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/mesa"
append_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/mesa-egl"

# Tell libGL and libva where to find the drivers
export LIBGL_DRIVERS_PATH="$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/dri"
append_dir LD_LIBRARY_PATH "$LIBGL_DRIVERS_PATH"
append_dir LIBVA_DRIVERS_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/dri"

# Set where the VDPAU drivers are located
export VDPAU_DRIVER_PATH="/usr/lib/$ARCH/vdpau/"
if [ -e "/var/lib/snapd/lib/gl/vdpau/libvdpau_nvidia.so" ]; then
  export VDPAU_DRIVER_PATH="/var/lib/snapd/lib/gl/vdpau"
  if [ "$__NV_PRIME_RENDER_OFFLOAD" = 1 ]; then
    # Prevent picking VA-API (Intel/AMD) over NVIDIA VDPAU
    # https://download.nvidia.com/XFree86/Linux-x86_64/510.54/README/primerenderoffload.html#configureapplications
    unset LIBVA_DRIVERS_PATH
  fi
fi

# Workaround in snapd for proprietary nVidia drivers mounts the drivers in
# /var/lib/snapd/lib/gl that needs to be in LD_LIBRARY_PATH
# Without that OpenGL using apps do not work with the nVidia drivers.
# Ref.: https://bugs.launchpad.net/snappy/+bug/1588192
append_dir LD_LIBRARY_PATH "/var/lib/snapd/lib/gl"
append_dir LD_LIBRARY_PATH "/var/lib/snapd/lib/gl/vdpau"

# Unity7 export (workaround for https://launchpad.net/bugs/1638405)
append_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/libunity"

# Pulseaudio export
append_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/pulseaudio"

# EGL vendor files on glvnd enabled systems
prepend_dir __EGL_VENDOR_LIBRARY_DIRS "/var/lib/snapd/lib/glvnd/egl_vendor.d"
append_dir __EGL_VENDOR_LIBRARY_DIRS "$SNAP_DESKTOP_RUNTIME/usr/share/glvnd/egl_vendor.d"

# Tell GStreamer where to find its plugins
export GST_PLUGIN_PATH="$SNAP/usr/lib/$ARCH/gstreamer-1.0"
export GST_PLUGIN_SYSTEM_PATH="$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/gstreamer-1.0"
# gst plugin scanner doesn't install in the correct path: https://github.com/ubuntu/snapcraft-desktop-helpers/issues/43
export GST_PLUGIN_SCANNER="$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner"

# XDG Config
prepend_dir XDG_CONFIG_DIRS "$SNAP_DESKTOP_RUNTIME/etc/xdg"
prepend_dir XDG_CONFIG_DIRS "$SNAP/etc/xdg"

# Define snaps' own data dir
prepend_dir XDG_DATA_DIRS "$SNAP_DESKTOP_RUNTIME/usr/share"
prepend_dir XDG_DATA_DIRS "$SNAP/usr/share"
prepend_dir XDG_DATA_DIRS "$SNAP/share"
prepend_dir XDG_DATA_DIRS "$SNAP/data-dir"
prepend_dir XDG_DATA_DIRS "$SNAP_USER_DATA"

# Set XDG_DATA_HOME to local path
export XDG_DATA_HOME="$SNAP_USER_DATA/.local/share"
ensure_dir_exists "$XDG_DATA_HOME"

# Workaround for GLib < 2.53.2 not searching for schemas in $XDG_DATA_HOME:
#   https://bugzilla.gnome.org/show_bug.cgi?id=741335
prepend_dir XDG_DATA_DIRS "$XDG_DATA_HOME"

# Set cache folder to local path
export XDG_CACHE_HOME="$SNAP_USER_COMMON/.cache"
if [[ -d "$SNAP_USER_DATA/.cache" && ! -e "$XDG_CACHE_HOME" ]]; then
  # the .cache directory used to be stored under $SNAP_USER_DATA, migrate it
  mv "$SNAP_USER_DATA/.cache" "$SNAP_USER_COMMON/"
fi
ensure_dir_exists "$XDG_CACHE_HOME"

# Create $XDG_RUNTIME_DIR if not exists (to be removed when LP: #1656340 is fixed)
# shellcheck disable=SC2174
ensure_dir_exists "$XDG_RUNTIME_DIR" -m 700

# Ensure the app finds locale definitions (requires locales-all to be installed)
append_dir LOCPATH "$SNAP_DESKTOP_RUNTIME/usr/lib/locale"

# If any, keep track of where XDG dirs were so we can potentially migrate the content later
update_xdg_dirs_values

# Setup user-dirs.* or run xdg-user-dirs-update if needed
needs_xdg_update=false
needs_xdg_reload=false
needs_xdg_links=false

if [ "$HOME" != "$SNAP_USER_DATA" ] && ! is_subpath "$XDG_CONFIG_HOME" "$HOME"; then
  for f in user-dirs.dirs user-dirs.locale; do
    if [ -f "$HOME/.config/$f" ]; then
      mv "$HOME/.config/$f" "$XDG_CONFIG_HOME"
      needs_xdg_reload=true
    fi
  done
fi

if can_open_file "$REALHOME/.config/user-dirs.dirs"; then
  # shellcheck disable=SC2154
  if [ "$needs_update" = true ] || [ "$needs_xdg_reload" = true ]; then
    sed "/^#/!s#\$HOME#${REALHOME}#g" "$REALHOME/.config/user-dirs.dirs" > "$XDG_CONFIG_HOME/user-dirs.dirs"
    md5sum < "$REALHOME/.config/user-dirs.dirs" > "$XDG_CONFIG_HOME/user-dirs.dirs.md5sum"
    # It's possible user-dirs.dirs exists when user-dirs.locale doesn't. This
    # simply means the user opted to never ask to translate their user dirs
    if can_open_file "$REALHOME/.config/user-dirs.locale"; then
      cp -a "$REALHOME/.config/user-dirs.locale" "$XDG_CONFIG_HOME"
      md5sum < "$REALHOME/.config/user-dirs.locale" > "$XDG_CONFIG_HOME/user-dirs.locale.md5sum"
    elif [ -f "$XDG_CONFIG_HOME/user-dirs.locale.md5sum" ]; then
      rm "$XDG_CONFIG_HOME/user-dirs.locale.md5sum"
    fi
    needs_xdg_reload=true
  fi
else
  needs_xdg_update=true
  needs_xdg_links=true
fi

if [ $needs_xdg_reload = true ]; then
  update_xdg_dirs_values
  needs_xdg_reload=false
fi

# Check if we can actually read the contents of each xdg dir
for ((i = 0; i < ${#XDG_SPECIAL_DIRS_PATHS[@]}; i++)); do
  if ! can_open_file "${XDG_SPECIAL_DIRS_PATHS[$i]}"; then
    needs_xdg_update=true
    needs_xdg_links=true
    break
  fi
done

# If needs XDG update and xdg-user-dirs-update exists in $PATH, run it
if [ "$needs_xdg_update" = true ] && command -v xdg-user-dirs-update >/dev/null; then
  xdg-user-dirs-update
  update_xdg_dirs_values
fi

# Create links for user-dirs.dirs
if [ "$needs_xdg_links" = true ]; then
  for ((i = 0; i < ${#XDG_SPECIAL_DIRS_PATHS[@]}; i++)); do
    b="$(realpath "${XDG_SPECIAL_DIRS_PATHS[$i]}" --relative-to="$HOME" 2>/dev/null)"
    if [[ -n "$b" && "$b" != "." && -e "$REALHOME/$b" ]]; then
      [ -d "$HOME/$b" ] && rmdir "$HOME/$b" 2> /dev/null
      [ ! -e "$HOME/$b" ] && ln -s "$REALHOME/$b" "$HOME/$b"
    fi
  done
else
  # If we aren't creating new links, check if we have content saved in old locations and move it
  for ((i = 0; i < ${#XDG_SPECIAL_DIRS[@]}; i++)); do
    old="${XDG_SPECIAL_DIRS_INITIAL_PATHS[$i]}"
    new="${XDG_SPECIAL_DIRS_PATHS[$i]}"
    if [ -L "$old" ] && [ -d "$new" ] && [ "$(readlink "$old" 2>/dev/null)" != "$new" ] &&
         (is_subpath "$old" "$SNAP_USER_DATA" || is_subpath "$old" "$SNAP_USER_COMMON"); then
      mv -vn "$old"/* "$new"/ 2>/dev/null
    elif [ -d "$old" ] && [ -d "$new" ] && [ "$old" != "$new" ] &&
         (is_subpath "$old" "$SNAP_USER_DATA" || is_subpath "$old" "$SNAP_USER_COMMON"); then
      mv -vn "$old"/* "$new"/ 2>/dev/null
    fi
  done
fi

# If detect wayland server socket, then set environment so applications prefer
# wayland, and setup compat symlink (until we use user mounts. Remember,
# XDG_RUNTIME_DIR is /run/user/<uid>/snap.$SNAP so look in the parent directory
# for the socket. For details:
# https://forum.snapcraft.io/t/wayland-dconf-and-xdg-runtime-dir/186/10
# Applications that don't support wayland natively may define DISABLE_WAYLAND
# (to any non-empty value) to skip that logic entirely.
wayland_available=false
if [[ -n "$XDG_RUNTIME_DIR" && -z "$DISABLE_WAYLAND" ]]; then
    wdisplay="wayland-0"
    if [ -n "$WAYLAND_DISPLAY" ]; then
        wdisplay="$WAYLAND_DISPLAY"
    fi
    wayland_sockpath="$XDG_RUNTIME_DIR/../$wdisplay"
    wayland_snappath="$XDG_RUNTIME_DIR/$wdisplay"
    if [ -S "$wayland_sockpath" ]; then
        # if running under wayland, use it
        #export WAYLAND_DEBUG=1
        # shellcheck disable=SC2034
        wayland_available=true
        # create the compat symlink for now
        if [ ! -e "$wayland_snappath" ]; then
            ln -s "$wayland_sockpath" "$wayland_snappath"
        fi
    fi
fi

# Make PulseAudio socket available inside the snap-specific $XDG_RUNTIME_DIR
if [ -n "$XDG_RUNTIME_DIR" ]; then
    pulsenative="pulse/native"
    pulseaudio_sockpath="$XDG_RUNTIME_DIR/../$pulsenative"
    if [ -S "$pulseaudio_sockpath" ]; then
        export PULSE_SERVER="unix:${pulseaudio_sockpath}"
    fi
fi

# GI repository
prepend_dir GI_TYPELIB_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/girepository-1.0"
prepend_dir GI_TYPELIB_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/girepository-1.0"
prepend_dir GI_TYPELIB_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/gjs/girepository-1.0"
prepend_dir GI_TYPELIB_PATH "$SNAP/usr/lib/$ARCH/girepository-1.0"
prepend_dir GI_TYPELIB_PATH "$SNAP/usr/lib/girepository-1.0"
prepend_dir GI_TYPELIB_PATH "$SNAP/usr/lib/gjs/girepository-1.0"

# Keep an array of data dirs, for looping through them
IFS=':' read -r -a data_dirs_array <<< "$XDG_DATA_DIRS"

# Font Config and themes
export FONTCONFIG_PATH="$SNAP_DESKTOP_RUNTIME/etc/fonts"
export FONTCONFIG_FILE="$SNAP_DESKTOP_RUNTIME/etc/fonts/fonts.conf"

function make_user_fontconfig {
  echo "<fontconfig>"
  if [ -d "$REALHOME/.local/share/fonts" ]; then
    echo "  <dir>$REALHOME/.local/share/fonts</dir>"
  fi
  if [ -d "$REALHOME/.fonts" ]; then
    echo "  <dir>$REALHOME/.fonts</dir>"
  fi
  for ((i = 0; i < ${#data_dirs_array[@]}; i++)); do
    if [ -d "${data_dirs_array[$i]}/fonts" ]; then
      echo "  <dir>${data_dirs_array[$i]}/fonts</dir>"
    fi
  done
  echo '  <include ignore_missing="yes">conf.d</include>'
  # We need to include a user-writable cachedir first to support
  # caching of per-user fonts.
  echo '  <cachedir prefix="xdg">fontconfig</cachedir>'
  echo "  <cachedir>$SNAP_COMMON/fontconfig</cachedir>"
  echo "</fontconfig>"
}

if [ "$needs_update" = true ]; then
  rm -rf "$XDG_DATA_HOME"/{fontconfig,fonts,fonts-*,themes,.themes}

  # This fontconfig fragment is installed in a location that is
  # included by the system fontconfig configuration: namely the
  # etc/fonts/conf.d/50-user.conf file.
  ensure_dir_exists "$XDG_CONFIG_HOME/fontconfig"
  async_exec make_user_fontconfig > "$XDG_CONFIG_HOME/fontconfig/fonts.conf"

  # the themes symlink are needed for GTK 3.18 when the prefix isn't changed
  # GTK 3.20 looks into XDG_DATA_DIRS which has connected themes.
  if [ -d "$SNAP/data-dir/themes" ]; then
    ln -sf "$SNAP/data-dir/themes" "$XDG_DATA_HOME"
    ln -sfn "$SNAP/data-dir/themes" "$SNAP_USER_DATA/.themes"
  elif [ -d "$SNAP_DESKTOP_RUNTIME/usr/share/themes" ]; then
    ln -sf "$SNAP_DESKTOP_RUNTIME/usr/share/themes" "$XDG_DATA_HOME"
    ln -sfn "$SNAP_DESKTOP_RUNTIME/usr/share/themes" "$SNAP_USER_DATA/.themes"
  fi
fi

# Build mime.cache
# needed for gtk and qt icon
if [ "$needs_update" = true ]; then
  rm -rf "$XDG_DATA_HOME/mime"
  if [ ! -f "$SNAP_DESKTOP_RUNTIME/usr/share/mime/mime.cache" ]; then
    if command -v update-mime-database >/dev/null; then
      cp --preserve=timestamps -dR "$SNAP_DESKTOP_RUNTIME/usr/share/mime" "$XDG_DATA_HOME"
      async_exec update-mime-database "$XDG_DATA_HOME/mime"
    fi
  fi
fi

# Gio modules and cache (including gsettings module)
export GIO_MODULE_DIR="$XDG_CACHE_HOME/gio-modules"
function compile_giomodules {
  if [ -f "$1/glib-2.0/gio-querymodules" ]; then
    rm -rf "$GIO_MODULE_DIR"
    ensure_dir_exists "$GIO_MODULE_DIR"
    ln -s "$1"/gio/modules/*.so "$GIO_MODULE_DIR"
    "$1/glib-2.0/gio-querymodules" "$GIO_MODULE_DIR"
  fi
}
if [ "$needs_update" = true ]; then
  async_exec compile_giomodules "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH"
fi

# Setup compiled gsettings schema
GS_SCHEMA_DIR="$XDG_DATA_HOME/glib-2.0/schemas"
function compile_schemas {
  if [ -f "$1" ]; then
    rm -rf "$GS_SCHEMA_DIR"
    ensure_dir_exists "$GS_SCHEMA_DIR"
    for ((i = 0; i < ${#data_dirs_array[@]}; i++)); do
      schema_dir="${data_dirs_array[$i]}/glib-2.0/schemas"
      if [ -f "$schema_dir/gschemas.compiled" ]; then
        # This directory already has compiled schemas
        continue
      fi
      if [ -n "$(ls -A "$schema_dir"/*.xml 2>/dev/null)" ]; then
        ln -s "$schema_dir"/*.xml "$GS_SCHEMA_DIR"
      fi
      if [ -n "$(ls -A "$schema_dir"/*.override 2>/dev/null)" ]; then
        ln -s "$schema_dir"/*.override "$GS_SCHEMA_DIR"
      fi
    done
    # Only compile schemas if we copied anything
    if [ -n "$(ls -A "$GS_SCHEMA_DIR"/*.xml "$GS_SCHEMA_DIR"/*.override 2>/dev/null)" ]; then
      "$1" "$GS_SCHEMA_DIR"
    fi
  fi
}
if [ "$needs_update" = true ]; then
  async_exec compile_schemas "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/glib-2.0/glib-compile-schemas"
fi

# Enable gsettings user changes
# symlink the dconf file if home plug is connected for read
DCONF_DEST_USER_DIR="$SNAP_USER_DATA/.config/dconf"
if [ ! -f "$DCONF_DEST_USER_DIR/user" ]; then
  if [ -f "$REALHOME/.config/dconf/user" ]; then
    ensure_dir_exists "$DCONF_DEST_USER_DIR"
    ln -s "$REALHOME/.config/dconf/user" "$DCONF_DEST_USER_DIR"
  fi
fi
# symlink the runtime dconf file as well
if [ -r "$XDG_RUNTIME_DIR/../dconf/user" ]; then
  ensure_dir_exists "$XDG_RUNTIME_DIR/dconf"
  ln -sf "../../dconf/user" "$XDG_RUNTIME_DIR/dconf/user"
fi

# Testability support
append_dir LD_LIBRARY_PATH "$SNAP/testability"
append_dir LD_LIBRARY_PATH "$SNAP/testability/$ARCH"
append_dir LD_LIBRARY_PATH "$SNAP/testability/$ARCH/mesa"

# Gdk-pixbuf loaders
export GDK_PIXBUF_MODULE_FILE="$XDG_CACHE_HOME/gdk-pixbuf-loaders.cache"
export GDK_PIXBUF_MODULEDIR="$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/gdk-pixbuf-2.0/2.10.0/loaders"
if [ "$needs_update" = true ] || [ ! -f "$GDK_PIXBUF_MODULE_FILE" ]; then
  rm -f "$GDK_PIXBUF_MODULE_FILE"
  if [ -f "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/gdk-pixbuf-2.0/gdk-pixbuf-query-loaders" ]; then
    async_exec "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/gdk-pixbuf-2.0/gdk-pixbuf-query-loaders" > "$GDK_PIXBUF_MODULE_FILE"
  fi
fi

# Icon themes cache
if [ "$needs_update" = true ]; then
  rm -rf "$XDG_DATA_HOME/icons"
  ensure_dir_exists "$XDG_DATA_HOME/icons"
  for ((i = 0; i < ${#data_dirs_array[@]}; i++)); do
    # The runtime and theme content snaps should already contain icon caches
    # so we skip them to optimise app start time.
    if [[ "${data_dirs_array[$i]}" == "$SNAP/data-dir" || "${data_dirs_array[$i]}" == "$SNAP_DESKTOP_RUNTIME/"* ]]; then
      continue
    fi
    for theme in "${data_dirs_array[$i]}"/icons/*; do
      if [ -f "$theme/index.theme" ] && [ ! -f "$theme/icon-theme.cache" ]; then
        theme_dir="$XDG_DATA_HOME/icons/$(basename "$theme")"
        if [ ! -d "$theme_dir" ]; then
          ensure_dir_exists "$theme_dir"
          ln -s "$theme"/* "$theme_dir"
          if [ -f "$SNAP_DESKTOP_RUNTIME/usr/sbin/update-icon-caches" ]; then
            async_exec "$SNAP_DESKTOP_RUNTIME/usr/sbin/update-icon-caches" "$theme_dir"
          elif [ -f "$SNAP_DESKTOP_RUNTIME/usr/sbin/update-icon-cache.gtk2" ]; then
            async_exec "$SNAP_DESKTOP_RUNTIME/usr/sbin/update-icon-cache.gtk2" "$theme_dir"
          fi
        fi
      fi
    done
  done
fi

# GTK theme and behavior modifier
# Those can impact the theme engine used by Qt as well
gtk_configs=(gtk-3.0/settings.ini gtk-3.0/bookmarks gtk-2.0/gtkfilechooser.ini)
for f in "${gtk_configs[@]}"; do
  dest="$XDG_CONFIG_HOME/$f"
  if [ ! -L "$dest" ]; then
    ensure_dir_exists "$(dirname "$dest")"
    ln -s "$REALHOME/.config/$f" "$dest"
  fi
done

# create symbolic link to ibus socket path for ibus to look up its socket files
# (see comments #3 and #6 on https://launchpad.net/bugs/1580463)
IBUS_CONFIG_PATH="$XDG_CONFIG_HOME/ibus"
ensure_dir_exists "$IBUS_CONFIG_PATH"
[ -d "$IBUS_CONFIG_PATH/bus" ] && rm -rf "$IBUS_CONFIG_PATH/bus"
ln -sfn "$REALHOME/.config/ibus/bus" "$IBUS_CONFIG_PATH"

# Set libgweather path
export LIBGWEATHER_LOCATIONS_PATH="$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/libgweather-4/Locations.bin"

# Set libthai dict path
export LIBTHAI_DICTDIR="$SNAP_DESKTOP_RUNTIME/usr/share/libthai/"
###################################
# KDE NEON launcher specific part #
###################################

# Add paths for games
append_dir PATH "$SNAP/usr/games"
append_dir PATH "$SNAP_DESKTOP_RUNTIME/usr/games"

# Qt Libs
prepend_dir LD_LIBRARY_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/qt5/libs"

# We use layout in snapcraft.yaml to overlay at5 directories, rather than env vars, as they don't seem to work for WebEngine
# Add QT_PLUGIN_PATH (Qt Modules).
#append_dir QT_PLUGIN_PATH "$SNAP/usr/lib/$ARCH/qt5/plugins"
#append_dir QT_PLUGIN_PATH "$SNAP/usr/lib/$ARCH"
#append_dir QT_PLUGIN_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/qt5/plugins"
#append_dir QT_PLUGIN_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/"
# And QML2_IMPORT_PATH (Qt Modules).
#append_dir QML2_IMPORT_PATH "$SNAP/usr/lib/$ARCH/qt5/qml"
#append_dir QML2_IMPORT_PATH "$SNAP/lib/$ARCH"
#append_dir QML2_IMPORT_PATH "$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/qt5/qml"
#append_dir QML2_IMPORT_PATH "$SNAP_DESKTOP_RUNTIME/lib/$ARCH"

# Fix locating the QtWebEngineProcess executable
#export QTWEBENGINEPROCESS_PATH="$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/qt5/libexec/QtWebEngineProcess"

# Removes Qt warning: Could not find a location
# of the system Compose files
#export QTCOMPOSE="$SNAP_DESKTOP_RUNTIME/usr/share/X11/locale"
#export QT_XKB_CONFIG_ROOT="/usr/share/X11/xkb"

# KIO specific
# Directly fork slaves.
export KDE_FORK_SLAVES=1
# Path to KIO slaves.
#export KF5_LIBEXEC_DIR="$SNAP_DESKTOP_RUNTIME/usr/lib/$ARCH/libexec/kf5"

# Add path to VLC plugins
export VLC_PLUGIN_PATH="$SNAP_DESKTOP_RUNTIME/usr/lib/x86_64-linux-gnu/vlc/plugins"

# Ensure QtChooser behaves.
#export QTCHOOSER_NO_GLOBAL_DIR=1
#export QT_SELECT=5
# qtchooser hardcodes reference paths, we'll need to rewrite them properly
#ensure_dir_exists "$XDG_CONFIG_HOME/qtchooser"
#echo "$SNAP/usr/lib/qt5/bin" > "$XDG_CONFIG_HOME/qtchooser/5.conf"
#echo "$SNAP/usr/lib/$ARCH" >> "$XDG_CONFIG_HOME/qtchooser/5.conf"
#echo "$SNAP/usr/lib/qt5/bin" > "$XDG_CONFIG_HOME/qtchooser/default.conf"
#echo "$SNAP/usr/lib/$ARCH" >> "$XDG_CONFIG_HOME/qtchooser/default.conf"

# This relies on qtbase patch
# 0001-let-qlibraryinfo-fall-back-to-locate-qt.conf-via-XDG.patch
# to make QLibraryInfo look in XDG_* locations for qt.conf. The paths configured
# here are applied to everything that uses QLibraryInfo as final fallback and
# has no XDG_* fallback before that. Currently the most interesting offender
# is QtWebEngine which will not work unless the Data path is correctly set.
#cat << EOF > "$XDG_CONFIG_HOME/qt.conf"
#[Paths]
#Data = $SNAP_DESKTOP_RUNTIME/usr/share/qt5/
#Translations = $SNAP_DESKTOP_RUNTIME/usr/share/qt5/translations
#EOF

if [ -e "$SNAP_DESKTOP_RUNTIME/usr/share/i18n" ]; then
    export I18NPATH="$SNAP_DESKTOP_RUNTIME/usr/share/i18n"
    locpath="$XDG_DATA_HOME/locale"
    ensure_dir_exists "$locpath"
    export LOCPATH="$locpath:/usr/lib/locale"
    LC_ALL=C.UTF-8 async_exec "$SNAP/bin/locale-gen"
fi
###########################################################
# Mark update and exec binary
# This is not used with the gnome extension for
# core22 and later, please see
# https://github.com/snapcore/snapcraft-desktop-integration
###########################################################

# shellcheck disable=SC2154
[ "$needs_update" = true ] && echo "SNAP_DESKTOP_LAST_REVISION=$SNAP_REVISION" > "$SNAP_USER_DATA/.last_revision"

wait_for_async_execs

if [ -n "$SNAP_DESKTOP_DEBUG" ]; then
  echo "desktop-launch elapsed time: $(date +%s.%N --date="$START seconds ago")"
  echo "Now running: exec $SNAP/usr/local/bin/sdrangel $@"
fi

# This doesn't appear to work - so we use layout in snapcraft.yaml
#export QTWEBENGINE_RESOURCES_PATH=$SNAP/usr/share/qt5/resources
#export QTWEBENGINE_LOCALES_PATH=$SNAP/usr/share/qt5/translations

# Without this, we get:
# Qt: Session management error: None of the authentication protocols specified are supported
unset SESSION_MANAGER

# Check we have USB access
if ! snapctl is-connected raw-usb ; then
    notify-send -u critical -a SDRangel "SDRangel does not have USB access." "To enable, run:\n\nsnap connect sdrangel:raw-usb"
fi

# Check we have SSE 4.2
if ! cat /proc/cpuinfo | grep sse4_2 > /dev/null ; then
    notify-send -u critical -a SDRangel "SSE 4.2 support not detected in your CPU." "SDRangel may crash."
fi

# Safe to enable soapy, as we only include soapy remote
exec $SNAP/opt/install/sdrangel/bin/sdrangel --soapy "$@"
