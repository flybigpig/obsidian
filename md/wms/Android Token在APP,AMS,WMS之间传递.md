
# 介绍

在Android系统中，`Token`（令牌）是一种用于标识和管理Activity、Service等组件的重要机制。它在应用层、`ActivityManagerService`（AMS）和`WindowManagerService`（WMS）之间传递，确保了组件之间的正确通信和管理。以下是`Token`在这些组件之间传递的详细过程：

### **1. 应用层发起请求**

当应用程序通过`startActivity`方法发起一个Activity启动请求时，会创建一个`Intent`对象，并将其传递给`ActivityManagerService`。

### **2. ActivityManagerService处理请求**

`ActivityManagerService`接收到来自应用层的`startActivity`请求后，会执行以下操作：

- **任务栈管理**：AMS会检查目标Activity是否已经存在，或者是否需要创建一个新的任务栈。
    
- **进程启动**：如果目标Activity所在的进程尚未启动，AMS会通过`Zygote`启动一个新的进程。
    
- **Token传递**：AMS会为新启动的Activity分配一个`Token`，这个`Token`是一个`IBinder`对象，用于在系统服务和应用进程之间标识Activity。
    

### **3. 应用进程接收Token**

当应用进程启动后，`ActivityManagerService`会通过`ApplicationThread`（一个Binder代理）调用`ActivityThread`的`scheduleLaunchActivity`方法，将`Token`传递给应用进程。

- **ActivityThread处理**：`ActivityThread`会根据`Token`和`Intent`信息，通过反射创建目标Activity的实例，并调用其生命周期方法（如`onCreate`、`onResume`等）。
    

### **4. WindowManagerService参与**

在Activity的生命周期中，当Activity需要显示窗口时，`ActivityThread`会通过`WindowManager`请求`WindowManagerService`（WMS）创建窗口。

- **Token传递给WMS**：在创建窗口的过程中，`Token`会被传递给WMS，WMS使用`Token`来关联窗口和对应的Activity。
    
- **窗口创建**：WMS根据`Token`和窗口属性创建窗口，并将其添加到显示列表中。
    

### **5. 窗口绘制**

- **ViewRootImpl初始化**：`WindowManagerService`会创建一个`ViewRootImpl`对象，它是窗口绘制的核心组件。
    
- **Surface创建**：WMS通知`SurfaceFlinger`创建一个`Surface`，用于窗口的绘制。
    
- **渲染完成**：应用进程通过OpenGL等图形库在`Surface`上绘制界面，并最终显示到屏幕上。
    

### **总结**

`Token`在Android系统中扮演着关键的角色，它在应用层、`ActivityManagerService`和`WindowManagerService`之间传递，确保了Activity的正确启动、窗口的创建和管理。通过`Token`，系统能够准确地识别和管理各个组件，从而实现高效的跨进程通信和资源管理。


----


# 通信

在 Android 系统中，Token 在保证 APP 和 ActivityManagerService（AMS）之间通信的安全性方面发挥着关键作用，下面从多个方面详细阐述其保障机制：

### 1. 唯一标识与身份验证

  

- **唯一标识生成**：当 APP 发起一个请求（如启动 Activity）时，AMS 会为该请求生成一个唯一的 Token，通常是一个 `IBinder` 对象。这个 Token 就像一把 “钥匙”，唯一对应着该请求或操作。例如，在启动 Activity 时，AMS 会为对应的 `ActivityRecord` 对象生成一个 Token：

  

收起

java

```
ActivityRecord r = new ActivityRecord(mService, callerApp, callingPid, callingUid,
        callingPackage, intent, resolvedType, aInfo, mService.getGlobalConfiguration(),
        resultRecord, resultWho, requestCode, componentSpecified, voiceSession != null,
        mSupervisor, checkedOptions, sourceRecord);
r.token = new Binder();
```

  

- **身份验证机制**：在后续的通信过程中，APP 和 AMS 都会使用这个 Token 来验证对方的身份。当 APP 向 AMS 发送进一步的请求时，会携带这个 Token，AMS 接收到请求后，会检查 Token 的有效性。只有 Token 匹配且有效时，AMS 才会处理该请求，从而防止恶意请求的干扰。

### 2. 防止请求伪造

  

- **Token 的绑定与关联**：Token 与特定的请求和操作紧密绑定。例如，在启动 Activity 的场景中，Token 与特定的 `ActivityRecord` 关联，它代表了该 Activity 启动请求的唯一性。当 APP 发送请求时，AMS 可以通过检查 Token 来确定该请求是否是之前合法发起的请求的延续。
- **防止重放攻击**：由于 Token 是唯一且与特定操作关联的，即使攻击者截获了请求和 Token，也无法使用该 Token 再次发起相同的请求。因为 AMS 会记录每个 Token 的使用状态，一旦一个 Token 被使用过，再次使用相同 Token 的请求会被视为无效请求而被拒绝。

### 3. 跨进程通信的安全性

  

- **Binder 机制的安全性**：Token 本质上是一个 `IBinder` 对象，它基于 Android 的 Binder 机制进行跨进程通信。Binder 机制本身具有一定的安全性，它通过内核空间的 Binder 驱动来实现进程间的通信，对通信过程进行了严格的管理和控制。
- **权限检查**：在使用 Token 进行通信时，AMS 会结合 Android 的权限管理机制对请求进行权限检查。即使请求携带了有效的 Token，如果 APP 没有相应的权限，AMS 也会拒绝该请求。例如，某些敏感操作（如启动系统级 Activity）需要特定的权限，AMS 会在接收到请求时检查 APP 是否具备这些权限。

### 4. 生命周期管理与有效性控制

  

- **Token 的生命周期管理**：Token 具有自己的生命周期，它与对应的操作或请求的生命周期相关联。当操作完成或请求过期时，AMS 会销毁对应的 Token。例如，当一个 Activity 被销毁时，与之关联的 Token 也会被清除，这样可以避免 Token 被滥用。
- **有效性检查**：在每次通信时，AMS 都会检查 Token 的有效性。如果 Token 已经过期或被销毁，AMS 会拒绝处理相关请求，从而保证通信的安全性。

  

通过以上多种机制，Token 有效地保证了 APP 和 AMS 之间通信的安全性，防止了恶意请求、请求伪造和重放攻击等安全问题，确保了 Android 系统中应用程序和系统服务之间通信的可靠性和稳定性。


# 作用

在Android系统中，`Token`（令牌）是一种用于标识和管理组件的重要机制，它在应用层、`ActivityManagerService`（AMS）和`WindowManagerService`（WMS）之间传递，确保了组件之间的正确通信和管理。以下是`Token`在Android系统中的主要作用：

### **1. 标识和管理Activity**

`Token`用于标识和管理Activity。每个Activity都有一个唯一的`Token`，这个`Token`是一个`IBinder`对象，用于在系统服务和应用进程之间标识Activity。

### **2. 跨进程通信**

`Token`在跨进程通信中起着关键作用。当应用进程启动一个新的Activity时，`ActivityManagerService`会为这个Activity分配一个`Token`，并将其传递给应用进程。应用进程通过`Token`与`WindowManagerService`通信，请求创建窗口。

### **3. 窗口管理**

`Token`还用于窗口管理。当Activity需要显示窗口时，`ActivityThread`会通过`WindowManager`请求`WindowManagerService`创建窗口。在这个过程中，`Token`被传递给`WindowManagerService`，用于关联窗口和对应的Activity。

### **4. 安全性**

`Token`机制在身份验证和授权方面具有重要作用。通过使用`Token`，客户端无需在每次请求时都提供用户名和密码，降低了敏感信息泄露的风险。同时，服务器可以通过验证`Token`来确保请求来自已授权的用户。

### **5. 简化认证流程**

`Token`机制可以简化认证流程。用户登录成功后，服务器会生成一个`Token`并将其返回给客户端。客户端在后续请求中携带该`Token`，以证明用户身份并获取相应的资源。

### **6. 权限管理**

`Token`还可以用于权限管理。服务器根据`Token`中的权限信息，决定用户可以访问哪些资源。

### **7. 优化性能**

通过使用`Token`，可以减少服务器的查询操作，优化性能。在初始阶段，客户端可以通过每次请求都带上用户名和密码来唯一确认一个请求，但这会增加服务器的压力。使用`Token`后，服务器可以快速验证用户身份，提高效率。

### **总结**

`Token`在Android系统中扮演着关键的角色，它在应用层、`ActivityManagerService`和`WindowManagerService`之间传递，确保了Activity的正确启动、窗口的创建和管理。此外，`Token`机制在身份验证、授权、简化认证流程和优化性能方面具有重要作用。