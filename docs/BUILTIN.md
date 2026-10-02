# 手动内置：从文件替换到重新打包

这份包就是本项目这次原生内置使用的布局，面向一加 13 / 0916T、保留一加 vendor/odm 的 OriginOS 移植 ROM。HAL 与 v0.2.1 Action 发布的 `nyako-vibrator` 字节一致，765 个编号、615 个波形。

**这不是 Magisk 模块，也不是 recovery 卡刷包。** ZIP 没有自动安装脚本。先解压到单独的目录，再按下面的步骤操作 ROM 解包文件。不要往正在使用的一加官方系统分区里覆盖。

## 1. 备份原文件

以下用 `ROM/` 代表你的 ROM 解包目录。它下面应有 `odm/`、`vendor/`、`system/`、`config/` 等目录。

把这四个文件复制到 ROM 目录外的备份文件夹：

```text
ROM/odm/bin/hw/vendor.oplus.hardware.vibrator-service
ROM/odm/etc/init/vibrator-default.rc
ROM/config/odm_fs_config
ROM/config/odm_file_contexts
```

原来如果已经有 `odm/etc/nyako-vibrator/`，也一起备份。备份不要塞回 odm 目录里，否则会被一起打进镜像。

## 2. 复制 HAL 和波形

按 ZIP 内的相对路径复制到 ROM：

| ZIP 内文件 | 放到 ROM 中的位置 |
| --- | --- |
| `odm/bin/hw/vendor.oplus.hardware.vibrator-service` | 同路径，替换旧 HAL |
| `odm/etc/nyako-vibrator/` 整个目录 | 同路径，包含波形、场景、构建信息和来源说明 |

波形目录必须保留下面的层级，不要把同名 effect 文件混到一个目录：

```text
odm/etc/nyako-vibrator/waves/
├── def/effect_2.bin
├── soft/effect_109.bin
└── donor/effect_337.bin
```

## 3. 修改 init 启动文件

打开 `ROM/odm/etc/init/vibrator-default.rc`，找到 `service vendor.oplus.vibrator` 开头的整段服务定义，换成：

```text
service vendor.oplus.vibrator /odm/bin/hw/vendor.oplus.hardware.vibrator-service --serve /odm/etc/nyako-vibrator/waves /dev/null
    class hal
    user system
    group system input
    interface aidl android.hardware.vibrator.IVibrator/default
    stdio_to_kmsg
```

在文件里再添加下面的动作；已有完全相同的动作时不要重复添加：

```text
on post-fs-data
    chmod 0666 /dev/awinic_haptic
    start vendor.oplus.vibrator
```

**保留原文件中 `on boot` 下的 chown、chmod 等驱动权限动作。** 不要只留下 service。ZIP 附带的 `odm/etc/init/vibrator-default.rc` 是本次一加 ODM 修改后的完整示例；如果你的原文件权限动作一致，可以直接覆盖，否则按上面两段手动修改自己的原文件。

整个 ROM 只能有一个 `vendor.oplus.vibrator` 服务定义和一个 `android.hardware.vibrator.IVibrator/default` 实现。不要额外加入第二份同名 rc。`/dev/null` 用来接收程序的启动标记，内置方式不需要模块的 ready 文件或挂载脚本。

原来的 `odm/etc/vintf/manifest/vibrator-default.xml` 保留 AIDL v2 声明：

```xml
<manifest version="1.0" type="device">
    <hal format="aidl">
        <name>android.hardware.vibrator</name>
        <version>2</version>
        <fqname>IVibrator/default</fqname>
    </hal>
</manifest>
```

## 4. 合并 DNA 打包权限

打开 ZIP 中的 `metadata/odm_fs_config.additions`，逐行合并到 `ROM/config/odm_fs_config`：

- 每行第一列是路径。如果 ROM 原文件已经有相同路径，用新行替换原行；没有就追加。
- 相同路径只保留一条，其他无关条目原样保留。
- **不要用 additions 文件覆盖整份 odm_fs_config。** 它只列出这次内置涉及的路径。

关键值如下，完整波形清单以 additions 文件为准：

```text
odm/bin/hw/vendor.oplus.hardware.vibrator-service 0 0 0755
odm/etc/init/vibrator-default.rc 0 0 0644
odm/etc/nyako-vibrator 0 0 0755
odm/etc/nyako-vibrator/waves 0 0 0755
odm/etc/nyako-vibrator/waves/def 0 0 0755
odm/etc/nyako-vibrator/waves/def/effect_2.bin 0 0 0644
```

所有新增目录用 `0 0 0755`，波形和说明文件用 `0 0 0644`，HAL 用 `0 0 0755`。Windows 文件属性不能代替这份打包权限配置。

## 5. 合并 SELinux 文件标签

用同样的方法，把 `metadata/odm_file_contexts.additions` 合并到 `ROM/config/odm_file_contexts`。已有同一路径的标签行就替换，没有就追加；保留其他文件的标签。

这里的路径是正则表达式，点号前的反斜杠要保留。例如：

```text
/odm/bin/hw/vendor\.oplus\.hardware\.vibrator\-service u:object_r:hal_vibrator_default_exec:s0
/odm/etc/init/vibrator\-default\.rc u:object_r:vendor_configs_file:s0
/odm/etc/nyako\-vibrator/waves/def/effect_2\.bin u:object_r:vendor_configs_file:s0
```

`-` 写成 `\-` 或直接写 `-` 都能匹配同一个实际路径。合并时识别这种等价写法，不要留下两条冲突标签。

进程沿用原来的 `hal_vibrator_default` 域，不使用 `ksu` 或 `magisk` 域。保留运行时 vendor_file_contexts 中原有的 HAL 执行文件映射；目标一加 ODM 已有这个映射。打包后文件的标签由合并后的 odm_file_contexts 写入镜像。

本包不提供整套 SELinux 策略，不把其他 ROM 的策略覆盖进来。这次目标包已有 awinic 驱动读写、mmap、input 写入、波形读取等相关规则。别的移植包如果缺规则，需要按实际拒绝日志补齐。文件标签正确不等于整包策略一定能编译；若 ROM 自己已有策略缺失，先解决整包原来的问题。

本次目标的普通／debug 策略已用正确的 system-as-root 路径编译通过，详细记录见仓库的 [SELinux 说明](SELINUX.md)。

## 6. 重新打包 ODM

回到你原来用的 DNA 工具，选择重新打包 **odm**，使用刚修改的：

```text
ROM/config/odm_fs_config
ROM/config/odm_file_contexts
```

文件系统格式、压缩方式、镜像大小按原 ROM 的打包配置处理；本次目标 ODM 是 EROFS。不要直接把文件夹压成 ZIP 当成 odm.img。

本次只涉及 ODM 及其两个打包配置，不需要因为震动改动重打 system 或 vendor。生成新的 `odm.img` 后，放回你原来的完整 ROM／super 组包流程。

**只替换 DSU 的 system.img 不会带上 ODM 的变化。** 要让安装流程实际使用新 ODM 镜像；是否支持单独安装 DSU ODM 取决于你使用的 DSU 工具。不要把移植版 ODM 单独刷进一加官方主系统。

## 7. 启动移植 ROM 后测试

启动这份移植 ROM 前，停用旧 `originos_0916t_demo` 模块，避免旧模块再次接管。KernelSU 如果留着旧 initrc 注入，停用后执行 `ksud initrc refresh`，再重启进入移植 ROM。内置版本本身不需要 root 管理器。

**如果仍使用 Magisk／KernelSU 模块：回一加主系统前必须先停用，否则会卡在第二屏（卡二）。**

进入新 ROM 后，有 root 时可以手动查看：

```sh
su -c 'getprop init.svc.vendor.oplus.vibrator'
su -c '/odm/bin/hw/vendor.oplus.hardware.vibrator-service --check-service'
```

预期分别是 `running` 和包含 `private_53=1 private_10053=1` 的输出。这只是你手动查看结果，安装和开机没有新增校验。

随后实际试密码按键、连续输入、AI 唤醒、强度滑条、两种风格、三类触感体验和铃声震动。新增编号需要一次完整重启，让系统重新读取能力列表。

`stdio_to_kmsg` 只在 userdebug/eng 的调试节点可用时输出 stderr 日志，普通 user 系统不能保证有这些详细日志；说明见 [AOSP init 文档](https://android.googlesource.com/platform/system/core/+/refs/heads/main/init/README.md)。启动失败时可从 logcat 的 init、linker、servicemanager 日志查起。

## 回退

恢复步骤 1 备份的四个文件；如果 `odm/etc/nyako-vibrator/` 是本次新加的，移除它。然后重新打包 ODM 并按原流程安装。不要恢复其他版本的整套 config。

765 个编号中，65、691、3066、3103、26007 仍有兼容或待确认部分。HE/FMQ 等未完成内容沿用仓库主 README 的说明。原生内置改变的是部署方式，不会自动补齐尚未实现的 HAL 功能。
