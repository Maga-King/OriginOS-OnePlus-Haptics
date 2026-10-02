#!/system/bin/sh
# Registration ordering only: no Binder/model/hash/hardware validation.
# /dev is recreated at boot, so this notification cannot survive a reboot.
count=0
while [ "$count" -lt 50 ]; do
  [ ! -f /dev/originos_0916t_demo/boot-ready ] || exit 0
  sleep 0.1
  count=$((count + 1))
done
echo 'HAL registration notification timed out; releasing init.'
exit 1
