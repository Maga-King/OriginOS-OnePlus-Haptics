# 交给现有元模块挂载

本机安装了 mountify，所以这次用标准 `system/` 目录交给它挂载。安装的是普通震动模块，不替换你已有的元模块，也不替换 Nyako 核心。

下载 `Nyako-OriginOS-Haptics-Metamodule-v0.3.0-demo2.zip`，在 DSU 的 KernelSU 管理器安装，然后完整重启回 DSU。它沿用 `originos_0916t_demo` ID，更新时替换旧震动模块。

包内主要路径：

```text
system/odm/bin/hw/vendor.oplus.hardware.vibrator-service
system/odm/etc/nyako-vibrator/waves/...
system/lib64/libNyako_hook_haptics_concurrency.so
```

不带 `skip_mount`、自挂载脚本、监督进程或早期 initrc。挂载完成后，普通 late_start 脚本只让 init 重启原震动服务，由原服务声明决定用户、组和 SELinux 域。安装时设置 HAL、波形和插件的文件标签，供 mountify 复制；没有机型、哈希或 Binder 自检门槛。KernelSU 的挂载与脚本顺序见 [官方说明](https://kernelsu.org/guide/metamodule.html)。

插件成为系统内置插件，默认启用。以前单独安装的同名外置插件应移出 `/data/local/nyako/modules/haptics_concurrency/`，避免重复扫描。保留其他插件和进程白名单；有白名单时保留 `android`。系统作用域改变后需要完整重启，热替换 SO 不会给已运行的 system_server 安装 Hook。

**回一加主系统前停用本模块。** 用过旧版早期 initrc 的，停用或更新后执行 `ksud initrc refresh`。元模块本身可以保留。

`demo2` 保留普通短震动的旧直接输出，已在目标手机热更新后由用户确认恢复正常。较长波形采用混合输出，补齐 1000 字节预填充、结束标记和 FIFO 完成等待。短触感到达正在直接播放的短会话时，暂时等待该短会话结束，不能宣称每一种重叠都已经无等待。完整插件链路和实际触感仍以重启后的日志与实测为准。

不使用元模块的用户仍可选原自挂载 ZIP；手动内置包仍保留 ODM 相对目录。三种包里的 HAL 来自同一次构建。
