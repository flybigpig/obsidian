# property_set(EXIT_PROP_NAME, value, "1"); 源码级解析（BootAnimation 场景）

结合你之前关注的 BootAnimation（开机动画）相关源码（如构造函数、waitForSurfaceFlinger、boot->run），`property_set(EXIT_PROP_NAME, value, "1");` 是 BootAnimation 中**控制动画退出、传递状态的核心系统属性操作**，通常用于动画执行完成/异常退出时，向系统（如 SystemServer、SurfaceFlinger）传递退出状态，以下从「核心含义、参数拆解、源码关联、调用场景、注意事项」全维度解析，确保与原有上下文衔接流畅。

## 一、核心前提：明确关键变量与函数背景


在 BootAnimation 源码中，该函数的调用依赖 Android Native 层两个核心基础，先明确避免歧义：

1. **EXIT_PROP_NAME**：宏定义的「动画退出系统属性名」，BootAnimation 源码中标准定义如下（不同机型可能略有差异，但语义一致）： `#define EXIT_PROP_NAME "service.bootanim.exit"` 作用：系统级属性的key，用于标识「开机动画是否需要退出」，SystemServer、SurfaceFlinger 会监听该属性，判断动画状态。
    
2. **property_set 函数**：Android Native 层用于「设置系统属性」的核心接口，定义在 `system/core/libcutils/properties.cpp` 中，并非 BootAnimation 自定义方法，作用是向系统属性服务（property_service）写入键值对，供其他进程读取，实现跨进程状态通信（此处即 BootAnimation 与 SystemServer/SurfaceFlinger 的通信）。
    

补充：该函数的第三个参数（此处为 "1"）是 Android 高版本（通常 Android 10+）新增的「权限参数」，用于控制属性写入的权限，这是区别于旧版本 `property_set(const char* key, const char* value)` 的关键差异。

## 二、逐参数深度拆解（结合 BootAnimation 场景）

函数原型（简化版，贴合实际调用场景）：

```cpp
int property_set(const char* key, const char* value, const char* perm);
```

对应调用 `property_set(EXIT_PROP_NAME, value, "1");`，三个参数的含义、作用及取值规范如下，重点结合动画退出场景说明：

### 1. 第一个参数：EXIT_PROP_NAME（key：系统属性名）

- 核心值：本质是字符串 `"service.bootanim.exit"`（由宏 EXIT_PROP_NAME 定义）；
    
- 作用：作为「开机动画退出状态」的标识，是跨进程通信的「约定key」—— BootAnimation 写入该属性，SystemServer 或 SurfaceFlinger 读取该属性，判断是否需要终止动画；
    
- 关键约束：系统属性名需遵循 Android 规范，以 `service.` 开头标识「服务相关属性」，确保与其他系统属性不冲突，BootAnimation 作为系统服务级动画，统一使用该key。
    

### 2. 第二个参数：value（value：系统属性值）

value 是传递的「具体退出状态值」，BootAnimation 中仅支持固定枚举值（语义由系统约定），核心取值及含义如下（高频考点）：

|   |   |   |
|---|---|---|
|value 取值|核心含义|调用场景|
|"0"|动画不退出，继续执行|极少使用，仅用于异常场景下「恢复动画执行」|
|"1"|动画正常退出（最常用）|动画播放完成、SystemServer 通知退出、桌面（Launcher）就绪时|
|"2"|动画异常退出|Surface 创建失败、帧解码异常、SurfaceFlinger 服务异常时|

补充：BootAnimation 中，`value` 通常不会传递自定义字符串，仅使用 "0"/"1"/"2"，若传递其他值，SystemServer 会视为「无效状态」，默认按异常退出处理。

### 3. 第三个参数："1"（perm：属性写入权限）

这是 Android 高版本新增的关键参数，用于控制「该系统属性的写入权限」，取值为字符串格式，核心规则及场景如下：

- 取值含义："1" 表示「仅当前进程及系统进程（如 root、system 权限进程）可写入，所有进程可读取」；
    
- 设计原因：`service.bootanim.exit` 是系统核心属性，若允许普通应用进程写入，会导致动画被恶意终止（如第三方应用篡改该属性，强制退出开机动画），因此通过权限控制保证安全性；
    
- 对比说明：若传入 "0"，表示「所有进程可读写」（禁止在 BootAnimation 中使用，存在安全风险）；传入 "2"，表示「仅 root 权限进程可读写」（过于严格，SystemServer 可能无法正常写入状态），因此 "1" 是 BootAnimation 的唯一合法取值。
    

## 三、核心作用与调用场景（结合 BootAnimation 完整流程）

结合你之前解析的 `boot->run("BootAnimation", PRIORITY_DISPLAY);`（动画启动）、`threadLoop()`（动画主循环），`property_set(EXIT_PROP_NAME, value, "1");` 仅在「动画退出阶段」调用，是 BootAnimation 生命周期终止的「核心触发点」，核心场景有3个，均贴合动画完整流程：

### 场景1：动画正常播放完成（最常见）

当 BootAnimation 解码完 `bootanimation.zip` 中的所有帧，且播放完毕后，会调用该函数传递正常退出状态，源码片段（简化版）：

```cpp
// BootAnimation::threadLoop() 动画主循环中
if (mFrames.empty()) { // 所有帧播放完成
    // 写入正常退出状态，value="1"，权限"1"
    property_set(EXIT_PROP_NAME, "1", "1");
    mExitPending = true; // 标记退出，终止 threadLoop 死循环
}
```

### 场景2：接收系统通知，主动退出

当 SystemServer 启动完成、桌面（Launcher）准备就绪后，会向 BootAnimation 发送退出通知（通过 Binder 或系统属性），BootAnimation 接收后调用该函数，源码片段（简化版）：

```cpp
// 接收 SystemServer 退出通知的回调方法
void BootAnimation::onSystemReady() {
    // 系统就绪，动画需退出，传递正常退出状态
    property_set(EXIT_PROP_NAME, "1", "1");
    mExitPending = true;
}
```

### 场景3：动画异常，强制退出

当出现 Surface 创建失败、帧解码异常、SurfaceFlinger 服务崩溃等情况时，动画无法继续执行，会调用该函数传递异常退出状态，便于系统排查问题：

```cpp
// 初始化 Surface 失败的异常处理
if (!initSurface()) {
    ALOGE("BootAnimation init Surface failed!");
    // 写入异常退出状态，value="2"
    property_set(EXIT_PROP_NAME, "2", "1");
    mExitPending = true;
    return false;
}
```

## 四、与 BootAnimation 其他流程的衔接（保证上下文流畅）

结合你之前关注的 BootAnimation 核心流程，该函数的调用处于「动画终止阶段」，完整链路衔接如下，确保与原有解析（构造函数、线程启动、Surface 绘制）无缝衔接：

```cpp
// 1. 初始化动画（构造函数，等待 SurfaceFlinger 就绪）
sp<BootAnimation> boot = new BootAnimation(callbacks);
// 2. 启动动画线程（优先级 PRIORITY_DISPLAY）
boot->run("BootAnimation", PRIORITY_DISPLAY);
// 3. 进入 threadLoop 死循环，执行动画绘制（解码帧→绘制→提交 SurfaceFlinger）
// 4. 触发退出条件（播放完成/系统通知/异常），调用 property_set
property_set(EXIT_PROP_NAME, value, "1");
// 5. 标记 mExitPending = true，终止 threadLoop 循环
// 6. 执行 cleanUp()，清理 Surface、释放资源，线程退出
```

关键衔接点：调用 `property_set` 后，BootAnimation 会标记 `mExitPending = true`，终止 `threadLoop()` 中的动画绘制死循环，随后执行资源清理操作，最终线程退出；同时，SystemServer 监听 `service.bootanim.exit` 属性，读取到对应 value 后，确认动画退出，继续执行系统启动后续流程（如启动 Launcher）。

## 五、关键注意事项（源码调试/面试高频考点）

结合 Native 层编码规范、BootAnimation 运行机制，以下注意事项必须掌握，同时贴合「上下文流畅」要求，关联之前的源码细节：

1. **权限参数不可随意修改**：第三个参数必须传入 "1"，不可改为 "0" 或 "2"。改为 "0" 会导致普通应用可篡改退出状态，改为 "2" 会导致 SystemServer 无法正常写入状态，均会引发动画异常（如无法退出、系统卡死）；
    
2. **value 取值必须符合系统约定**：仅可使用 "0"/"1"/"2"，不可传递自定义字符串（如 "exit"），否则 SystemServer 无法识别状态，会导致动画退出后系统无后续响应；
    
3. **调用时机必须在资源清理前**：需先调用 `property_set` 传递退出状态，再标记 `mExitPending = true`、执行 cleanUp() 清理资源。若先清理资源再写入属性，可能导致属性写入失败（进程资源已释放）；
    
4. **与 SurfaceFlinger 的联动注意**：写入属性后，SurfaceFlinger 会监听该属性，当检测到 value 为 "1"/"2" 时，会释放 BootAnimation 占用的 Surface 资源，避免资源泄漏（关联之前的 waitForSurfaceFlinger 方法，SurfaceFlinger 全程管理动画 Surface）；
    
5. **异常场景需传递正确 value**：异常退出时必须传入 "2"，而非 "1"，便于通过 Logcat 和系统属性排查问题（如通过 `getprop service.bootanim.exit` 查看退出原因）；
    
6. **函数返回值需判断（可选但推荐）**：`property_set` 函数返回 int 类型，0 表示写入成功，非 0 表示写入失败（如权限不足、属性服务异常），推荐添加返回值判断，避免状态传递失败： `// 推荐写法，增加失败处理` `if (property_set(EXIT_PROP_NAME, "1", "1") != 0) {` `ALOGE("BootAnimation set exit property failed!");` `}`
    

## 六、重点解析：EXIT_PROP_NAME 何时设置为1

EXIT_PROP_NAME（即 `service.bootanim.exit`）设置为1，是 Android 开机动画**正常退出**的核心标志（对应 value="1"），仅在「动画完成使命、无需继续显示」的正常场景下触发，结合前文源码、系统启动流程及底层机制，具体设置时机分为3类，全程关联前文内容，同时补充底层校验逻辑，确保无脱节：

### （一）时机1：开机动画所有帧播放完毕（基础时机）

这是最基础、最原始的设置时机，完全由 BootAnimation 自身控制，与系统服务无直接关联，对应前文「场景1：动画正常播放完成」：

1. 触发条件：BootAnimation 在 `threadLoop()` 主循环中，逐帧解码并绘制 `/system/media/bootanimation.zip` 中的动画帧，当检测到 `mFrames.empty()`（所有帧已解码并播放完毕），且无异常时，触发设置操作；
    
2. 源码联动：如前文源码片段所示，此时会调用 `property_set(EXIT_PROP_NAME, "1", "1")`，同时标记 `mExitPending = true`，终止动画绘制循环，随后执行资源清理；
    
3. 补充细节：该时机不依赖 SystemServer、Launcher 等组件，即使系统服务未完全就绪，只要动画帧播放完毕，就会设置为1，适用于动画时长短于系统启动时长的场景。
    

### （二）时机2：SystemServer 启动完成并主动通知（核心时机）

这是最核心、最常用的设置时机，由 SystemServer 主导，BootAnimation 被动响应，对应前文「场景2：接收系统通知，主动退出」，也是系统设计的默认主流流程：

1. 触发条件：SystemServer 是 Android 系统核心服务管理器，当它完成 AMS、WMS、ATMS、SurfaceFlinger 等所有核心服务的初始化后，会判定「系统已具备正常运行条件」，此时通过 Binder 向 BootAnimation 发送退出通知（触发 `onSystemReady()` 回调）；
    
2. 源码联动：BootAnimation 接收通知后，立即调用 `property_set(EXIT_PROP_NAME, "1", "1")`，标记退出循环，终止动画；同时 SystemServer 也会同步监听该属性，确认其被设为1后，继续执行后续系统启动流程（如启动 Launcher）；
    
3. 补充细节：Android 高版本（如 Android 12+）中，SystemServer 会在 WMS 模块中调用 `mBootAnimation.stop()` 间接触发该操作，确保动画退出与系统服务就绪的时序一致性，避免出现「动画早退导致黑屏」的问题；同时，SystemServer 也可能直接通过 `SystemProperties.set("service.bootanim.exit", "1")` 主动设置该属性，强制动画退出。
    

### （三）时机3：Launcher（桌面）准备就绪（用户可感知时机）

这是用户能直接感知的设置时机，由 Launcher 就绪状态触发，确保动画退出后用户能立即看到桌面，无缝衔接用户交互：

1. 触发条件：Launcher 是用户桌面应用，当它完成自身初始化、View 树绘制、桌面图标加载后，会向 SurfaceFlinger 申请「前台显示权限」；SurfaceFlinger 检测到 Launcher 就绪后，会间接通知 BootAnimation「无需继续显示」，或由 Launcher 通知 AMS，再由 AMS 通知 BootAnimation；
    
2. 源码联动：BootAnimation 接收通知后，调用 `property_set(EXIT_PROP_NAME, "1", "1")`，终止动画绘制，随后 SurfaceFlinger 切换显示图层，将 Launcher 桌面显示在屏幕上，用户感知到动画结束、桌面出现；
    
3. 补充细节：BootAnimation 会在每帧绘制完毕后，通过 `checkExit()` 方法主动校验 EXIT_PROP_NAME 的值，若检测到其被设为1，会立即调用 `requestExit()` 终止动画，确保响应及时性；同时，系统会记录动画从启动到 Launcher 就绪的时间，用于异常排查（如超时未进入桌面会触发系统自恢复）。
    

### （四）关键补充：设置为1的核心约束（必记）

- 1. 仅用于「正常退出」：EXIT_PROP_NAME 设为1，仅对应动画「正常完成播放」「系统就绪」「桌面就绪」三种正常场景，异常场景（如 Surface 失败）绝不会设置为1，而是设置为2；
    
- 2. 权限约束：设置时必须携带权限参数 "1"（如 `property_set(EXIT_PROP_NAME, "1", "1")`），确保只有系统进程和 BootAnimation 自身能修改该属性，防止普通应用恶意篡改；
    
- 3. 时序约束：设置为1的操作，必须在 BootAnimation 资源清理（cleanUp()）之前执行，确保系统能正常接收退出状态，避免资源释放后属性写入失败；
    
- 4. 双向校验：BootAnimation 会主动校验该属性值（每帧绘制后），SystemServer、SurfaceFlinger 会被动监听该属性值，双方同步状态后，动画才会真正退出，确保系统时序一致。
    

## 七、补充解析：Android 开机动画何时结束（核心答案）

结合前文 `property_set` 调用逻辑、BootAnimation 生命周期及系统启动流程，Android 开机动画的「结束时机」分为 **正常结束** 和 **异常结束** 两大类，均以「调用 `property_set(EXIT_PROP_NAME, value, "1");` 并终止 `threadLoop` 循环」为标志，具体时机如下，全程关联前文源码细节，保证上下文连贯：

### （一）正常结束时机（最常见，对应 value="1"）

正常结束是系统设计的默认流程，核心是「动画完成使命」或「系统就绪无需继续显示」，具体有3个关键时机，与前文调用场景完全对应：

1. **开机动画帧播放完毕**：这是最基础的结束时机。BootAnimation 会解码 `/system/media/bootanimation.zip` 中的所有动画帧，在 `threadLoop()` 主循环中逐帧绘制，当检测到 `mFrames.empty()`（所有帧已播放）时，会调用 `property_set(EXIT_PROP_NAME, "1", "1")`，标记 `mExitPending = true`，终止循环并退出，此时动画正常结束。
    
2. **SystemServer 启动完成并通知退出**：这是最核心的结束时机。SystemServer 是 Android 系统的核心服务管理器，负责启动 AMS、WMS、ATMS 等所有系统服务，当 SystemServer 完成所有核心服务初始化后，会通过 Binder 向 BootAnimation 发送退出通知（触发 `onSystemReady()` 回调），BootAnimation 接收通知后，调用 `property_set` 写入退出状态，随后退出，确保系统后续流程（如启动 Launcher）正常执行。
    
3. **桌面（Launcher）准备就绪**：这是用户可感知的结束时机。Launcher 是用户桌面应用，当 Launcher 完成初始化、绘制好桌面界面后，会向 SurfaceFlinger 申请显示权限，SurfaceFlinger 检测到 Launcher 就绪后，会间接通知 BootAnimation 退出，BootAnimation 调用 `property_set` 后终止，随后 SurfaceFlinger 切换显示图层，用户看到桌面，动画正式结束。
    

### （二）异常结束时机（对应 value="2"）

异常结束是动画无法正常执行的兜底流程，核心是「动画绘制所需的基础条件不满足」，导致无法继续显示，具体时机与前文异常场景完全对应，均会触发 `property_set(EXIT_PROP_NAME, "2", "1")`，具体如下：

1. **Surface 创建失败**：BootAnimation 依赖 SurfaceFlinger 提供的 Surface 作为绘制画布（关联前文 `waitForSurfaceFlinger()` 方法），若 `initSurface()` 方法调用失败（如 SurfaceFlinger 服务异常、窗口申请失败），动画无法绘制，会立即调用 `property_set` 写入异常状态，强制退出。
    
2. **动画帧解码异常**：若 `bootanimation.zip` 文件损坏、格式不支持，或解码过程中出现 IO 错误，BootAnimation 无法获取下一帧数据，无法继续播放，会触发异常处理逻辑，调用 `property_set` 后退出。
    
3. **核心服务异常**：若 SurfaceFlinger、系统属性服务（property_service）等核心服务崩溃或未就绪，BootAnimation 无法完成绘制提交或状态传递，会判定为异常，调用 `property_set` 强制退出，避免系统卡死。
    

### （三）关键补充：结束的标志的是什么？

无论正常还是异常结束，开机动画的「真正结束标志」是两个：

- 1. 调用 `property_set(EXIT_PROP_NAME, value, "1")`，向系统写入退出状态（value=1 正常，value=2 异常），供 SystemServer、SurfaceFlinger 同步状态；
    
- 2. BootAnimation 线程退出：`mExitPending = true` 终止 `threadLoop()` 死循环，执行 `cleanUp()` 清理 Surface、释放解码资源，随后 BootAnimation 线程（即前文 `boot->run()` 启动的线程）彻底退出，动画生命周期完全终止。
    

补充说明：动画结束后，`service.bootanim.exit` 属性会保留对应 value 值，可通过 `adb shell getprop service.bootanim.exit` 查看，用于调试排查动画退出异常（如返回 2 则说明动画异常退出，需排查 Surface 或解码问题）。

## 八、总结（贴合上下文，核心必记）

在 BootAnimation 场景中，`property_set(EXIT_PROP_NAME, value, "1");` 的核心作用是：**动画退出时，向系统写入「退出状态属性」，实现 BootAnimation 与 SystemServer、SurfaceFlinger 的跨进程状态通信，同时通过权限控制保证状态写入的安全性，是动画生命周期终止的核心触发操作**。

其中，EXIT_PROP_NAME 设置为1的核心时机的是「动画正常退出」，具体为「动画帧播放完毕」「SystemServer 启动完成通知」「Launcher 就绪」三种场景，均对应 value="1"，且设置时需遵循权限约束和时序约束；异常场景绝不会将其设为1，而是设为2，这是区分动画正常与异常退出的核心标志。

关联你之前解析的 BootAnimation 源码：该函数是 `boot->run()` 启动动画后，唯一的「退出触发方式」，承接动画绘制循环，衔接资源清理流程，其参数取值、调用时机直接决定动画能否正常退出、系统能否顺利进入后续启动阶段，也是 Native 层系统属性操作、跨进程通信的典型案例，更是面试高频考点。