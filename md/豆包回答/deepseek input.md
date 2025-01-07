
## inputreader是如何通知inputDispatch启动的?

在 Android 的输入子系统中，`InputReader` 和 `InputDispatcher` 是两个核心组件，它们共同协作来处理输入事件。`InputReader` 负责从硬件设备读取原始输入事件并将其转换为 Android 输入事件，而 `InputDispatcher` 负责将这些事件分发给目标应用程序窗口。

`InputReader` 和 `InputDispatcher` 之间的协作是通过 **事件队列** 和 **线程唤醒机制** 实现的。以下是 `InputReader` 如何通知 `InputDispatcher` 启动的详细流程：
### 1. **InputReader 和 InputDispatcher 的关系**
- **`InputReader`**：
  - 运行在一个独立的线程中（`InputReaderThread`）。
  - 从输入设备（如触摸屏、键盘）读取原始事件，并将其转换为 Android 输入事件（如 `MotionEvent`、`KeyEvent`）。
- **`InputDispatcher`**：
  - 运行在另一个独立的线程中（`InputDispatcherThread`）。
  - 负责将 `InputReader` 生成的事件分发给目标应用程序窗口。

`InputReader` 和 `InputDispatcher` 之间通过 **共享的事件队列** 进行通信。当 `InputReader` 读取到新的事件时，它会将事件放入队列，并通知 `InputDispatcher` 进行处理。

---

### 2. **事件队列**
- **`InputReader` 的事件队列**：
  - `InputReader` 将生成的输入事件放入一个共享的队列中。
  - 这个队列是线程安全的，支持多线程的读写操作。
- **`InputDispatcher` 的事件队列**：
  - `InputDispatcher` 从队列中取出事件，并将其分发给目标应用程序窗口。

---

### 3. **通知机制**
`InputReader` 通过以下步骤通知 `InputDispatcher` 启动事件分发：

#### （1）**事件生成**
- `InputReader` 从输入设备读取原始事件，并将其转换为 Android 输入事件。
- 例如，触摸屏事件会被转换为 `MotionEvent`，按键事件会被转换为 `KeyEvent`。

#### （2）**事件入队**
- `InputReader` 将生成的事件放入共享的事件队列中。
- 这个队列是一个 **生产者-消费者模型**，`InputReader` 是生产者，`InputDispatcher` 是消费者。

#### （3）**唤醒 `InputDispatcher`**
- 当 `InputReader` 将新事件放入队列后，它会通过 **条件变量（Condition Variable）** 或 **信号量（Semaphore）** 唤醒 `InputDispatcher`。
- 具体的唤醒机制依赖于操作系统的线程同步原语。

#### （4）**事件分发**
- `InputDispatcher` 被唤醒后，会从事件队列中取出事件。
- 它根据事件类型和目标窗口的状态，将事件分发给合适的应用程序窗口。

---

### 4. **源码分析**
以下是 `InputReader` 和 `InputDispatcher` 协作的关键代码片段：

#### （1）`InputReader` 通知 `InputDispatcher`
在 `InputReader` 的 `loopOnce()` 方法中，`InputReader` 会读取输入事件并将其放入队列，然后通知 `InputDispatcher`：
```cpp
void InputReader::loopOnce() {
    // 读取输入事件
    RawEvent rawEvent;
    getEvents(&rawEvent);

    // 处理原始事件并生成 Android 输入事件
    processEvents(&rawEvent);

    // 将事件放入队列
    mQueuedListener->flush(); // 将事件放入共享队列

    // 唤醒 InputDispatcher
    mDispatcher->wake(); // 通知 InputDispatcher 有新事件
}
```

#### （2）`InputDispatcher` 的事件循环
在 `InputDispatcher` 的 `dispatchOnce()` 方法中，`InputDispatcher` 会等待事件并处理：
```cpp
void InputDispatcher::dispatchOnce() {
    // 等待新事件
    mLooper->pollOnce(-1); // 阻塞等待，直到被 InputReader 唤醒

    // 从队列中取出事件
    EventEntry* event = mInboundQueue.dequeue();

    // 分发事件
    dispatchEvent(event);
}
```

#### （3）线程同步机制
`InputReader` 和 `InputDispatcher` 之间的线程同步通常通过 **条件变量** 或 **信号量** 实现。例如：
```cpp
// InputReader 唤醒 InputDispatcher
mDispatcher->wake(); // 调用条件变量的 notify() 或信号量的 release()

// InputDispatcher 等待事件
mLooper->pollOnce(-1); // 调用条件变量的 wait() 或信号量的 acquire()
```

---

### 5. **总结**
- **`InputReader` 负责读取和转换输入事件**：
  - 从硬件设备读取原始事件，生成 Android 输入事件。
- **`InputDispatcher` 负责分发事件**：
  - 将事件分发给目标应用程序窗口。
- **通知机制**：
  - `InputReader` 通过共享的事件队列和线程同步机制（如条件变量或信号量）通知 `InputDispatcher` 启动事件分发。
- **协作流程**：
  1. `InputReader` 读取事件并放入队列。
  2. `InputReader` 唤醒 `InputDispatcher`。
  3. `InputDispatcher` 从队列中取出事件并分发。

通过这种协作机制，`InputReader` 和 `InputDispatcher` 能够高效地处理输入事件，确保用户操作的及时响应。


## binder通信oneway与非oneway的区别?oneway通信时需要注意什么?

在 Android 的 Binder 通信机制中，`oneway` 是一个重要的修饰符，用于标记 Binder 调用是单向的还是双向的。`oneway` 和非 `oneway` 的主要区别在于调用是否阻塞以及是否需要等待返回结果。

---

### 1. **`oneway` 与非 `oneway` 的区别**

#### （1）**非 `oneway` 调用（默认）**
- **阻塞调用**：
  - 调用方（客户端）会等待服务端处理完方法并返回结果。
  - 调用线程会被阻塞，直到服务端返回。
- **双向通信**：
  - 调用方可以接收服务端的返回值或异常信息。
- **适用场景**：
  - 需要获取服务端处理结果的场景，例如查询数据、执行同步操作等。

#### （2）**`oneway` 调用**
- **非阻塞调用**：
  - 调用方（客户端）不会等待服务端处理完方法，调用后立即返回。
  - 调用线程不会被阻塞。
- **单向通信**：
  - 调用方无法接收服务端的返回值或异常信息。
- **适用场景**：
  - 不需要获取服务端处理结果的场景，例如发送通知、触发异步操作等。

---

### 2. **`oneway` 调用的特点**
- **异步性**：
  - `oneway` 调用是异步的，调用方不会等待服务端处理。
- **无返回值**：
  - `oneway` 方法不能有返回值（必须返回 `void`）。
- **无异常传递**：
  - 服务端抛出的异常不会传递给调用方。
- **调用顺序保证**：
  - `oneway` 调用会按照调用顺序依次执行，但调用方无法知道服务端何时处理完成。

---

### 3. **`oneway` 调用时需要注意的事项**

#### （1）**方法定义**
- `oneway` 方法必须返回 `void`，不能有返回值。
- 方法参数可以是任意类型，但调用方无法获取服务端的处理结果。

示例：
```java
oneway void sendNotification(String message);
```

#### （2）**调用顺序**
- `oneway` 调用会按照调用顺序依次执行，但调用方无法知道服务端何时处理完成。
- 如果需要保证某些操作的顺序，可以在服务端内部实现同步机制。

#### （3）**异常处理**
- 服务端抛出的异常不会传递给调用方，因此调用方无法知道服务端是否处理成功。
- 如果需要对异常进行处理，可以在服务端记录日志或通过其他方式通知调用方。

#### （4）**性能优化**
- `oneway` 调用适合用于不需要返回结果的场景，可以减少调用方的等待时间，提高性能。
- 但过度使用 `oneway` 调用可能会导致服务端负载过高，需要根据实际场景合理使用。

#### （5）**线程安全**
- `oneway` 调用是非阻塞的，调用方可以快速返回并继续执行其他操作。
- 但服务端处理 `oneway` 调用时可能涉及多线程操作，需要确保线程安全。

---

### 4. **`oneway` 调用的使用场景**
- **发送通知**：
  - 例如，客户端向服务端发送一个通知，不需要等待服务端处理完成。
- **触发异步操作**：
  - 例如，客户端触发一个后台任务，不需要获取任务执行结果。
- **日志记录**：
  - 例如，客户端向服务端发送日志信息，不需要等待服务端确认。

---

### 5. **示例代码**

#### （1）定义 AIDL 接口
```java
// INotificationService.aidl
interface INotificationService {
    // 非 oneway 方法
    String getMessage();

    // oneway 方法
    oneway void sendNotification(String message);
}
```

#### （2）实现服务端
```java
public class NotificationService extends INotificationService.Stub {
    @Override
    public String getMessage() {
        return "Hello from service!";
    }

    @Override
    public void sendNotification(String message) {
        // 模拟处理通知
        System.out.println("Received notification: " + message);
    }
}
```

#### （3）客户端调用
```java
// 非 oneway 调用
String message = notificationService.getMessage();
System.out.println("Message from service: " + message);

// oneway 调用
notificationService.sendNotification("This is a notification");
```

---

### 6. **总结**
- **`oneway` 调用**：
  - 非阻塞，调用方不等待服务端处理。
  - 无返回值和异常传递。
  - 适合用于不需要返回结果的场景。
- **非 `oneway` 调用**：
  - 阻塞，调用方等待服务端处理并返回结果。
  - 适合用于需要获取服务端处理结果的场景。

在使用 `oneway` 调用时，需要注意方法定义、调用顺序、异常处理和性能优化等问题，以确保通信的可靠性和效率。


## binder通信中不同进程的指向同一个Binder服务的客户端请求服务端时使用的handle是否是相等的呢?比如AMS服务

在 Android 的 Binder 通信机制中，不同进程的客户端请求同一个 Binder 服务时，**使用的 `handle` 是相等的**。这是因为 Binder 驱动会为每个 Binder 服务分配一个唯一的 `handle`，并在系统范围内共享这个 `handle`。

---

### 1. **Binder 通信中的 `handle`**
- **`handle` 的作用**：
  - `handle` 是 Binder 驱动为每个 Binder 服务分配的唯一标识符。
  - 客户端通过 `handle` 来识别和访问特定的 Binder 服务。
- **`handle` 的分配**：
  - 当服务端注册一个 Binder 服务时，Binder 驱动会为其分配一个 `handle`。
  - 这个 `handle` 在系统范围内是唯一的，并且在不同进程中指向同一个 Binder 服务时是相等的。

---

### 2. **不同进程访问同一个 Binder 服务**
以 **AMS（ActivityManagerService）** 为例：
- **AMS 是系统服务**：
  - AMS 是一个全局的 Binder 服务，由系统进程（`system_server`）注册并提供服务。
- **客户端访问 AMS**：
  - 不同进程（如应用程序进程）通过 `ServiceManager` 获取 AMS 的 Binder 代理对象。
  - 每个客户端获取的 AMS 代理对象中，`handle` 是相同的，因为它们在 Binder 驱动中指向同一个 AMS 服务。

---

### 3. **`handle` 的传递和共享**
- **`handle` 的传递**：
  - 当客户端通过 `ServiceManager` 获取 Binder 服务时，`ServiceManager` 会返回该服务的 `handle`。
  - 这个 `handle` 会被封装在客户端的 Binder 代理对象中。
- **`handle` 的共享**：
  - 不同进程的客户端获取同一个 Binder 服务时，Binder 驱动会返回相同的 `handle`。
  - 这是因为 Binder 驱动维护了一个全局的 `handle` 映射表，确保每个 Binder 服务的 `handle` 是唯一的且一致的。

---

### 4. **示例：AMS 的 `handle`**
以下是一个简化的流程，说明不同进程访问 AMS 时 `handle` 的使用：

#### （1）AMS 注册
- 在系统启动时，AMS 会注册到 `ServiceManager` 中：
  ```java
  ServiceManager.addService(Context.ACTIVITY_SERVICE, ams);
  ```
- Binder 驱动为 AMS 分配一个唯一的 `handle`（例如 `handle = 1`）。

#### （2）客户端获取 AMS 代理
- 客户端通过 `ServiceManager` 获取 AMS 的代理对象：
  ```java
  IBinder binder = ServiceManager.getService(Context.ACTIVITY_SERVICE);
  ```
- `ServiceManager` 返回的 `IBinder` 对象中包含了 AMS 的 `handle`（例如 `handle = 1`）。

#### （3）不同进程的 `handle`
- 不同进程的客户端获取 AMS 代理时，Binder 驱动会返回相同的 `handle`（例如 `handle = 1`）。
- 这是因为所有客户端都指向同一个 AMS 服务。

---

### 5. **验证 `handle` 是否相等**
可以通过以下方式验证不同进程的客户端是否使用相同的 `handle`：
- 获取 Binder 代理对象的 `handle`：
  ```java
  int handle = ((BinderProxy) binder).getHandle();
  ```
- 在不同进程中打印 `handle`，会发现它们的值是相同的。

---

### 6. **总结**
- **`handle` 是唯一的**：
  - Binder 驱动为每个 Binder 服务分配一个唯一的 `handle`。
- **不同进程的 `handle` 相等**：
  - 不同进程的客户端访问同一个 Binder 服务时，使用的 `handle` 是相等的。
- **以 AMS 为例**：
  - 所有应用程序进程访问 AMS 时，使用的 `handle` 是相同的，因为它们都指向同一个 AMS 服务。

通过理解 `handle` 的作用和分配机制，可以更好地掌握 Binder 通信的原理和实现方式。

## InputDispatcher

`InputDispatcher` 是 Android 输入子系统中的一个核心组件，负责将输入事件（如触摸事件、按键事件等）从系统层分发给目标应用程序窗口。它是 Android 输入事件处理流程中的关键环节，连接了系统层的输入事件生成和应用层的事件处理。

---

### 1. **InputDispatcher 的作用**
`InputDispatcher` 的主要职责包括：
1. **接收输入事件**：
   - 从 `InputReader` 接收经过处理的输入事件（如 `MotionEvent` 或 `KeyEvent`）。
2. **确定目标窗口**：
   - 通过 `WindowManagerService` 确定输入事件的目标窗口（即当前聚焦的窗口或触摸事件发生的窗口）。
3. **分发事件**：
   - 将输入事件分发给目标窗口的 `ViewRootImpl`。
4. **处理事件超时和异常**：
   - 监控事件分发的超时情况，防止应用程序无响应（ANR）。
   - 处理事件分发的异常情况，如窗口丢失或应用程序崩溃。

---

### 2. **InputDispatcher 的工作流程**
`InputDispatcher` 的工作流程可以分为以下几个步骤：

#### （1）**接收输入事件**
- `InputReader` 从硬件层读取原始输入事件，并将其转换为 Android 输入事件（如 `MotionEvent` 或 `KeyEvent`）。
- `InputReader` 将处理后的输入事件传递给 `InputDispatcher`。

#### （2）**确定目标窗口**
- `InputDispatcher` 通过 `WindowManagerService` 获取当前聚焦的窗口或触摸事件发生的窗口。
- 对于触摸事件，`InputDispatcher` 会根据触摸坐标确定目标窗口。

#### （3）**分发事件**
- `InputDispatcher` 将输入事件发送给目标窗口的 `ViewRootImpl`。
- `ViewRootImpl` 是应用程序与系统输入事件传递的桥梁，负责将事件传递给应用程序的视图层级。

#### （4）**监控事件处理**
- `InputDispatcher` 会监控应用程序对事件的处理情况。
- 如果应用程序未能在规定时间内处理事件，`InputDispatcher` 会触发 ANR（Application Not Responding）机制。

---

### 3. **InputDispatcher 的关键机制**

#### （1）**事件分发策略**
- `InputDispatcher` 根据事件类型和目标窗口的状态决定如何分发事件。
  - 对于按键事件，通常分发给当前聚焦的窗口。
  - 对于触摸事件，根据触摸坐标确定目标窗口。

#### （2）**事件超时监控**
- `InputDispatcher` 会为每个事件设置一个超时时间（通常为 5 秒）。
- 如果应用程序未能在超时时间内处理事件，`InputDispatcher` 会触发 ANR。

#### （3）**事件拦截和过滤**
- `InputDispatcher` 可以根据系统策略拦截或过滤某些输入事件。
  - 例如，系统可能会拦截某些按键事件（如电源键、音量键）并优先处理。

#### （4）**多窗口支持**
- 在支持多窗口模式的设备上，`InputDispatcher` 需要根据窗口的层级关系和用户交互情况确定事件的目标窗口。

---

### 4. **InputDispatcher 与应用程序的交互**
`InputDispatcher` 通过 `ViewRootImpl` 与应用程序交互。具体流程如下：

#### （1）**事件传递到 ViewRootImpl**
- `InputDispatcher` 将输入事件发送给目标窗口的 `ViewRootImpl`。
- `ViewRootImpl` 是应用程序与系统输入事件传递的桥梁。

#### （2）**事件分发到视图层级**
- `ViewRootImpl` 将事件传递给应用程序的视图层级（View Hierarchy）。
- 事件的分发和处理遵循 Android 的触摸事件分发机制（`dispatchTouchEvent`、`onInterceptTouchEvent`、`onTouchEvent`）。

#### （3）**事件处理结果反馈**
- 应用程序处理完事件后，会将处理结果反馈给 `InputDispatcher`。
- 如果事件未被消费，`InputDispatcher` 可能会将事件传递给其他窗口或触发默认行为。

---

### 5. **InputDispatcher 的源码分析**
`InputDispatcher` 的源码位于 Android 系统的 `frameworks/native/services/inputflinger/` 目录下。以下是一些关键类和方法：

#### （1）**InputDispatcher.cpp**
- 这是 `InputDispatcher` 的核心实现文件。
- 主要方法包括：
  - `dispatchOnce()`：处理单个输入事件。
  - `dispatchMotionLocked()`：分发触摸事件。
  - `dispatchKeyLocked()`：分发按键事件。

#### （2）**InputChannel**
- `InputChannel` 是 `InputDispatcher` 与应用程序之间通信的通道。
- 每个窗口都有一个对应的 `InputChannel`，用于接收输入事件。

#### （3）**InputWindowHandle**
- `InputWindowHandle` 表示一个窗口的句柄，包含了窗口的属性和状态信息。
- `InputDispatcher` 使用 `InputWindowHandle` 来确定事件的目标窗口。

---

### 6. **InputDispatcher 的调试和优化**
#### （1）**调试输入事件**
- 可以使用 `adb shell dumpsys input` 命令查看 `InputDispatcher` 的状态和事件分发情况。
- 该命令会输出当前输入事件的分发状态、目标窗口信息以及事件队列。

#### （2）**优化事件分发**
- 减少视图层级：过多的视图层级会增加事件分发的延迟。
- 避免事件阻塞：确保应用程序能够及时处理输入事件，避免触发 ANR。

---

### 7. **总结**
`InputDispatcher` 是 Android 输入事件处理流程中的核心组件，负责将输入事件从系统层分发给目标应用程序窗口。它的主要职责包括：
1. 接收输入事件。
2. 确定目标窗口。
3. 分发事件。
4. 监控事件处理。

理解 `InputDispatcher` 的工作原理对于调试和优化 Android 应用程序的输入事件处理非常重要。通过合理设计视图层级和事件处理逻辑，可以提高应用程序的响应速度和用户体验。

## InputReader 如何通知InputDispatcher

1. **通信机制基础 - 管道（Pipe）**
    
    - **管道原理介绍**：在 Android 系统中，InputReader 和 InputDispatcher 之间主要通过管道（Pipe）进行通信。管道是一种传统的 Unix/Linux 进程间通信（IPC）方式，它提供了一种单向的数据传输通道。在这个场景中，管道的一端由 InputReader 用于写入数据（即输入事件相关的信息），另一端由 InputDispatcher 用于读取数据。
    - **创建管道过程**：在系统启动阶段，当 InputReader 和 InputDispatcher 被初始化时，会创建这个用于通信的管道。这个管道就像一条连接两者的 “数据管道”，使得 InputReader 能够将获取到的输入事件传递给 InputDispatcher。例如，通过系统调用`pipe()`函数创建管道，这个函数会返回两个文件描述符，一个用于管道的读端，一个用于管道的写端。
2. **事件通知过程**
    
    - **InputReader 写入事件数据**：当 InputReader 从硬件设备（如触摸屏、键盘等）读取到输入事件后，会将这些事件数据封装成合适的格式，然后通过管道的写端将数据写入管道。例如，对于一个触摸事件，InputReader 会将触摸点的坐标、触摸的类型（按下、抬起、移动）、时间戳等信息封装成一个数据结构，然后将这个数据结构写入管道。
    - **InputDispatcher 监听读取**：InputDispatcher 会在一个独立的线程中不断地监听管道的读端。它使用系统的文件 I/O 操作（如`read()`函数）来检查管道中是否有可读的数据。当 InputReader 写入数据到管道后，InputDispatcher 会检测到管道中有数据可读，从而知道有新的输入事件需要处理。这个过程就像是 InputReader 通过管道向 InputDispatcher 发送了一个 “信号”，告诉它有新的 “任务”（输入事件）来了。
3. **事件队列与后续处理**
    
    - **放入事件队列**：InputDispatcher 在读取到管道中的输入事件后，会将这些事件放入一个内部的事件队列（通常称为`mInboundQueue`）。这个队列是按照事件的产生时间来排序的，以确保输入事件能够按照它们实际发生的顺序进行处理。例如，如果有多个触摸事件依次发生，它们会在这个队列中按照发生的先后顺序排列，然后 InputDispatcher 会从队列头部开始依次处理这些事件。
    - **触发事件分发流程**：放入事件队列后，InputDispatcher 就会开始对这些输入事件进行后续的处理，包括确定目标窗口、通过 Binder IPC（进程间通信）将事件分发给目标窗口所在的应用进程等一系列流程，从而完成输入事件从读取到最终分发的整个过程。

除了管道通信，InputReader和InputDispatcher之间还有其他通信方式吗？

管道的读端和写端是如何与InputReader和InputDispatcher关联的？

分析InputDispatcher如何分发输入事件到目标窗口。


## 除了管道通信，InputReader和InputDispatcher之间还有其他通信方式吗？

1. **共享内存（Shared Memory）通信的潜在利用**
    
    - **共享内存原理在系统中的应用**：在 Android 系统架构下，共享内存是一种高效的进程间通信方式。虽然 InputReader 和 InputDispatcher 之间没有典型的、直接以共享内存为主要通信方式的设计，但在整个输入系统的底层机制中，它们可能会间接涉及共享内存相关的概念。
    - **数据缓存与共享内存关联**：例如，在硬件设备（如触摸屏、键盘等）产生输入事件后，这些事件数据可能会先存储在一块共享的缓存区域（与共享内存机制相关）。InputReader 在读取这些数据时，实际上是从这个共享缓存中获取信息，然后再通过管道将其传递给 InputDispatcher。这种共享缓存区域的使用可以提高数据的读取和处理效率，减少数据复制的开销，因为多个组件（可能包括其他与输入相关的底层模块）可以在一定程度上共享对这些数据的访问。
2. **信号（Signal）机制的辅助作用**
    
    - **信号机制简介**：信号是 Unix/Linux 系统中的一种异步事件通知机制，在 Android 系统中也有所应用。虽然不是 InputReader 和 InputDispatcher 之间的主要通信方式，但可以作为一种辅助通信手段。信号可以用于通知某个进程或组件某个事件的发生。
    - **在输入系统中的潜在应用**：在一些特殊情况下，例如当输入系统出现异常或者需要进行紧急处理的事件时，可能会利用信号机制。假设 InputReader 检测到硬件设备出现连续的错误输入信号（如触摸屏故障导致大量异常触摸点数据），它可以向系统发送一个信号，这个信号可能会间接影响 InputDispatcher 的工作状态，比如暂停当前的事件分发流程，等待 InputReader 对硬件问题进行处理或者对异常数据进行清理后再继续。不过这种情况相对较少，并且信号机制在这种场景下主要是起到一种协调和通知的作用，而不是像管道通信那样用于传递具体的输入事件数据。


```
void InputReader::loopOnce() {

    int32_t oldGeneration;

    int32_t timeoutMillis;

    bool inputDevicesChanged = false;

    std::vector<InputDeviceInfo> inputDevices;

    { // acquire lock

        AutoMutex _l(mLock);

        oldGeneration = mGeneration;

        timeoutMillis = -1;

        uint32_t changes = mConfigurationChangesToRefresh;

        if (changes) {

            mConfigurationChangesToRefresh = 0;

            timeoutMillis = 0;

            refreshConfigurationLocked(changes);

        } else if (mNextTimeout != LLONG_MAX) {

            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);

            timeoutMillis = toMillisecondTimeoutDelay(now, mNextTimeout);

        }

    } // release lock

    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);

    { // acquire lock

        AutoMutex _l(mLock);

        mReaderIsAliveCondition.broadcast();

        if (count) {

            processEventsLocked(mEventBuffer, count);

        }

        if (mNextTimeout != LLONG_MAX) {

            nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);

            if (now >= mNextTimeout) {

#if DEBUG_RAW_EVENTS

                ALOGD("Timeout expired, latency=%0.3fms", (now - mNextTimeout) * 0.000001f);

#endif

                mNextTimeout = LLONG_MAX;

                timeoutExpiredLocked(now);

            }

        }

        if (oldGeneration != mGeneration) {

            inputDevicesChanged = true;

            getInputDevicesLocked(inputDevices);

        }

    } // release lock

    // Send out a message that the describes the changed input devices.

    if (inputDevicesChanged) {

        mPolicy->notifyInputDevicesChanged(inputDevices);

    }

    // Flush queued events out to the listener.
    // This must happen outside of the lock because the listener could potentially call
    // back into the InputReader's methods, such as getScanCodeState, or become blocked
    // on another thread similarly waiting to acquire the InputReader lock thereby
    // resulting in a deadlock.  This situation is actually quite plausible because the
    // listener is actually the input dispatcher, which calls into the window manager,
    // which occasionally calls into the input reader.

    mQueuedListener->flush();

}
```