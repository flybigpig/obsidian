
在 Android 系统中，`bootFinished` 函数的触发过程涉及多个组件的协同工作，以下是详细的触发机制：

### 1. **开机动画的启动**
开机动画（`bootanimation`）的启动是由 `SurfaceFlinger` 服务发起的。`SurfaceFlinger` 是 Android 系统中负责显示的核心服务。

- 在 `SurfaceFlinger` 的初始化过程中，会启动一个线程 `StartPropertySetThread`，该线程会设置系统属性 `service.bootanim.exit` 为 `0`，并启动 `bootanimation`。
- 这是通过设置系统属性 `ctl.start` 为 `bootanim` 来实现的，`init` 进程会监听这些属性变化，并启动 `bootanimation` 服务。

### 2. **`bootFinished` 函数的调用**
`bootFinished` 函数的作用是通知系统启动完成，并停止开机动画。该函数的调用路径如下：

- **`WindowManagerService` 调用**：在 `WindowManagerService` 的 `performEnableScreen` 方法中，会通过 `SurfaceControl.bootFinished()` 方法调用 `bootFinished`。
- **JNI 调用**：`SurfaceControl.bootFinished()` 方法通过 JNI 调用原生方法 `nativeBootFinished`，该方法进一步调用 `SurfaceComposerClient.bootFinished()`。
- **Binder 调用**：`SurfaceComposerClient.bootFinished()` 方法通过 Binder 机制调用 `SurfaceComposerAIDL.bootFinished()`，最终调用 `SurfaceFlinger.bootFinished()`。

### 3. **`bootFinished` 函数的实现**
在 `SurfaceFlinger` 的 `bootFinished` 函数中，会执行以下操作：

```cpp
void SurfaceFlinger::bootFinished() {
    if (mBootFinished == true) {
        ALOGE("Extra call to bootFinished");
        return;
    }
    mBootFinished = true;
    ...
    property_set("service.bootanim.exit", "1"); // 设置属性，通知开机动画退出
    ...
}
```

- 该函数会设置系统属性 `service.bootanim.exit` 为 `1`，通知 `bootanimation` 服务停止动画。

### 4. **开机动画的结束**
`bootanimation` 服务会不断检查系统属性 `service.bootanim.exit` 的值。当该属性值为 `1` 时，`bootanimation` 服务会调用 `requestExit` 方法，结束开机动画。

### 5. **触发 `BOOT_COMPLETED` 广播**
在 `bootFinished` 函数被调用后，系统会发送 `BOOT_COMPLETED` 广播，通知应用程序系统启动完成。

### 总结
`bootFinished` 函数的触发过程涉及多个组件的协同工作，从 `WindowManagerService` 到 `SurfaceFlinger`，再到 `bootanimation` 服务。通过设置系统属性和 Binder 通信，最终实现停止开机动画并通知系统启动完成。