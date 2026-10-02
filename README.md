# OriginOS · 一加 0916T 震动 HAL

这是给一加移植 OriginOS 做的震动适配。

现在用自己的 HAL 接收系统请求，再通过这台一加原有的驱动播放波形。输入法固定用一加 effect 2；连续点击会打断上一段，长波形也能取消。

主要适配对象是一加 13 / 0916T 马达 。

## 下载和刷入

在 [Releases](https://github.com/Maga-King/OriginOS-OnePlus-Haptics/releases) 下载 `OriginOS-OnePlus-Haptics-v版本号.zip`，在 Magisk 或 KernelSU 管理器里安装。

**从 OriginOS DSU 回一加主系统前，必须先停用这个 Magisk／KernelSU 模块，否则会卡在第二屏（卡二）。** KernelSU 使用过早期 initrc 注入时，停用后执行 `ksud initrc refresh` 刷新，再切回主系统。

要放进解包 ROM，下载单独的 `OriginOS-OnePlus-Haptics-Builtin-v版本号.zip`，按 [手动内置步骤](docs/BUILTIN.md) 操作。它保留 `odm/` 相对路径和 DNA 打包配置增量，不含自动安装或配置合并脚本。

模块自带挂载脚本，不依赖额外挂载模块，不直接修改 odm/vendor 分区文件。安装和启动没有机型、序列号、哈希或 Binder 自检门槛。进程异常退出会恢复原服务，不会自己把模块永久禁用。

KernelSU 的早期注册路径已经在目标手机上验证。Magisk 的安装入口和运行脚本已经包含，早期能力缓存还没有完成同等实机验证。完整功能目前优先在 KernelSU 上测试。

日志在 `/data/adb/originos_0916t_demo/service.log`。需要回退时，在管理器里停用模块再重启，或者执行模块目录中的 `rollback.sh`。

## 这版补了什么

目前公布 **765 个可调用编号**：供体表里的 761 个，加上实际抓到的 4 个桌面／上滑编号。内置 615 个波形文件。

- 输入法 141～150 使用一加官方 effect 2，十档强度；密码按键保持短反馈。
- 49 档强度、清脆／柔和风格、组合效果，以及播放打断和完成回调。
- 337、449、519 已接入用户提供的三首铃声震动波形。
- 625 保留原始两次受击的时间线，脉冲间隔约 60ms；631～637 保留赛车七档强度。
- HE1、完整单包 HE2，有限循环、间隔和连续事件的频率曲线。

有几个编号仍使用兼容方案，放在这里说清楚：

| 编号 | 当前处理 |
| --- | --- |
| 65 | 清脆原定义缺失，使用同场景柔和版本 |
| 691 | 使用提供的 `T_Rtp_long_vibrate_4v2.bin`，按原表相对强度缩放；原始文件绑定还没确认 |
| 3066 | 原神原文件缺失，用一加官方 effect 41 补反馈，尾部留白到原配置的 3000ms |
| 3103 | 原神原文件缺失，用一加官方 effect 42 补反馈，尾部留白到原配置的 2215ms |
| 26007 | 具体事件和原波形未知，用官方 effect 103 提供约 554ms 的短节奏 |

这些编号已经有输出，原厂节奏仍待拿到文件。单马达也无法复刻双马达的左右定位；337 当前使用 Major 主通道。

三首原始铃声音频没有包含在用户提供的文件里，本仓库只补对应的震动数据。没有把别的铃声改名顶替，也没有改动设置的选择界面。

HE3、HE2 分包、无限循环、播放中参数更新、瞬态频率调制和生产环境 FMQ 还没做完。部分标准扩展能力仍明确返回不支持。

## 自己编译

需要 **Python 3.12** 和 **Android NDK r27d（27.3.13750724）**。不需要再找原 ROM、手机里的库、这台电脑上的工具目录，也不需要重新生成 AIDL。

```bash
git clone https://github.com/Maga-King/OriginOS-OnePlus-Haptics.git
cd OriginOS-OnePlus-Haptics
python3 tools/build.py --ndk /你的/Android/ndk/27.3.13750724 --out out
```

Windows 也可以运行这个脚本，把 `--ndk` 换成 Windows NDK 路径即可。正式发布使用 GitHub Actions 的 Linux 工具链；跨操作系统的本地编译不保证字节完全一致。

产物包括 HAL `mio-vibrator`、`driver_query`、完整模块 ZIP、`source.zip`、`waveforms.zip`、`build-info.json` 和 `SHA256SUMS`。不会把预编译 HAL 当作构建输入。

### GitHub Actions 和 Release

Actions 页面的 **构建并发布 HAL** 可以手动运行；推送 `v*` 标签也会触发。默认会编译、测试并创建对应版本的 Release。

流程固定 NDK 版本，在两个不同路径分别完整编译，然后逐字节比较 HAL、完整模块 ZIP 和内置源码包。ZIP 条目排序、时间戳和权限固定。测试通过才会发布。

Release 和 Action artifact 是同一次构建的产物。手机部署直接使用下载下来的 Release ZIP，不再本地重新编译一份。`build-info.json` 记录源码提交和 HAL 摘要，`SHA256SUMS` 供人工核对；它们不参与手机安装或启动。

复现 v0.2.0 时请检出 `v0.2.0` 标签，并给构建脚本传入 `--revision "$(git rev-parse HEAD)"`；打包元数据也需要相同提交号。独立的 **构建并发布内置包** Action 会从该标签完整重编译，确认整个模块与已有 Release 字节一致，再生成内置包并附加到同一 Release，不移动原标签。后续内置脚本和文档在 main 分支维护。

## 内置 ZIP 的目录

`assets/waveforms.zip` 保留相对路径，不会把同名文件摊平：

```text
waves/
├── def/effect_2.bin
├── soft/effect_109.bin
└── donor/effect_337.bin
```

完整模块中也保留这套 `waves/` 目录，并内置 `waveforms.zip` 和可独立重新编译的 `source.zip`。`prebuilt/` 中的模块副本来自 Release；重新编译时不会读取它。

## 来源和开发记录

优先使用一加官方 ODM 的合适波形，再考虑我在 [OP13HyperOSFix](https://github.com/Maga-King/OP13HyperOSFix) 里调过的波形；没有接近的再保留或适配 vivo 数据。来源见 [assets/provenance.json](assets/provenance.json)，场景关系见 [docs/SCENES.json](docs/SCENES.json)。
