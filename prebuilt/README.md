# 发布包副本

手动内置请使用 `Nyako-OriginOS-Haptics-Builtin-v0.2.1-ConfigFix.zip`。它修正了 DNA 配置文件中连字符的写法，并补上 `/dev/awinic_haptic` 的 ueventd 权限规则，避免节点被创建为 0600 导致 HAL 退出、卡二。HAL 和波形与原 v0.2.1 一致。此修订包由本地打包脚本生成，不是原 Action 的产物；仓库的打包脚本也已修正，后续 Action 将采用相同格式。原发布包保留用于追溯。

这里的两个 ZIP 直接下载自 v0.2.1 Release，与对应 Action artifact 相同。构建脚本不读取 prebuilt，不会拿现成 HAL 冒充编译结果。

- `Nyako-OriginOS-Haptics-v0.2.1.zip`：Magisk／KernelSU 模块。**回一加主系统前必须停用，否则会卡二。**
- `Nyako-OriginOS-Haptics-Builtin-v0.2.1.zip`：原手动内置包，DNA 用户请改用上面的 ConfigFix 版本。

`verification.json` 记录产物和解包目录文件的比对结果。已通过隔离 Binder 测试；重打包后的原生开机与触感仍待实测。
