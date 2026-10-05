# Nyako 短触感并发试用插件

配合 [OnePlus 0916T OriginOS 震动 HAL](https://github.com/Maga-King/OriginOS-OnePlus-Haptics) 使用。当前插件版本为 `0.1.2`。

打字、手势等短触感被已有震动的优先级挡住时，插件把单个预设效果送入 HAL 的短触感逻辑路。HAL 对齐两路波形，最终只输出一路 PCM；新短触感替换旧短触感，按 token 与 usage 分别取消。原框架拒绝记录保留，插件的 `parallel` 日志记录另一条输出的结果，不把拒绝伪装成正常会话成功。

`0.1.1` 只处理通知；`0.1.2` 扩大到通知、铃声、闹钟等普通 `SingleVibrationSession`。不介入 Vendor/External 会话，不绕过权限、AppOps、设置或勿扰。新请求仍限 TOUCH、硬件、输入法、手势的单个 Prebaked/ExtPrebaked 效果，支持 Mono 和单振子 Stereo，最长 120 ms。多段组合、HE 和真正的框架多会话管理仍未完成。

这一版借鉴 OPPO 的来源识别、独立取消和统一混音思路，仍是插件补发机制，并非删除了原优先级拦截。正常按键和 AI 唤醒使用原来的映射。

## 编译

从 HAL 仓库执行：

```text
python plugin/haptics_concurrency/build.py --repo <Nyakohookframework目录> --sdk <Android SDK目录> --java <JDK目录> --ndk <NDK r27d目录> --core <兼容libnyako_core.so路径>
```

框架示例目录中的脚本入口为 `examples/haptics_concurrency/build.py`，参数相同。需要 Android SDK 37、build-tools 37.0.0、Java 11 编译目标和 ARM64 Android 28 原生目标。Nyako API 只作编译依赖，DEX 仅包含本插件的类，不打包框架核心或 API 实现。

成品在本目录的 `out/`：`libNyako_hook_haptics_concurrency.so` 与 `Nyako-Haptics-Concurrency-0.1.2.zip`。

## 使用

需要支持短触感独立逻辑路的 HAL，以及已正确集成的 Nyako 框架。HAL `0.3.0-demo2` 有长波形提前停止的问题，不再推荐；使用包含流式修复的更新版本。稳定版 `0.2.1` 没有这条短触感接口。

已有元模块时，使用 HAL 的 Metamodule 包，它已把插件放在 `/system/lib64/`，默认启用；不要再安装第二份外置插件。单独安装时，把插件 ZIP 导入 Nyako 管理器，启用并保留 `android` 作用域。已有进程白名单时需包含 `android`。

更新后必须完整重启回 DSU。热换 SO 或重启输入法不会重载 system_server 中已经加载的插件。日志 `NyakoHaptics: parallel` 包含 effect、usage、currentUsage、package、owner 和 HAL 返回结果；49 表示当前通知、33 表示铃声、17 表示闹钟。

已有短会话走直接输出时，混入的短路可能等待它结束；并非所有场景都能零延迟叠加。实际通知、铃声及快速打字还需实机复核。

回一加主系统前必须停用震动模块，否则会卡二。手动内置、权限及文件标签说明见 HAL 仓库文档。本插件没有添加安装或开机校验脚本。
