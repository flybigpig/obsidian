
![[pic/Pasted image 20250710094523.png]]

Google 在 Android11 觉得 HIDL 那一套有点多余，把 HIDL HAL 弃用了。并提供了新的 AIDL HAL。

主要有以下几点变化：

- 一般情况下 HAL 是一个 binder 服务，注册到 ServiceManager，通过命令 `adb shell service list` 中有大量的 hal binder 服务。
- 通常，App 作为 binder 客户端与 Framework 中的系统进程（SystemServer SurfaceFlinger 等）通信，Framework 中的系统进程会作为 hal binder 服务的客户端，访问 hal。一般情况下，App 访问 hal 要经过两次跨进程通信
- 特殊情况下，比如某个硬件只有一个 App 使用，App 可以直接通过 binder 通信访问到 hal binder 服务。
- 某些 hal 对性能要求高（主要是显示相关的），hal 层是一个 so 库，系统进程通过 dlopen 的方式加载。这类 hal 叫 stable-c hal