# 发布包副本

这里的两个 ZIP 直接下载自 v0.2.1 Release，与对应 Action artifact 相同。构建脚本不读取 prebuilt，不会拿现成 HAL 冒充编译结果。

- `Nyako-OriginOS-Haptics-v0.2.1.zip`：Magisk／KernelSU 模块。**回一加主系统前必须停用，否则会卡二。**
- `Nyako-OriginOS-Haptics-Builtin-v0.2.1.zip`：手动内置包，无安装和自动合并脚本，按包内 README 操作。

`verification.json` 记录产物和解包目录文件的比对结果。已通过隔离 Binder 测试；重打包后的原生开机与触感仍待实测。
