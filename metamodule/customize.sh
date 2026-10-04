#!/system/bin/sh
ui_print '- Nyako haptics: existing metamodule mounts ODM HAL and /system/lib64 plugin.'
ui_print '- No self-mount, HAL supervisor or early initrc injection.'
ui_print '- Disable before returning to stock OnePlus. Full DSU reboot required.'
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/system/odm/bin/hw/vendor.oplus.hardware.vibrator-service" 0 0 0755
set_perm "$MODPATH/service.sh" 0 0 0755
chcon -R u:object_r:vendor_file:s0 "$MODPATH/system/odm"
chcon -R u:object_r:vendor_configs_file:s0 "$MODPATH/system/odm/etc"
chcon u:object_r:hal_vibrator_default_exec:s0 "$MODPATH/system/odm/bin/hw/vendor.oplus.hardware.vibrator-service"
chcon -R u:object_r:system_lib_file:s0 "$MODPATH/system/lib64"
rm -f /data/local/nyako/state/manifest.json /data/local/nyako/state/scopes
