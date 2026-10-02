Nyako v0.2.1：统一 HAL 文件名、日志和源码前缀，保留已有震动调校。

**回一加主系统前必须先停用 Magisk／KernelSU 模块，否则会卡在第二屏（卡二）。** KernelSU 使用早期 initrc 注入时，停用后执行 `ksud initrc refresh` 再切换。

- `Nyako-OriginOS-Haptics-v0.2.1.zip`：Magisk／KernelSU 模块。
- `Nyako-OriginOS-Haptics-Builtin-v0.2.1.zip`：手动配置包，保留 `odm/` 相对目录，不带自动安装脚本。详细操作见包内 `README_CN.md` 和仓库的 [手动内置步骤](https://github.com/Maga-King/OriginOS-OnePlus-Haptics/blob/main/docs/BUILTIN.md)。

两种包和单独的 `nyako-vibrator` 使用同一次 Action 编译的 HAL。

- 一加 0916T / OriginOS，765 个可调用编号，615 个波形文件。
- 保留已调好的输入法 effect 2、AI 唤醒、密码短反馈和强度调节。
- 补入三首铃声的震动文件、受击和赛车效果；3066、3103、26007 用明确标注的兼容波形补齐请求。
- 完整模块 ZIP 可在 Magisk / KernelSU 安装，包含官方 Magisk recovery 安装入口。
- 内置源码 ZIP、波形 ZIP，都保留相对目录。无需本地 ROM 和私人路径即可重新构建。
- 固定 NDK r27d，在两个不同目录重新编译并比较所有正式产物；正式产物来自这次 Action。

安装后重启，让系统刷新能力列表。当前实机以 KernelSU DSU 为主；Magisk 早期能力缓存仍待实测。

三首原始音频仍缺。HE/FMQ 等未完成项和兼容波形的边界详见 README，不宣称完整原厂复刻。
