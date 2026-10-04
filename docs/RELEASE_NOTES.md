Nyako v0.3.0-demo1：通知与短触感并发试用版。

HAL 由一个线程输出两路 PCM，正常会话与短触感分别取消。单路保留原波形，叠加可能溢出时降低通知分量。不装插件也可以正常使用 HAL；被 Vivo 框架优先级挡住的请求，需要另外安装 Nyako 短触感插件。

这次只补发通知期间的非循环、单个预设短触感，覆盖 TOUCH、硬件反馈、输入法反馈和手势反馈，最长 120 ms。多段组合、HE、外部控制和铃声冲突尚未接入补发，实际延迟和触感还需要复核。输入法 effect 2、AI 唤醒以及 765 个场景的既有波形保持不变。

- 模块 ZIP：Magisk／KernelSU 安装；手动内置 ZIP：按包内中文说明合并 ODM 文件和 config。
- 插件独立构建步骤见 [Nyako 短触感插件](https://github.com/Maga-King/OriginOS-OnePlus-Haptics/tree/main/plugin/haptics_concurrency)。需要有访问权限的 Nyako API 和现有核心，公开仓库不包含私有核心。
- 主机并发、取消、卡住驱动和既有场景回归已通过；目标手机三组底层物理输出测试返回正常。完整重启后的插件补发链路仍待实测。

**从 DSU 回一加主系统前，必须停用 Magisk／KernelSU HAL 模块，否则会卡二。使用早期 initrc 注入时，停用后执行 `ksud initrc refresh`。插件安装或开关改变后，需要完整重启回 DSU。**

这是预发布试用版本。稳定版 0.2.1 继续保留。HE/FMQ、部分原始音频和性能适配等原有未完成项不在本次完成范围内。
