`synchronized` 是 Java/Android 中最基础的**内置锁（Intrinsic Lock）**，用于解决多线程并发访问共享资源的线程安全问题，其底层基于 JVM 的 **对象监视器（Monitor）** 和操作系统的 **互斥锁（Mutex）** 实现。以下从 “核心特性、使用方式、底层原理、优化机制、常见问题” 五个维度彻底讲透。

## 一、核心结论：synchronized 是什么？

`synchronized` 是**可重入的互斥锁**，核心作用是：

1. **互斥性**：同一时刻，只有一个线程能获取锁，其他线程阻塞等待，解决 “多个线程同时修改共享资源” 的问题；
2. **可重入性**：同一线程多次获取同一把锁不会死锁（如递归调用同步方法）；
3. **内存可见性**：释放锁时，线程会将工作内存的修改同步到主内存；获取锁时，线程会从主内存刷新最新数据到工作内存（替代 `volatile` 的内存可见性能力）；
4. **自动释放**：无论线程正常执行还是抛出异常，锁都会自动释放（对比 `Lock` 需手动 `unlock()` 更安全）。

## 二、synchronized 的三种使用方式

### 1. 修饰实例方法（锁：当前实例对象）

java

运行

```java
public class SyncDemo {
    // 锁对象是 SyncDemo 的实例（this）
    public synchronized void instanceMethod() {
        // 临界区：共享资源操作
        System.out.println(Thread.currentThread().getName() + " 执行实例方法");
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public static void main(String[] args) {
        SyncDemo demo = new SyncDemo();
        // 线程1和线程2竞争同一实例锁，串行执行
        new Thread(demo::instanceMethod, "线程1").start();
        new Thread(demo::instanceMethod, "线程2").start();

        // 线程3使用新实例，与线程1/2不竞争锁，并行执行
        SyncDemo demo2 = new SyncDemo();
        new Thread(demo2::instanceMethod, "线程3").start();
    }
}
```

**关键**：不同实例的同步方法互不阻塞，同一实例的同步方法串行执行。

### 2. 修饰静态方法（锁：当前类的 Class 对象）

java

运行

```java
public class SyncDemo {
    // 锁对象是 SyncDemo.class（全局唯一）
    public static synchronized void staticMethod() {
        System.out.println(Thread.currentThread().getName() + " 执行静态方法");
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public static void main(String[] args) {
        // 所有线程竞争同一 Class 锁，串行执行（无论是否同一实例）
        new Thread(SyncDemo::staticMethod, "线程1").start();
        new Thread(SyncDemo::staticMethod, "线程2").start();
        new Thread(() -> new SyncDemo().staticMethod(), "线程3").start();
    }
}
```

**关键**：静态同步方法的锁是类对象，全局唯一，所有实例共享该锁。

### 3. 修饰代码块（锁：自定义对象）

java

运行

```java
public class SyncDemo {
    // 自定义锁对象（推荐：避免用 this/Class 锁导致范围过大）
    private final Object lock = new Object();
    private static final Object staticLock = new Object();

    public void blockMethod() {
        // 锁：自定义实例锁
        synchronized (lock) {
            System.out.println(Thread.currentThread().getName() + " 执行实例代码块");
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    public static void staticBlockMethod() {
        // 锁：自定义静态锁
        synchronized (staticLock) {
            System.out.println(Thread.currentThread().getName() + " 执行静态代码块");
        }
    }
}
```

**核心优势**：精准控制锁的范围，避免 “锁粒度太大” 导致的性能问题（如仅锁定共享资源操作的代码段，而非整个方法）。

## 三、synchronized 底层原理（JVM + 操作系统）

### 1. 核心概念：对象头与 Monitor

Java 中所有对象都有 **对象头（Object Header）**，这是 synchronized 实现锁的核心载体：

|对象头结构|作用|
|---|---|
|Mark Word|存储对象的哈希码、GC 分代年龄、**锁状态（无锁 / 偏向锁 / 轻量级锁 / 重量级锁）**、持有锁的线程 ID 等；|
|Klass Pointer|指向对象所属类的 Class 指针（如 SyncDemo.class）；|
|Array Length|数组对象特有，存储数组长度；|

**Monitor（监视器 / 管程）**：

- 每个对象都关联一个 Monitor（由 JVM 维护），当线程获取锁时，会关联到 Monitor 的 `owner` 字段（标记持有锁的线程）；
- Monitor 维护一个 “等待队列”（Entry Set）和 “条件队列”（Wait Set），未获取锁的线程进入 Entry Set 阻塞，调用 `wait()` 的线程进入 Wait Set 等待。

### 2. 锁的升级过程（从偏向锁到重量级锁）

JDK 1.6 后，synchronized 引入 “锁升级” 机制（优化性能），锁状态从低到高依次为：

#### （1）偏向锁（Biased Locking）

- **适用场景**：单线程重复获取同一把锁（如主线程多次调用同步方法）；
- **原理**：对象头 Mark Word 记录当前线程 ID，后续该线程获取锁时，无需 CAS 竞争，直接返回（零成本获取锁）；
- **升级触发**：当有第二个线程竞争锁时，偏向锁升级为轻量级锁。

#### （2）轻量级锁（Lightweight Locking）

- **适用场景**：多线程交替获取锁（无持续竞争）；
- **原理**：线程获取锁时，通过 CAS 将对象头 Mark Word 替换为 “指向当前线程栈帧中锁记录的指针”，成功则获取锁；失败则自旋（循环重试）；
- **升级触发**：自旋次数达到阈值（默认 10 次）或自旋线程数超过 CPU 核心数的一半，升级为重量级锁。

#### （3）重量级锁（Heavyweight Locking）

- **适用场景**：多线程持续竞争锁（如高并发写）；
- **原理**：依赖操作系统的 Mutex 互斥锁实现，线程获取锁失败时，会从用户态切换到内核态，进入阻塞状态（CPU 开销大）；
- **释放锁**：持有锁的线程释放后，操作系统唤醒阻塞队列中的线程竞争锁。

**锁升级流程总结**：

plaintext

```plaintext
无锁 → 偏向锁（单线程）→ 轻量级锁（多线程交替）→ 重量级锁（多线程持续竞争）
```

> 注意：锁升级是**不可逆**的（偏向锁可撤销，但升级后不会降级）。

### 3. 字节码层面的实现

以同步代码块为例，编译后的字节码包含 `monitorenter` 和 `monitorexit` 指令：

java

运行

```java
// 原代码
synchronized (lock) {
    // 临界区
}

// 字节码核心指令
monitorenter // 获取锁（关联Monitor）
// 临界区字节码
monitorexit  // 正常释放锁
monitorexit  // 异常释放锁（保证锁一定释放）
```

- `monitorenter`：线程执行该指令时，尝试获取 Monitor 的所有权，成功则 `owner` 设为当前线程，失败则阻塞；
- `monitorexit`：线程执行该指令时，释放 Monitor 所有权，唤醒 Entry Set 中的线程。

## 四、synchronized 的优化机制（JDK 1.6+）

JDK 1.6 前，synchronized 是 “重量级锁”，性能差；JDK 1.6 引入以下优化，使其性能接近 `ReentrantLock`：

### 1. 偏向锁（Biased Locking）

- 消除无竞争情况下的锁获取开销，单线程场景下性能最优；
- 可通过 JVM 参数关闭：`-XX:-UseBiasedLocking`。

### 2. 轻量级锁 & 自旋锁（Spin Lock）

- 自旋锁避免线程从用户态切换到内核态（阻塞的核心开销），适用于 “锁持有时间短” 的场景；
- 自适应自旋：JVM 根据前一次自旋的结果，动态调整自旋次数（如上次自旋成功，本次增加次数；失败则减少）。

### 3. 锁消除（Lock Elimination）

JVM 编译器优化，自动移除 “不可能存在竞争的锁”：

java

运行

```java
// 示例：局部变量锁，JVM 会消除该锁
public void lockElimination() {
    Object lock = new Object();
    synchronized (lock) {
        System.out.println("无竞争的锁，会被消除");
    }
}
```

### 4. 锁粗化（Lock Coarsening）

将多次连续的锁获取 / 释放合并为一次：

java

运行

```java
// 优化前：多次加锁/释放
for (int i = 0; i < 1000; i++) {
    synchronized (lock) {
        // 简单操作
    }
}

// 优化后：一次加锁/释放（JVM 自动粗化）
synchronized (lock) {
    for (int i = 0; i < 1000; i++) {
        // 简单操作
    }
}
```

## 五、常见问题与实战避坑

### 1. 死锁问题（最典型）

**死锁条件**：互斥、持有并等待、不可剥夺、循环等待。

java

运行

```java
// 死锁示例：线程1持有lock1，等待lock2；线程2持有lock2，等待lock1
public class DeadLockDemo {
    private static final Object lock1 = new Object();
    private static final Object lock2 = new Object();

    public static void main(String[] args) {
        // 线程1
        new Thread(() -> {
            synchronized (lock1) {
                System.out.println("线程1持有lock1，等待lock2");
                try { Thread.sleep(100); } catch (InterruptedException e) {}
                synchronized (lock2) {
                    System.out.println("线程1获取lock2");
                }
            }
        }).start();

        // 线程2
        new Thread(() -> {
            synchronized (lock2) {
                System.out.println("线程2持有lock2，等待lock1");
                try { Thread.sleep(100); } catch (InterruptedException e) {}
                synchronized (lock1) {
                    System.out.println("线程2获取lock1");
                }
            }
        }).start();
    }
}
```

**解决死锁**：

- 固定锁的获取顺序（如先获取 lock1，再获取 lock2）；
- 使用 `tryLock(long timeout, TimeUnit unit)` 超时释放；
- 避免嵌套锁（尽量减少锁的层级）。

### 2. 锁粒度太大导致性能问题

**反例**：同步整个方法，包含无竞争的代码：

java

运行

```java
// 低效：锁包含了无需同步的日志打印
public synchronized void badMethod() {
    System.out.println("无竞争的日志打印"); // 无需同步
    // 共享资源操作（仅这行需要同步）
    sharedCount++;
}
```

**正例**：缩小锁范围到仅共享资源操作：

java

运行

```java
public void goodMethod() {
    System.out.println("无竞争的日志打印");
    synchronized (lock) {
        sharedCount++; // 仅同步共享资源操作
    }
}
```

### 3. 错误使用字符串常量作为锁

java

运行

```java
// 错误：字符串常量池导致锁冲突
public void wrongLock() {
    synchronized ("lock") { // 所有使用 "lock" 的地方共享同一锁
        // 临界区
    }
}
```

**原因**：字符串常量池会复用相同字符串，不同类 / 方法中的 `"lock"` 是同一对象，导致无关代码块竞争同一锁。**解决方案**：使用 `new String("lock")` 或自定义 Object 锁。

### 4. synchronized 与 volatile 的区别

|特性|synchronized|volatile|
|---|---|---|
|原子性|支持（保证临界区代码原子执行）|不支持（仅保证单次读 / 写原子）|
|内存可见性|支持|支持|
|有序性|支持（禁止指令重排序）|支持（禁止指令重排序）|
|适用场景|多线程写共享资源（如 i++）|多线程读、单线程写（如状态标记）|

## 六、Android 中的特殊场景

### 1. 主线程同步锁导致 ANR

Android 主线程（UI 线程）的 `Looper.loop()` 是无限循环，若主线程获取锁后阻塞（如等待子线程释放锁），会导致无法处理 UI 事件，触发 ANR（5 秒无响应）：

java

运行

```java
// 危险：主线程等待子线程释放锁，易ANR
private final Object lock = new Object();

@Override
protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    // 子线程持有锁
    new Thread(() -> {
        synchronized (lock) {
            try { Thread.sleep(10000); } catch (InterruptedException e) {}
        }
    }).start();

    // 主线程获取锁，阻塞10秒 → ANR
    synchronized (lock) {
        // UI操作
    }
}
```

**解决方案**：主线程避免获取 “可能被子线程长时间持有” 的锁，改用 Handler/Callback 异步处理。

### 2. 多进程场景下 synchronized 失效

synchronized 是**进程内锁**，不同进程的线程竞争同一 “锁对象” 时，因对象不共享（进程隔离），synchronized 无法保证互斥：

java

运行

```java
// 多进程下失效：进程A和进程B的 lock 是不同对象
private final Object lock = new Object();

// AndroidManifest 中声明多进程
<service android:name=".MyService" android:process=":remote" />
```

**解决方案**：多进程同步使用 `ContentProvider`、`SharedPreferences`（MODE_MULTI_PROCESS 已废弃）或 AIDL + Binder 锁。

## 七、总结

`synchronized` 是 Java/Android 线程安全的基础，核心要点：

1. **使用方式**：实例方法（锁 this）、静态方法（锁 Class）、代码块（自定义锁），优先用代码块缩小锁粒度；
2. **底层原理**：基于对象头 Mark Word 和 Monitor，JDK 1.6 后通过锁升级（偏向→轻量→重量）优化性能；
3. **避坑重点**：避免死锁、锁粒度太大、字符串常量锁、主线程长时间持锁；
4. **适用场景**：多线程写共享资源（如计数器、集合），简单场景优先用 synchronized（无需手动释放），复杂场景（如超时、公平锁）用 `ReentrantLock`。

理解 synchronized 的原理和优化机制，是面试中并发编程的核心考点，也是实际开发中解决线程安全问题的基础。