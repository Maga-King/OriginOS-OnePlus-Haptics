# 通知期间按键没震动：并发调度调查

2026-10-04 在本机 OriginOS 移植系统复现。当前只完成定位，没有把并发修复写进发布的 HAL。

## 已确认的问题

通知播放期间，vivo AI 输入法请求的编号 148 被系统记录为 `ignored_for_higher_importance`，没有进入 HAL 播放。同一段记录里，通知结束后的按键恢复为约 33～37 ms 的完整请求。

因此，不能把这次现象简单归因于 HAL 在等通知播放完。框架已经丢掉的请求，HAL 收不到；即使 HAL 支持叠加，也无法直接补回。

目标框架的 `VibratorManagerService.shouldIgnoreForOngoing()` 比较新旧会话的重要程度：正在运行的会话优先级更高时，直接拒绝新会话。`getVibrationImportance()` 把通知设为 3；TOUCH（usage 18）落到默认分支，为 0。此前把 TOUCH 写成 1，是与 HARDWARE_FEEDBACK 混淆了，现已更正。这与本机记录一致。[AOSP 同类调度代码](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main/services/core/java/com/android/server/vibrator/VibratorManagerService.java)也有这个仲裁步骤；具体结论以本机日志和目标 ROM 反编译结果为准。

该判断按 usage 执行，不按某个波形编号执行。所以问题有可能影响其他低优先级触感，但尚未逐项实测，不能说所有场景都已确认受影响。当前输入法使用的是 148，不能继续只按早期观察到的 146 排查。

## 我们的 HAL 也有需要改进的地方

`PlaybackQueue::submit()` 会取消当前任务，并替换尚未播放的任务，没有把通知列为必须播放完的任务。底层播放循环约每 0.5 ms 检查取消，但 ioctl 的耗时仍需实测，不能把这个检查间隔当成实测抢占延迟。

这套设计支持替换，不支持通知与触感的 PCM 叠加。标准接口和 RichTap 共用输出队列；标准 `off()` 和私有 stop 当前也会停止这条公共队列。把 cancel 删除，会变成排队等待；让两个线程各自写 RTP，则可能互相 STOP、改增益或覆盖共享缓冲区，都不适合作为修复。

完成回调同样是调度的一部分。不能为了让框架释放会话而提前报告整段通知完成，那会让框架状态与实际输出不一致。

## 官方实现提供了哪些线索

本机一加官方 AAC 库的反编译结果包含：

- `VibratorMixer::signal_insert()` 和 `signal_append()`：按 performer dimension 选择两组命令队列，分别支持插入和追加。
- `VibratorMixer::begin_stream_mode()`、`end_stream_mode()`：单独处理流模式生命周期。
- `MixController::stream_mix_data()`、`schedule_pattern()`：提供流数据与 pattern 调度路径。
- `VibratorMixer::pend_interrupt()`：结合队列时长和当前时刻判断中断条件，并非简单取消所有旧请求。

这些线索说明官方实现比我们当前的单任务替换队列复杂，但尚不能据此断言某个官方场景一定走哪条混合路径。

vivo 官方工具库存在 `VivoRichtapAlgo::get_mix_wave_data()`，输入是一组 vib_event。它证明有波形事件混合实现，但不能直接证明通知会话与独立按键会话必然并行。其 `getExtVibrator()` 返回保存的 IVibrator 对象，仍需继续追踪对象创建和使用，不能仅凭接口名称认定它是独立物理马达或独立混音通道。

已保存的小米、Qualcomm 和 Pixel 开源 HAL 可以参考驱动停止、完成回调、增益恢复和取消时序；它们不能代替目标 vivo 框架的会话仲裁逻辑，也不意味着它们都支持混音。

## 修复需要覆盖两层

先在框架层把需要并发的短触感交给单独的调度入口，避免它在进入 HAL 前被拒绝；这条入口要保留原有开关、强度、权限和用户设置，不能把按键一律伪装成高优先级通知。仅关闭优先级判断仍可能使框架取消原通知，需要同时处理会话生命周期。

然后在 HAL 内保留唯一的物理输出线程，将通知、短触感等逻辑来源合成一条 PCM 流。取消和完成回调按来源或任务处理，某一路的 stop 不应误停其他路。连续按键仍需及时替换过时的按键，不积压成延迟震动。

混合时需处理幅度上限，不能把两个满幅波形直接相加后硬裁剪。通知结束、短触感结束、关闭震动以及流播放失败时，都要恢复正确的增益和状态。

验证应覆盖通知加按键、通知加手势、长触感加短触感、快速连按、标准接口与 RichTap 相互取消，以及各自的回调。修复前后要对照框架接受记录、HAL 接收记录和实际输出时序；当前尚未完成这些修复和测试。

原始手机日志包含应用信息，只保存在本地，没有上传到公开仓库。

## 不使用 HOOK 能不能做

可以直接修改目标 ROM 的 `services.jar`，内置框架补丁，不依赖 LSPosed 或运行时方法拦截。不过，仅放行优先级判断不等于并发：后面的会话调度仍可能取消通知。真正的修复需要在普通设置和权限检查通过后，把独立短触感交给单独的执行路径，让它不替换正在进行的通知会话；HAL 再负责混合、分路取消和各自的完成回调。这是框架与 HAL 的配套改动，尚未实现或刷入。

目前检查到的这个优先级判断不读取 property、设备能力或 FeatureConfig，因此没有发现通过改某个属性直接开启并发的开关。HAL 能力、HE 支持和输入法配置可能改变请求路径，但不能反过来让这个函数放行已经进入该路径的低优先级请求。

## 为什么不能直接说 vivo 官方没有这条限制

进一步对提供的两个 `services.jar` 分别重新反编译：

| 来源 | JAR SHA256 |
| --- | --- |
| vivo 官方包 | `54d80b9bfe3b6b3779fe8c5c4c91dd2e84e2a12df3209c0fc09e7d9dc062588b` |
| HybridTrash 包 | `9a0cef55419739dd291385d4e1ac87a4e78a2277950677656054c4a07259be71` |

手机当前的 JAR 与第二行一致。分别用同一版 JADX 提取的 `VibratorManagerService` 整个类文本一致，SHA256 都为 `268611c27692f688d2db43421758c31db37c54551f33a93cc7dbf9417a4a04fd`。这只证明这个类一致，不代表整个框架一致。

官方包也有同样的优先级拒绝；不能把差别解释成移植包额外加了这条限制。相同入口、usage 和会话重叠条件下，这段代码同样会拒绝。用户在官方机上感受到并发顺畅，仍可能来自不同请求入口、通知波形与占用时序，或其他机型/版本的实现；没有官方机同场景运行日志，暂时不能确定是哪一个原因。

当前手机的 QQ 通知是普通 Step 波形 `[100ms 静默, 200ms 震动, 200ms 静默, 100ms 震动]`，框架将整段作为通知会话管理。官方包与当前系统的 `persist.vivo.support.lra` 都是 1，`ro.vivo.lra.audioToHaptic.support` 都是 false；没有证据支持仅靠把这两项改成其他值来解决。

## OPPO／一加的进一步对照

底层来自此前提取的本机一加官方 AAC 库；框架来自工作区已有的 ColorOS 16 反编译样本。框架样本没有在这次调查中与本机最新官方 `services.jar` 做字节比对，下面的框架行为不能泛化到所有 OPPO 版本。

### 底层是真正的样本混合

`MixController::stream_mix_data()` 不只是名称带 mix：反编译中可以看到按两路流的样本进度对齐，将输入的 double 样本累加到已有缓冲区。混合结果随后进入动态保护、驱动缓冲区输出等步骤。`VibratorPerformer::write_mmap_buf()` 包含 ThermalCtrl 处理、数值缩放和转换为有符号 8 位样本时的边界限制。

`VibratorMixer::get_performer_dimension()` 先检查命令是否属于已有队列，再寻找空队列。两组队列都占用或不满足混合条件时会返回失败；代码也包含流模式和长 pattern 的限制。因此它不是无限并发，也不是两个线程不加协调地各自写马达。

### 框架有按来源保留效果的规则

样本中的 `VibratorCustomizedManager` 从 `/my_product/etc/vibrator/vibrator_customized_etc.json` 读取 `richtap_mix_wave_pkg_rule`，包括 `rule_on` 和 `package_list`。相关功能还受 `oplus.software.vibrator_luxunvibrator` 控制，默认规则为关闭。

`LinearMotorVibratorController.startRichtapVibratorImplLocked()` 对命中规则的应用检查发送者，对 HE2 检查此前 pattern 数据；根据判断决定是否调用 RichTap stop，并记录当前 SenderId。它没有把所有 RichTap 请求都无条件先停掉。HE2 的 SenderId 从 pattern 中提取 PID 和序列信息。

这条规则主要描述 RichTap pattern 的保留与停止，不能当成放行所有普通通知和按键的开关。把 JSON 复制到 vivo ROM 不会产生效果，vivo 当前框架没有这套配置读取路径。

### 普通框架仲裁仍然存在

ColorOS 样本的 `shouldIgnoreForOngoingLocked()` 先运行原有优先级判断。如果已被原判断拒绝，会直接返回，不进入扩展判断。`VibratorManagerServiceExtImpl.shouldIgnoreVibrationForOngoing()` 是附加限制，不是强制放行：其中还会忽略某些正在播放较长 OplusPrebakedSegment 时到来的更短请求。

标准 HAL `Vibrator::perform()` 与 Oplus／AAC 私有 `perform` 也使用不同路径：前者调用 InputFFDevice::playEffect，后者进入 AAC prebaked／pattern 调度。具体场景走哪条路径仍需运行日志确认。

可以借鉴的是来源识别、两路调度、同一时间轴上的样本混合、幅度保护，以及有条件的取消；不能把“OPPO 有 mixer”解释成“HAL 自动解决所有框架拒绝”。无 HOOK 修复仍需要目标 vivo 框架的静态补丁与 HAL 配套。
