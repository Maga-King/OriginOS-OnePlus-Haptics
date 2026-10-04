# Nyako 短触感并发试用插件

这份插件解决的是：通知还在震动时，输入法、手势等短触感在 Vivo 框架中被优先级挡住，根本没有送到 HAL。

插件运行在 `android`（system_server）作用域。原请求通过系统的权限、设置和 AppOps 判断后，如果被正在播放的通知以 `IGNORED_FOR_HIGHER_IMPORTANCE` 拒绝，插件将单个预设效果送进并发版 HAL 的独立短触感通道。原框架请求的拒绝记录保留，避免把另一路输出假报成原会话成功。

HAL 的一个线程输出两路 PCM。两路分别取消；新的短触感替换上一段短触感，不累积等待。叠加可能溢出的采样会降低通知分量，保留短触感分量。单路播放时保留原波形。普通短震动沿用直接播放；较长波形混合时使用 10 ms 帧，并满足驱动的 1000 字节预填充要求。短触感遇到正在直接播放的短会话时，暂时等待该短会话结束。实际触发延迟还需要实机测量。

试用范围是通知 usage=49 期间、非循环、单个 `PrebakedSegment` 或 `ExtPrebakedSegment` 的 TOUCH、硬件反馈、输入法反馈和手势反馈。HAL 只接受不超过 120 ms 的短触感。多段组合、HE、外部控制和铃声期间的优先级冲突还没有通过这个插件补发，不能当作全场景并发已经完成。正常播放的输入法和 AI 唤醒沿用原来的映射。

## 构建

本仓库只有我们自己写的插件业务代码，不包含私有框架核心或 API 实现。编译需要有权限访问的 Nyako 仓库和兼容的 `libnyako_core.so`，不必重新编译、替换手机上的核心。

```text
python plugin/haptics_concurrency/build.py --repo <Nyakohookframework源码目录> --sdk <Android SDK目录> --java <JDK目录> --ndk <NDK r27d目录> --core <libnyako_core.so路径>
```

使用 Android SDK 37、build-tools 37.0.0、Java 11 编译目标、ARM64 Android 28 原生目标。DEX 仅包含插件类，Nyako API 仅作编译依赖。`out/Nyako-Haptics-Concurrency-0.1.1.zip` 内只有一个 SO 和一个 conf，供 Nyako 管理器安装。支持 Mono 和只有一个振子的 Stereo 请求；本机输入法日志使用的是后者。

## 使用

1. 先安装或内置 `0.3.0-demo2` 并发版 HAL。普通 `0.2.1` HAL 没有短触感通道；`demo1` 有短震动无输出的问题，已撤回。
2. 在 DSU 的 Nyako 管理器导入插件 ZIP，启用「Nyako 短触感并发」，保留 `android` 作用域。已有进程白名单时将 `android` 加入其中。
3. 完整重启回 DSU。系统作用域插件需要完整重启，不能靠重启输入法或热替换 SO 生效。
4. 持续打字时触发一条通知，观察按键触感是否仍能出来，再测手势。日志 `NyakoHaptics: parallel` 和 `NYAKO parallel` 分别记录插件请求与 HAL 接收结果。

插件开关关闭后完整重启即可停用补发。HAL 回退可恢复旧的 `vendor.oplus.hardware.vibrator-service`。采用 Magisk 模块的用户，回一加主系统前必须关闭模块，否则会卡二。手动内置的文件、config 合并和权限步骤仍见 [内置说明](../../docs/BUILTIN.md)。本插件没有安装校验或开机校验脚本。

已有元模块时可以直接安装 [元模块版](../../docs/METAMODULE.md)，插件放在 `/system/lib64/`，成为默认启用的内置插件。对应二进制在 `assets/plugins/`，只包含我们编写的插件和 DEX，不包含框架核心；Action 将它打包，同时完整编译 HAL。重新编译插件仍使用上面的私有框架依赖。
