# 来源说明

本仓库发布的是自制 HAL、构建工具和配套模块。没有包含供体的闭源 HAL 服务、闭源 RichTap 引擎或内核模块，也不包含手机日志和账号凭据。

源码文件分别保留 `Apache-2.0` 或 `GPL-2.0-only` 的 SPDX 标记；不能把一个统一许可证直接套到所有内容。许可证文本放在 `licenses/`。AOSP 生成的 Binder 代码和 AIDL 声明保留原始版权信息。

波形来自用户提供的一加官方 ODM、vivo 固件及用户自己维护的调校仓库。它们的原始权利归对应权利人；源码许可证不授予这些资源额外的再分发许可。具体来源和改动方式见 `assets/provenance.json`。

参考资料：

- [OnePlusOSS SM8750 内核模块](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8750)，参考提交 `d50b305f7da9e14715a25120a4ac7b1a4b8b97c3`，用于理解 Qualcomm RichTap 驱动 ABI。
- [OP13HyperOSFix](https://github.com/Maga-King/OP13HyperOSFix)，波形来源提交 `9e0bb4eafcda3564de815778b55fce6cd99476e8`。
- [AOSP Vibrator AIDL](https://android.googlesource.com/platform/hardware/interfaces/+/refs/heads/main/vibrator/aidl/)，版本2；仓库已包含实际生成代码和 `aidl/provenance.json`。
- [Magisk 模块文档](https://topjohnwu.github.io/Magisk/guides.html)，recovery 安装入口使用 Magisk `v30.6` 的官方 `module_installer.sh`，保留其原始文件内容。

正式模块包额外携带 NDK 的运行库及工具链 notice。开发时的摘要比较和边界测试只用于构建验证，不是手机上的安装或启动限制。
