#!/system/bin/sh
MODDIR=${0%/*}
# shellcheck source=common.sh
. "$MODDIR/common.sh"
enter_init_namespace "$@" || exit 1
mounted() { awk -v target="$RUNROOT" '$5 == target {found=1} END {exit !found}' /proc/self/mountinfo; }
mount_bind() {
  # Use the module manager's mount implementation explicitly. The DSU toybox
  # mount resolves bind-remount operands incorrectly on this device.
  for helper in /data/adb/ksu/bin/busybox /data/adb/magisk/busybox; do
    if [ -x "$helper" ]; then "$helper" mount "$@"; return $?; fi
  done
  mount "$@"
}
case "${1:-start}" in
  stop)
    if mounted; then
      umount "$RUNROOT" || exit 1
    fi
    rmdir "$RUNROOT" "${RUNROOT%/*}" 2>/dev/null || :
    exit 0
    ;;
  start)
    [ ! -f "$MODDIR/disable" ] && [ ! -f "$MODDIR/remove" ] || exit 1
    if mounted; then
      exit 0
    fi
    mkdir -p "$RUNROOT" || exit 1
    chmod 0700 "${RUNROOT%/*}"
    mount_bind -o bind "$MODDIR" "$RUNROOT" || exit 1
    # Explicit source and target keep Android mount from resolving the source
    # as /dev/block/dm-* and attempting to remount the parent filesystem.
    if ! mount_bind -o remount,bind,ro "$MODDIR" "$RUNROOT"; then
      umount "$RUNROOT"
      echo 'Read-only remount failed.' >&2
      exit 1
    fi
    echo "Self-mounted read-only: $MODDIR -> $RUNROOT"
    ;;
  *) echo 'usage: self-mount.sh start|stop' >&2; exit 2 ;;
esac
