#!/system/bin/sh
ui_print '- OriginOS / OnePlus 0916T: Demo 0.2.0 self-mount'
ui_print '- Removed demo 20% cap; 49 strength steps cover 20% to 100% waveform amplitude.'
ui_print '- KernelSU: register HAL before system_server caches supported effects; reboot needed.'
ui_print '- Keyboard141-150 now uses official effect2; heavy click uses official effect109.'
ui_print '- HE1 and complete single-packet HE2: finite loops, stop/preemption and callbacks.'
ui_print '- Process exit restores the original HAL without changing module enable state.'
ui_print '- Added supplied ring waveforms and two-hit injury; 691 is an explicit experimental fallback.'
ui_print '- All donor IDs have output; 3066/3103/26007 use documented compatibility effects.'
ui_print '- Full HE/FMQ and performance remain unfinished; approved keyboard/AI unchanged.'
ui_print '- No ODM/VENDOR partition files or calibration data will be overwritten.'
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm_recursive "$MODPATH/bin" 0 0 0755 0755
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/action.sh" 0 0 0755
set_perm "$MODPATH/rollback.sh" 0 0 0755
set_perm "$MODPATH/uninstall.sh" 0 0 0755
set_perm "$MODPATH/self-mount.sh" 0 0 0755
set_perm "$MODPATH/wait-early.sh" 0 0 0755
if [ "${KSU:-false}" = true ]; then
  cp "$MODPATH/policy/ksu.rule" "$MODPATH/sepolicy.rule" || abort 'Cannot prepare KernelSU policy.'
  ui_print '- KernelSU detected; selecting ksu SELinux domain.'
else
  cp "$MODPATH/policy/magisk.rule" "$MODPATH/sepolicy.rule" || abort 'Cannot prepare Magisk policy.'
fi
ui_print '- Built-in read-only bind mount; no metamodule required.'
ui_print '- Installation prepared. Reboot to activate. See README_CN.txt for rollback.'
