# 这次内置的 SELinux 处理

这次目标 ROM 沿用一加 `hal_vibrator_default`，检查现有策略后，没有为 Nyako 额外添加 allow，也没有切换到 permissive。必要的文件标签已经写进内置包的 `metadata/odm_file_contexts.additions`，手动打包时仍要合并它。

| 用途 | 现有规则涉及的目标 |
| --- | --- |
| init 启动 HAL 并进入原震动域 | `hal_vibrator_default_exec` → `hal_vibrator_default` |
| awinic 设备的读写、ioctl、mmap | `aac_richtap_dev_device:chr_file` |
| input 设备查询与强度写入 | `input_device:chr_file` |
| 读取波形和配置 | `vendor_configs_file:dir/file` |
| 注册 IVibrator/default | `hal_vibrator_service:service_manager` |
| 系统请求及完成回调 | `hal_vibrator_server`、`hal_vibrator_client` 的 Binder 规则 |

这表示当前包的源码规则已覆盖这些访问。仍需在重打包启动后，确认进程实际进入 `hal_vibrator_default`，以及实际设备节点和波形文件的标签正确。隔离 Binder 测试不等于原生域下的完整实机测试。其他 ROM 如果域、节点标签或策略不同，不能直接照搬“无需额外规则”的结论。

## 解包目录有一处容易读错

System-as-root 解包中，`system/etc` 可能是绝对软链接，指向 `/system/etc`。在手机上直接读取它，会读到当前正在运行的系统，而不是解包 ROM。

本次真正的平台策略位于：

```text
ROM/system/system/etc/selinux/plat_sepolicy.cil
```

本次曾因此误报 `profcollect_watcher` 缺少定义，已经纠正。目标 ROM 里该类型实际存在；使用正确的平台文件与其余分区策略，并使用 ROM 自带的 `system/system/bin/secilc`，普通和 vivo debug 两条策略链均编译通过。编译结果只用于检查，没有加载到正在运行的手机内核，也没有修改 ROM 原有策略。

`profcollect_watcher` 是性能采集相关的 SELinux 域；本包规则让它访问 profcollect 数据、相关属性和服务。它与 Nyako 震动域分开。没有证据表明这份 ROM 因它存在缺失而影响开机，之前的判断不能继续当作刷机阻碍。

如果一份 ROM 确实引用了未定义类型，并且开机需要重新编译策略，可能在 SELinux 初始化阶段就失败。具体流程见 [AOSP init 的策略加载代码](https://android.googlesource.com/platform/system/core/+/android16-release/init/selinux.cpp)。
