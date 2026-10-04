#!/system/bin/sh
MODDIR=${0%/*}
# shellcheck source=common.sh
. "$MODDIR/common.sh"
enter_init_namespace "$@" || exit 1
STATE=/data/adb/originos_0916t_demo
mkdir -p "$STATE"
chmod 0700 "$STATE"
previous=$(cat "$STATE/pid" 2>/dev/null)
if own_pid "$previous"; then echo 'Demo already running.'; exit 0; fi
# Keep one previous log, with bounded total storage across reboots.
[ ! -f "$STATE/service.log" ] || mv -f "$STATE/service.log" "$STATE/service.previous.log"
exec >> "$STATE/service.log" 2>&1
echo 'Starting OriginOS 0916T demo 0.3.0-demo2 PCM concurrency self-mount'
date
if [ -f "$MODDIR/disable" ] || [ -f "$MODDIR/remove" ]; then
  # A crash can create disable before ksud regenerates its cached initrc.
  # In that case the early action already stopped the original: restore it.
  [ "${1:-}" != --init-early ] || setprop ctl.start vendor.oplus.vibrator
  exit 0
fi
rm -f "$STATE/ready" "$STATE/pid"
ready="$STATE/ready"
[ "${1:-}" != --init-early ] || ready=/dev/originos_0916t_demo/boot-ready
child=
owned=0
[ "${1:-}" != --init-early ] || owned=1
restore() {
  trap - EXIT INT TERM
  if own_pid "$child"; then
    kill "$child"
    sleep 1
    own_pid "$child" && kill -9 "$child"
  fi
  sh "$MODDIR/self-mount.sh" stop || echo 'Runtime unmount failed; reboot will clear it.'
  if [ "$owned" = 1 ]; then
    setprop ctl.start vendor.oplus.vibrator
    echo 'Requested original service restart.'
  fi
  rm -f "$STATE/ready" "$STATE/pid" "$ready"
}
trap restore EXIT INT TERM
sh "$MODDIR/self-mount.sh" start || {
  echo 'Self-mount failed; restoring original if init stopped it.'
  exit 1
}
owned=1
if [ "${1:-}" != --init-early ]; then
  setprop ctl.stop vendor.oplus.vibrator
  count=0
  while [ "$(getprop init.svc.vendor.oplus.vibrator)" != stopped ]; do
    count=$((count + 1))
    [ "$count" -le 30 ] || { echo 'Original service did not stop; restoring.'; exit 1; }
    sleep 0.1
  done
fi
# In early mode init.rc already stopped the old service. Do not issue a
# synchronous property request while init is waiting for our registration.
"$RUNROOT/bin/nyako-vibrator" --serve "$RUNROOT/waves" "$ready" &
child=$!
echo "$child" > "$STATE/pid"
echo "Demo process launched: $child"
# Monitor disable/removal and process death. Never restart the demo in a crash loop.
while kill -0 "$child" 2>/dev/null; do
  [ ! -f "$MODDIR/disable" ] && [ ! -f "$MODDIR/remove" ] || { echo 'Disabled or removed; restoring.'; exit 0; }
  [ "$(getprop init.svc.vendor.oplus.vibrator)" = stopped ] || { echo 'Original service restarted; relinquishing device.'; exit 1; }
  bytes=$(wc -c < "$STATE/service.log")
  if [ "$bytes" -gt 2097152 ]; then
    tail -c 262144 "$STATE/service.log" > "$STATE/service.tail.log"
    : > "$STATE/service.log"
    echo 'Log rotated; recent history in service.tail.log.'
  fi
  sleep 2
done
echo 'Demo exited; restoring original for this boot. Module enable state unchanged.'
exit 1
