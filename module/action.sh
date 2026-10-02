#!/system/bin/sh
MODDIR=${0%/*}
STATE=/data/adb/originos_0916t_demo
mkdir -p "$STATE"
chmod 0700 "$STATE"
OUT="$STATE/diagnostic.txt"
{
  date
  cat "$MODDIR/module.prop"
  getprop ro.system.build.fingerprint
  getprop init.svc.vendor.oplus.vibrator
  getenforce
  cat /sys/class/qcom-haptics/lra_frequency_hz
  cat /sys/class/qcom-haptics/t_lra_us
  grep '/dev/originos_0916t_demo/payload' /proc/1/mountinfo
  cat "$MODDIR/sepolicy.rule"
  "$MODDIR/bin/nyako-vibrator" --check-service
  timeout 10 dumpsys vibrator_manager
  logcat -d -t 300 -b all | grep -E 'avc: denied|NYAKO|vibrator|Richtap'
} > "$OUT" 2>&1
echo "Diagnostics saved: $OUT"
tail -n 15 "$STATE/service.log" 2>/dev/null
echo "Rollback: su -c sh $MODDIR/rollback.sh"
