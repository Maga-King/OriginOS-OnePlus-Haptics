# 并发插件的手动内置

`0.3.0-demo4` 的手动包同时包含 HAL 和插件 `0.1.2`。它没有自动安装脚本；先按 [HAL 内置步骤](BUILTIN.md) 合并 ODM 文件，再按本页合并 system。旧文档中“只打包 ODM”的说明仅适用于不内置插件的 HAL。

先备份 `ROM/config/system_fs_config`、`ROM/config/system_file_contexts`，以及已有同名插件。备份放在解包目录外。已有 Nyako 框架核心及 app_process64 集成需保留；本包没有私人框架核心，也不会替代它。

复制：

```text
ZIP/system/system/lib64/libNyako_hook_haptics_concurrency.so
→ ROM/system/system/lib64/libNyako_hook_haptics_concurrency.so
```

这是 DNA 的 system-as-root 解包层级，开机后实际路径为 `/system/lib64/libNyako_hook_haptics_concurrency.so`。如果你的解包结构不同，先对照原来的 `libnyako_core.so` 路径调整同层目录及配置前缀，不能多套一层 system。

在 `config/system_fs_config` 中按原有格式新增或替换这一行：

```text
system/system/lib64/libNyako_hook_haptics_concurrency.so 0 0 0644
```

在 `config/system_file_contexts` 中按原有格式新增或替换这一行：

```text
/system/system/lib64/libNyako_hook_haptics_concurrency\.so u:object_r:system_lib_file:s0
```

不要覆盖整份 config，也不要留下同一路径的重复条目。使用空格分列、四位权限；点转义为 `\.`，连字符保持原样。本包附有两份 `metadata/system_*.additions`，供逐行合并。

重新打包 **odm 和 system**，分别使用原来的 fs_config、file_contexts 和镜像选项，再走原有组包安装流程。启动前停用旧震动模块，并避免另装同名外置插件。已有 Nyako 进程白名单时需包含 `android`。内置版不依赖元模块挂载，但仍依赖已正确集成的 Nyako 框架。

插件必须完整重启才能加载，热换 HAL 不会重载它。通知、铃声及闹钟期间的短触感范围见 [混音规则](MIX_RULES.md)。没有安装或开机校验脚本；本页描述的文件权限和标签是打包所需元数据。
