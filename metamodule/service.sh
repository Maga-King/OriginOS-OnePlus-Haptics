#!/system/bin/sh
# Mounting is handled by the user's metamodule. Init owns the HAL process and
# its original service declaration, user/group and SELinux domain.
setprop ctl.restart vendor.oplus.vibrator
