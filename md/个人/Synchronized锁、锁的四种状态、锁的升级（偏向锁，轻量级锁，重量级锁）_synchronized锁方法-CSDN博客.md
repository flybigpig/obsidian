---
created: 2025-04-27T17:08:25 (UTC +08:00)
tags: [synchronized锁方法]
source: https://blog.csdn.net/m0_64422133/article/details/143506109?ops_request_misc=%257B%2522request%255Fid%2522%253A%2522b25282d40b7a127670c531603dbb3ed8%2522%252C%2522scm%2522%253A%252220140713.130102334..%2522%257D&request_id=b25282d40b7a127670c531603dbb3ed8&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~top_positive~default-1-143506109-null-null.142^v102^pc_search_result_base9&utm_term=synchronized&spm=1018.2226.3001.4187
author: 成就一亿技术人!
---
**目录**

[1\. Synchronized锁](#1.%20Synchronized%E9%94%81)

[1.1 介绍](#1.1%20%E4%BB%8B%E7%BB%8D)

[1.2 三种应用方式★](#1.2%20%E4%B8%89%E7%A7%8D%E5%BA%94%E7%94%A8%E6%96%B9%E5%BC%8F%E2%98%85)

[1.2.1 synchronized同步方法](#1.2.1%20synchronized%E5%90%8C%E6%AD%A5%E6%96%B9%E6%B3%95)

[1.2.2 synchronized 同步静态方法](#1.2.2%20synchronized%20%E5%90%8C%E6%AD%A5%E9%9D%99%E6%80%81%E6%96%B9%E6%B3%95)

[1.2.3 synchronized 同步代码块](#1.2.3%20synchronized%20%E5%90%8C%E6%AD%A5%E4%BB%A3%E7%A0%81%E5%9D%97)

[1.3 Synchronized锁底层原理](#1.3%C2%A0Synchronized%E9%94%81%E5%BA%95%E5%B1%82%E5%8E%9F%E7%90%86)

[1.3.1 简答](#1.3.1%20%E7%AE%80%E7%AD%94)

[1.3.2 详述](#1.3.2%C2%A0%E8%AF%A6%E8%BF%B0)

[1. Monitor对象](#1.%C2%A0Monitor%E5%AF%B9%E8%B1%A1)

[2\. Monitor与对象锁关联时 具体的流程：](#2.%20Monitor%E4%B8%8E%E5%AF%B9%E8%B1%A1%E9%94%81%E5%85%B3%E8%81%94%E6%97%B6%20%E5%85%B7%E4%BD%93%E7%9A%84%E6%B5%81%E7%A8%8B%EF%BC%9A)

[2\. 锁的升级](#2.%20%E9%94%81%E7%9A%84%E5%8D%87%E7%BA%A7)[★](#1.2%20%E4%B8%89%E7%A7%8D%E5%BA%94%E7%94%A8%E6%96%B9%E5%BC%8F%E2%98%85)

[3. 偏向锁（Biased Locking）](#3.%C2%A0%E5%81%8F%E5%90%91%E9%94%81%EF%BC%88Biased%20Locking%EF%BC%89)

[4. 轻量级锁](#4.%C2%A0%E8%BD%BB%E9%87%8F%E7%BA%A7%E9%94%81)

[5\. 偏向锁、轻量级锁、重量级锁对比](#5.%20%E5%81%8F%E5%90%91%E9%94%81%E3%80%81%E8%BD%BB%E9%87%8F%E7%BA%A7%E9%94%81%E3%80%81%E9%87%8D%E9%87%8F%E7%BA%A7%E9%94%81%E5%AF%B9%E6%AF%94)

[6\. 偏向锁、轻量级锁 底层原理](#6.%20%E5%81%8F%E5%90%91%E9%94%81%E3%80%81%E8%BD%BB%E9%87%8F%E7%BA%A7%E9%94%81%20%E5%BA%95%E5%B1%82%E5%8E%9F%E7%90%86)

[7.小结](#7.%E5%B0%8F%E7%BB%93)[★](#1.2%20%E4%B8%89%E7%A7%8D%E5%BA%94%E7%94%A8%E6%96%B9%E5%BC%8F%E2%98%85)

* * *

## 1\. Synchronized锁

### 1.1 介绍

> **介绍：**Synchronized【对象锁】采用互斥的方式让同一时刻至多只有一个线程能持有【对象锁】，其它线程再想获取这个【对象锁】时就会阻塞住。

**Java中提供了两种实现同步的基础语义：synchronized方法和synchronized块**。

### 1.2 三种应用方式★

synchronized 关键字最主要有以下 3 种应用方式：

+   **同步方法：为当前对象（[this](https://javabetter.cn/oo/this-super.html "this")）加锁**，进入同步代码前要获得**当前对象的锁**；
+   **同步静态方法：为当前类加锁**（锁的是 [Class 对象](https://javabetter.cn/basic-extra-meal/fanshe.html "Class 对象")），进入同步代码前要获得**当前类的锁**；
+   **同步代码块：指定加锁对象**，对给定对象加锁，进入同步代码库前要获得**给定对象的锁**。

举例：

#### **1.2.1 synchronized同步方法**

通过在方法声明中加入 synchronized 关键字，可以保证在任意时刻，只有一个线程能执行该方法

代码：

```java
public class AccountingSync implements Runnable {
    //共享资源(临界资源)
    static int i = 0;
    // synchronized 同步方法
    public synchronized void increase() {
        i ++;
    }
    @Override
    public void run() {
        for(int j=0;j<1000000;j++){
            increase();
        }
    }
    public static void main(String args[]) throws InterruptedException {
        AccountingSync instance = new AccountingSync();
        Thread t1 = new Thread(instance);
        Thread t2 = new Thread(instance);
        t1.start();
        t2.start();
        t1.join();
        t2.join();
        System.out.println("static, i output:" + i);
    }
}
```

运行结果：

```java
/**
 * 输出结果:
 * static, i output:2000000
 */
```

上面代码 如果在方法 increase() 前不加 synchronized，因为 i++ 不具备原子性，所以最终结果会小于 2000000

解释：

> 如果`increase()`方法没有被声明为`synchronized`，那么在多线程环境下，两个线程可能会同时执行`i++`操作。这时，可能会发生以下情况：
> 
> +   线程A读取了`i`的值。
> +   线程B读取了`i`的值。
> +   线程A将`i`的值增加1，并写回内存。
> +   线程B也将`i`的值增加1，并写回内存。
> 
> 由于线程A和线程B读取的是同一个`i`的值，并且都对这个值增加了1，所以实际上`i`只增加了1，而不是2。这就导致了`i`的最终值小于预期的`2000000`。
> 
> 这种情况被称为“丢失更新”，因为线程B的操作覆盖了线程A的操作，导致线程A对`i`的增加没有被计入最终结果。

注意：一个对象只有一把锁，当一个线程获取了该对象的锁之后，其他线程无法获取该对象的锁，所以无法访问该对象的其他 synchronized 方法，但是其他线程还是可以访问该对象的其他非 synchronized 方法。

#### 1.2.2 synchronized 同步静态方法

如果一个线程 A 需要访问对象 obj1 的 synchronized 方法 f1(当前对象锁是 obj1)，另一个线程 B 需要访问对象 obj2 的 synchronized 方法 f2(当前对象锁是 obj2)，这样是允许的,

但是**线程安全是无法保证**的，例如：

```java
public class AccountingSyncBad implements Runnable {
    //共享资源(临界资源)
    static int i = 0;
    // synchronized 同步方法
    public synchronized void increase() {
        i ++;
    }

    @Override
    public void run() {
        for(int j=0;j<1000000;j++){
            increase();
        }
    }

    public static void main(String args[]) throws InterruptedException {
        // new 两个AccountingSync新实例
        Thread t1 = new Thread(new AccountingSyncBad());
        Thread t2 = new Thread(new AccountingSyncBad());
        t1.start();
        t2.start();
        t1.join();
        t2.join();
        System.out.println("static, i output:" + i);
    }
}
```

输出结果：

```
/**
 * 输出结果:
 * static, i output:1224617
 */
```

上述代码与前面不同的是，我们创建了两个对象 AccountingSyncBad，然后启动两个不同的线程对共享变量 i 进行操作，但很遗憾，操作结果是 1224617 而不是期望的结果 2000000。

因为上述代码犯了严重的错误，虽然使用了 synchronized 同步 increase 方法，但却 new 了两个不同的对象，这也就意味着存在着两个不同的对象锁，因此 t1 和 t2 都会进入各自的对象锁，也就是说 t1 和 t2 线程使用的是不同的锁，因此线程安全是无法保证的。

> 每个对象都有一个对象锁，不同的对象，他们的锁不会互相影响。

解决这种问题的的方式是将 synchronized 作用于静态的 increase 方法，这样的话，对象锁就锁的是当前的类，由于无论创建多少个对象，类永远只有一个，所有在这样的情况下对象锁就是唯一的。

#### 1.2.3 synchronized 同步代码块

某些情况下，我们编写的方法代码量比较多，存在一些比较耗时的操作，而需要同步的代码块只有一小部分，如果直接对整个方法进行同步，可能会得不偿失，此时我们可以使用同步代码块的方式对需要同步的代码进行包裹。

示例如下：

```java
public class AccountingSync2 implements Runnable {
    static AccountingSync2 instance = new AccountingSync2(); // 饿汉单例模式

    static int i=0;

    @Override
    public void run() {
        //省略其他耗时操作....
        //使用同步代码块对变量i进行同步操作,锁对象为instance
        synchronized(instance){
            for(int j=0;j<1000000;j++){
                i++;
            }
        }
    }

    public static void main(String[] args) throws InterruptedException {
        Thread t1=new Thread(instance);
        Thread t2=new Thread(instance);
        t1.start();t2.start();
        t1.join();t2.join();
        System.out.println(i);
    }
}
```

输出结果：

```
/**
 * 输出结果:
 * 2000000
 */
```

我们将 synchronized 作用于一个给定的实例对象 instance，即当前实例对象就是锁的对象，当线程进入 synchronized 包裹的代码块时就会要求当前线程持有 instance 实例对象的锁，如果当前有其他线程正持有该对象锁，那么新的线程就必须等待，这样就保证了每次只有一个线程执行 `i++` 操作。

当然除了用 instance 作为对象外，我们还可以使用 this 对象(代表当前实例)或者当前类的 Class 对象作为锁，如下代码：

```
//this,当前实例对象锁
synchronized(this){
    for(int j=0;j<1000000;j++){
        i++;
    }
}
//Class对象锁
synchronized(AccountingSync.class){
    for(int j=0;j<1000000;j++){
        i++;
    }
}
```

### 1.3 Synchronized锁底层原理

#### 1.3.1 简答

![](https://i-blog.csdnimg.cn/direct/0cf47f9d1c14460782e03d1f9a2d6f88.png)

#### 1.3.2 详述

synchronized 底层使用的JVM级别中的**Monitor (监视器)**来决定当前线程是否获得了锁，如果某一个线程获得了锁，在没有释放锁之前，其他线程是不能或得到锁的。synchronized 属于悲观锁。

![](https://i-blog.csdnimg.cn/direct/6126a451361d4e9fb0c12c60abb6512c.png)

如果使用 synchronized 给对象上锁（**重量级锁**）之后，对象会交给Monitor（监视器）管理，

##### 1. Monitor对象

`monitor`对象存在于每个Java对象的对象头中，`synchronized` 锁便是通过这种方式获取锁的，也是

首先需要明确的一点是：**Java 多线程的锁都是基于对象的**，Java 中的每一个对象都可以作为一个锁。还有一点需要注意的是，我们常听到的**类锁**其实也是对象锁

为什么Java中任意对象可以作为锁的原因？

Monitor**内部维护了三个变量：**

+   `WaitSet`：保存处于Waiting状态的线程
    
+   `EntryList`：保存处于Blocked状态的线程
    
+   `Owner`：持有锁的线程
    

##### **2\. Monitor与对象锁关联时 具体的流程：**

> 1\. 代码进入synchorized代码块，先让lock（对象锁）关联monitor（监视器），然后判断Owner是否有线程持有；  
> 2\. 如果没有线程持有，则让当前线程持有，表示该线程获取锁成功；  
> 3\. 如果有线程持有，则让当前线程进入entryList进行阻塞，如果Owner持有的线程已经释放了锁，在EntryList中的线程去竞争锁的持有权（非公平即阻塞队列中的线程会出现插队）；  
> 4\. 如果代码块中调用了wait()方法，则会进去WaitSet中进行等待。

**面试中你只回答以上 Synchronized锁底层原理，相当于说了一半，请继续往下看** 

## 2\. 锁的升级

**面试官追问：Monitor实现的锁属于重量级锁，你了解过锁升级吗？**

+   Monitor实现的锁属于重量级锁，里面涉及到了用户态和内核态的切换、进程的上下文切换，成本较高，性能比较低。

+   在JDK 1.6引入了两种新型锁机制：**偏向锁**和**轻量级锁**，它们的引入是为了解决在没有多线程竞争或基本没有竞争的场景下因使用传统锁机制带来的性能开销问题。

**synchronized的锁升级**  
在Java中，锁的升级过程是为了在不同的竞争情况下提供最佳的性能和资源利用。锁的升级过程可以分为以下几个阶段：

> +   **无锁状态：**在对象被创建时，默认处于无锁状态。此时，任何线程都可以自由地访问和修改对象的数据，没有任何同步措施。
> +   **偏向锁（Biased Locking）：**当一个线程第一次访问一个对象时，会尝试获取偏向锁。**只**在第一次获得锁时，会**有一个CAS操作**，之后该线程再获取锁，只需要判断mark word中是否是自己的线程id即可，而不是开销相对较大的CAS命令
> +   **轻量级锁（Lightweight Locking）：**如果另一个线程尝试获取已经被偏向的锁，偏向锁就会升级为轻量级锁。**轻量级修改了对象头的锁标志，相对重量级锁性能提升很多**。**每次修改都是CAS操作**，保证原子性。如果CAS成功，线程可以继续执行，否则就会进一步升级为重量级锁。
> +   **重量级锁（Heavyweight Locking）：**如果轻量级锁的CAS操作失败，表示存在竞争情况，锁就会升级为重量级锁。重量级锁底层使用的Monitor实现，里面涉及到了用户态和内核态的切换、进程的上下文切换，成本较高，**性能比较低**。

更加详细的锁升级流程可以参考这里 ：

[synchronized到底锁的什么？偏向锁、轻量级锁、重量级锁到底是什么？](https://javabetter.cn/thread/synchronized.html#%E9%94%81%E7%9A%84%E5%8D%87%E7%BA%A7%E6%B5%81%E7%A8%8B "synchronized到底锁的什么？偏向锁、轻量级锁、重量级锁到底是什么？")

**一旦锁发生了竞争，都会升级为重量级锁**

锁的升级过程是为了提供最佳的性能和资源利用。在无竞争情况下，偏向锁可以减少锁操作的开销。在轻量级竞争情况下，轻量级锁可以提供更好的性能。而在重度竞争情况下，重量级锁可以保证线程的同步和数据的一致性。

## 3. 偏向锁（Biased Locking）

**偏向锁是为了减少在单线程环境下锁的获取与释放开销而引入的机制。**

**核心思想**：当一个线程第一次访问一个同步代码块时，它会尝试获取偏向锁，如果成功获取，后续的访问都无需竞争，直接获得锁(以后该线程在进入和退出同步块时不需要进行CAS操作来加锁和解锁。)。

只有在其他线程尝试获取锁时，偏向锁才会升级为轻量级锁或重量级锁。

**使用背景**：

+   适用于大多数情况下只被一个线程访问的场景。
    
+   在无竞争的情况下，偏向锁可以显著减少获取锁的开销，提升性能。（它允许同一个线程在后续获取锁时无需进行额外的同步操作，几乎消除了锁的竞争开销。）
    

## 4. 轻量级锁

轻量级锁是在竞争不激烈的情况下，为了减少锁的开销而引入的机制。当一个线程尝试获取一个已经被偏向锁持有的锁时，会尝试将锁升级为轻量级锁。升级的过程中，会使用CAS（Compare and Swap）操作来尝试获取锁。如果获取成功，线程就可以进入临界区，执行同步操作。如果获取失败，表示有其他线程竞争锁，锁会膨胀为重量级锁。

**使用背景**：

+   适用于线程交替执行同步代码块的情况，尤其是在多个线程短时间内轮流申请同一锁资源，而非长时间持续竞争的场景中。
    
+   轻量级锁能够有效地减少线程之间的同步开销，优化程序执行效率。
    

**偏向锁对比轻量级锁**

偏向锁只会执行一次CAS操作，而轻量级锁在发生锁的竞争和释放时每次都会执行cas的操作，会造成一定的性能开销。

## 5\. 偏向锁、轻量级锁、重量级锁对比

| 特性 | 偏向锁 | 轻量级锁 | 重量级锁 |
| --- | --- | --- | --- |
| **优点** | \- 无竞争时性能极高 | \- 低竞争时性能较好 | \- 确保线程安全，适用于高竞争环境 |
|  | \- 减少锁操作的开销 | \- 避免线程阻塞和上下文切换开销 | \- 操作系统级别的锁，适用于需要严格同步的场景 |
| **缺点** | \- 竞争发生时需要额外的锁升级开销 | \- 竞争发生时可能会膨胀为重量级锁 | \- 线程阻塞和唤醒开销大 |
|  | \- 适用于单线程环境 | \- 竞争激烈时性能下降 | \- 可能导致系统资源占用高 |
| **使用场景** | \- 适用于单线程或几乎没有锁竞争的场景 | \- 适用于低竞争的多线程环境 | \- 适用于高锁竞争的多线程环境 |
| **锁获取** | \- 无需额外操作，直接标记偏向 | \- 通过CAS操作尝试获取 | \- 操作系统互斥量，可能导致线程阻塞 |
| **锁释放** | \- 无需额外操作，自动释放 | \- 通过CAS操作尝试释放 | \- 操作系统互斥量，需要显式释放 |
| **锁膨胀** | \- 竞争发生时升级为轻量级锁或重量级锁 | \- 竞争激烈时膨胀为重量级锁 | \- 不会发生膨胀，始终为重量级锁 |

各种锁的优缺点对比（来自《Java 并发编程的艺术》）：

| 锁 | 优点 | 缺点 | 适用场景 |
| --- | --- | --- | --- |
| 偏向锁 | 加锁和解锁不需要额外的消耗，和执行非同步方法比仅存在纳秒级的差距。 | 如果线程间存在锁竞争，会带来额外的锁撤销的消耗。 | 适用于只有一个线程访问同步块场景。 |
| 轻量级锁 | 竞争的线程不会阻塞，提高了程序的响应速度。 | 如果始终得不到锁竞争的线程使用自旋会消耗 CPU。 | 追求响应时间。同步块执行速度非常快。 |
| 重量级锁 | 线程竞争不使用自旋，不会消耗 CPU。 | 线程阻塞，响应时间缓慢。 | 追求吞吐量。同步块执行时间较长。 |

## 6\. 偏向锁、轻量级锁 底层原理

[java 中的锁 -- 偏向锁、轻量级锁、自旋锁、重量级锁\_java owner指针-CSDN博客](https://blog.csdn.net/yangstarss/article/details/79868045?ops_request_misc=&request_id=&biz_id=102&utm_term=%E5%81%8F%E5%90%91%E9%94%81&utm_medium=distribute.pc_search_result.none-task-blog-2~blog~sobaiduweb~default-2-79868045.nonecase&spm=1018.2226.3001.4450 "java 中的锁 -- 偏向锁、轻量级锁、自旋锁、重量级锁_java owner指针-CSDN博客")

[深入解析Synchronized锁底层原理\_synchronized底层-CSDN博客](https://blog.csdn.net/weixin_44817884/article/details/136680258?ops_request_misc=%257B%2522request%255Fid%2522%253A%2522C4E7B618-AF55-4387-B6D8-5469B2E44D65%2522%252C%2522scm%2522%253A%252220140713.130102334..%2522%257D&request_id=C4E7B618-AF55-4387-B6D8-5469B2E44D65&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~blog~sobaiduend~default-1-136680258-null-null.nonecase&utm_term=Synchronized%E9%94%81%E5%BA%95%E5%B1%82%E5%8E%9F%E7%90%86&spm=1018.2226.3001.4450 "深入解析Synchronized锁底层原理_synchronized底层-CSDN博客")

## 7.小结

+   Java 中的每一个对象都可以作为一个锁，Java 中的锁都是基于对象的。
+   synchronized 关键字可以用来修饰方法和代码块，它可以保证在同一时刻最多只有一个线程执行该段代码。
+   synchronized 关键字在修饰方法时，锁为当前实例对象；在修饰静态方法时，锁为当前 Class 对象；在修饰代码块时，锁为括号里面的对象。
+   Java 6 为了减少获得锁和释放锁带来的性能消耗，引入了“偏向锁”和“轻量级锁“。在 Java 6 以前，所有的锁都是”重量级“锁。所以在 Java 6 及其以后，一个对象其实有四种锁状态，它们级别由低到高依次是：无锁状态、偏向锁状态、轻量级锁状态、重量级锁状态。
+   偏向锁会偏向于第一个访问锁的线程，如果在接下来的运行过程中，该锁没有被其他的线程访问，则持有偏向锁的线程将永远不需要触发同步。也就是说，偏向锁在资源无竞争情况下消除了同步语句，连 CAS 操作都不做了，提高了程序的运行性能。
+   轻量级锁是通过 CAS 操作和自旋来实现的，如果自旋失败，则会升级为重量级锁。
+   重量级锁依赖于操作系统的互斥量（mutex） 实现的，而操作系统中线程间状态的转换需要相对较长的时间，所以重量级锁效率很低，但被阻塞的线程不会消耗 CPU。