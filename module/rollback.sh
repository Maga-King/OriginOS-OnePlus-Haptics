#!/system/bin/sh
MODDIR=${0%/*}
# shellcheck source=common.sh
. "$MODDIR/common.sh"
enter_init_namespace "$@" || exit 1
STATE=/data/adb/originos_0916t_demo
touch "$MODDIR/disable"
pid=$(cat "$STATE/pid" 2>/dev/null)
case "$pid" in ''|*[!0-9]*) pid= ;; esac
if own_pid "$pid"; then
  kill "$pid"
  count=0
  while kill -0 "$pid" 2>/dev/null && [ "$count" -lt 10 ]; do
    sleep 0.1
    count=$((count + 1))
  done
  if own_pid "$pid"; then
    kill -9 "$pid"
  fi
fi
sh "$MODDIR/self-mount.sh" stop || echo 'Unmount deferred; reboot clears the private mount.'
setprop ctl.start vendor.oplus.vibrator
echo 'Demo disabled; original vibrator service restart requested. Reboot for a clean rollback.'
