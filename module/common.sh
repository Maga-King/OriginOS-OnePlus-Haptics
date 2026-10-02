#!/system/bin/sh
# Sourced from a module entry point. Keep state writable, payload read-only.
MODDIR=${0%/*}
RUNROOT=/dev/originos_0916t_demo/payload
enter_init_namespace() {
  current=$(readlink /proc/self/ns/mnt) || return 1
  initial=$(readlink /proc/1/ns/mnt) || return 1
  [ "$current" = "$initial" ] && return 0
  for bb in /data/adb/ksu/bin/busybox /data/adb/magisk/busybox; do
    if [ -x "$bb" ]; then
      exec "$bb" nsenter -t 1 -m "$bb" sh "$0" "$@"
    fi
  done
  echo 'Cannot enter init mount namespace.' >&2
  return 1
}
own_pid() {
  case "$1" in ''|*[!0-9]*) return 1 ;; esac
  [ -r "/proc/$1/cmdline" ] || return 1
  executable=$(tr '\000' '\n' < "/proc/$1/cmdline" | head -n 1)
  case "$executable" in "$RUNROOT/bin/mio-vibrator"|"$MODDIR/bin/mio-vibrator") return 0 ;; esac
  return 1
}
