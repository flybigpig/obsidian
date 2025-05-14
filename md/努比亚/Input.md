

`Android` 系统是由事件驱动的，而 `input` 是最常见的事件之一，用户的点击、滑动、长按等操作，都属于 `input` 事件驱动，其中的核心就是 `InputReader` 和 `InputDispatcher`。`InputReader` 和 `InputDispatcher` 是跑在 `SystemServer`进程中的两个 `native` 循环线程，负责读取和分发 `Input` 事件。整个处理过程大致流程如下：

1. `InputReader`负责从`EventHub`里面把`Input`事件读取出来，然后交给 `InputDispatcher` 进行事件分发；
2. `InputDispatcher`在拿到 `InputReader`获取的事件之后，对事件进行包装后，寻找并分发到目标窗口;
3. `InboundQueue`队列（“iq”）中放着`InputDispatcher`从`InputReader`中拿到的`input`事件；
4. `OutboundQueue`（“oq”）队列里面放的是即将要被派发给各个目标窗口App的事件；
5. `WaitQueue`队列里面记录的是已经派发给 `App`（“wq”），但是 `App`还在处理没有返回处理成功的事件；
6. `PendingInputEventQueue`队列（“aq”）中记录的是应用需要处理的`Input`事件，这里可以看到`input`事件已经传递到了应用进程；
7. `deliverInputEvent` 标识 `App` `UI Thread` 被 `Input` 事件唤醒；
8. `InputResponse` 标识 `Input` 事件区域，这里可以看到一个 `Input_Down` 事件 + 若干个 `Input_Move` 事件 + 一个 `Input_Up` 事件的处理阶段都被算到了这里；
9. `App` 响应处理`Input` 事件，内部会在其界面`View`树中传递处理。

  



以下是对 Android 系统中 InputReader 和 InputDispatcher 相关内容的具体分析：

### InputReader 和 InputDispatcher 的角色与运行环境

- **角色**：InputReader 负责从底层的 EventHub 读取输入事件，而 InputDispatcher 则负责将读取到的事件进行包装并分发给目标窗口，它们是 Android 系统中处理输入事件的核心组件。
- **运行环境**：二者均是跑在 SystemServer 进程中的 native 循环线程。SystemServer 进程是 Android 系统中非常重要的进程，它负责启动和管理系统的各种服务，InputReader 和 InputDispatcher 在其中运行，能够确保对输入事件进行高效、及时的处理。

### 事件处理流程与相关队列

- **事件读取与传递**：InputReader 从 EventHub 中读取输入事件，然后将其传递给 InputDispatcher。EventHub 是 Android 系统中用于接收各种输入设备事件的底层接口，它收集来自触摸屏、键盘、鼠标等设备的事件，为 InputReader 提供事件来源。
- **InboundQueue（“iq”）**：InputDispatcher 从 InputReader 拿到事件后，将其放入 InboundQueue。这个队列就像是一个临时存储区，存放着待处理的输入事件，等待 InputDispatcher 进一步处理。
- **事件包装与分发**：InputDispatcher 从 InboundQueue 取出事件，对其进行包装，然后寻找目标窗口并将事件分发出去。在这个过程中，InputDispatcher 需要根据事件的类型、来源以及当前系统的状态等信息，确定事件应该发送到哪个应用程序的哪个窗口。
- **OutboundQueue（“oq”）**：经过包装和目标窗口确定后的事件会被放入 OutboundQueue，这里的事件即将被派发给各个目标窗口 App，它是事件从系统到应用程序的最后一个 “中转” 环节。
- **WaitQueue（“wq”）**：当事件被派发给 App 后，如果 App 还在处理该事件且没有返回处理成功的结果，那么这个事件就会被记录在 WaitQueue 中。这表明系统正在等待 App 对事件的处理完成，以便进行后续的操作。
- **PendingInputEventQueue（“aq”）**：该队列记录的是应用需要处理的 Input 事件，当事件进入这个队列时，意味着输入事件已经成功传递到了应用进程，应用将从这个队列中获取事件并在其界面 View 树中进行处理。

### 事件相关标识与应用处理

- **deliverInputEvent**：它标识着 App 的 UI Thread 被 Input 事件唤醒。当输入事件到达应用进程时，会触发 UI Thread 的唤醒，使其能够处理相关的输入事件，更新界面状态等。
- **InputResponse**：它标识了 Input 事件区域，包含了一个 Input_Down 事件、若干个 Input_Move 事件和一个 Input_Up 事件的处理阶段。这说明在一个完整的用户操作（如触摸屏幕并进行滑动等）过程中，一系列相关的输入事件都被归为一个 InputResponse 区域内进行处理，以便系统和应用能够完整地跟踪和处理用户的操作。
- **App 响应处理**：App 在接收到输入事件后，会在其界面 View 树中进行传递处理。View 树是 Android 应用界面的一种数据结构，它包含了各种视图组件（View），事件会从根视图开始，沿着 View 树的层级结构向下传递，直到找到能够处理该事件的具体视图组件，从而实现对用户输入的响应，如点击按钮、滑动列表等操作的处理。