---
created: 2025-04-11T10:00:29 (UTC +08:00)
tags: [synchronized]
source: https://blog.csdn.net/m0_53474063/article/details/112389756
author: 成就一亿技术人!
---

# synchronized详解-CSDN博客

> ## Excerpt
> 文章浏览阅读8.7w次，点赞108次，收藏542次。synchronized详解_synchronized

---
## 一、synchronized简单介绍

synchronized中文意思是同步，也称之为”同步锁“。

synchronized的作用是保证在同一时刻， 被修饰的代码块或方法只会有一个线程执行，以达到保证并发安全的效果。

synchronized是Java中解决并发问题的一种最常用的方法，也是最简单的一种方法。

在JDK1.5之前synchronized是一个重量级锁，相对于j.u.c.Lock，它会显得那么笨重，随着Javs SE 1.6对synchronized进行的各种优化后，synchronized并不会显得那么重了。

synchronized的作用主要有三个：

-   原子性：确保线程互斥地访问同步代码；
-   可见性：保证共享变量的修改能够及时可见，其实是通过Java内存模型中的“对一个变量unlock操作之前，必须要同步到主内存中；如果对一个变量进行lock操作，则将会清空工作内存中此变量的值，在执行引擎使用此变量前，需要重新从主内存中load操作或assign操作初始化变量值” 来保证的；
-   有序性：有效解决重排序问题，即 “一个unlock操作先行发生(happen-before)于后面对同一个锁的lock操作”；

## 二、synchronized的使用

synchronized的3种使用方式：

-   修饰实例方法：作用于当前实例加锁
-   修饰静态方法：作用于当前类对象加锁
-   修饰代码块：指定加锁对象，对给定对象加锁

synchronized的代码范例：

![](https://i-blog.csdnimg.cn/blog_migrate/81a39c171722fce52e45a2121b29198d.jpeg)

### 修饰方法

Synchronized修饰一个方法很简单，就是在方法的前面加synchronized，synchronized修饰方法和修饰一个代码块类似，只是作用范围不一样，修饰代码块是大括号括起来的范围，而修饰方法范围是整个函数。

方法一：

```java
public synchronized void method() {}

```

方法二：

```java
public void method() {
    synchronized (this) {}
}

```

写法一修饰的是一个方法，写法二修饰的是一个代码块，但写法一与写法二是等价的，都是锁定了整个方法时的内容。

synchronized关键字不能继承。 虽然可以使用synchronized来定义方法，但synchronized并不属于方法定义的一部分，因此，synchronized关键字不能被继承。如果在父类中的某个方法使用了synchronized关键字，而在子类中覆盖了这个方法，在子类中的这个方法默认情况下并不是同步的，而必须显式地在子类的这个方法中加上synchronized关键字才可以。当然，还可以在子类方法中调用父类中相应的方法，这样虽然子类中的方法不是同步的，但子类调用了父类的同步方法，因此，子类的方法也就相当于同步了。这两种方式的例子代码如下： 

在子类方法中加上synchronized关键字

```java
class Parent {

    public synchronized void method() {}
}

class Child extends Parent {

    public synchronized void method() {}
}

```

在子类方法中调用父类的同步方法

```java
class Parent {

    public synchronized void method() {}
}

class Child extends Parent {

    public void method() {
        super.method();
    }
}

```

注意：

-   在定义接口方法时不能使用synchronized关键字。
-   构造方法不能使用synchronized关键字，但可以使用synchronized代码块来进行同步。 

### 修饰一个代码块

(1) 一个线程访问一个对象中的synchronized(this)同步代码块时，其他试图访问该对象的线程将被阻塞。

注意下面两个程序的区别：

```java
class SyncThread implements Runnable {

    private static int count;

    public SyncThread() {
        count = 0;
    }

    public void run() {
        synchronized (this) {
            for (int i = 0; i < 5; i++) {
                try {
                    System.out.println(
                        Thread.currentThread().getName() + ":" + (count++)
                    );
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public int getCount() {
        return count;
    }
}

public class Demo00 {

    public static void main(String args[]) {
        SyncThread s = new SyncThread();
        Thread t1 = new Thread(s);
        Thread t2 = new Thread(s);
        t1.start();
        t2.start();
    }
}

```

调用方式二种，当两个并发线程(thread1和thread2)访问同一个对象(syncThread)中的synchronized代码块时，在同一时刻只能有一个线程得到执行，另一个线程受阻塞，必须等待当前线程执行完这个代码块以后才能执行该代码块。Thread1和thread2是互斥的，因为在执行synchronized代码块时会锁定当前的对象，只有执行完该代码块才能释放该对象锁，下一个线程才能执行并锁定该对象

调用方式一中，thread1和thread2同时在执行。这是因为synchronized只锁定对象，每个对象只有一个锁（lock）与之相关联。

(2) 

```java
class Counter implements Runnable {

    private int count;

    public Counter() {
        count = 0;
    }

    public void countAdd() {
        synchronized (this) {
            for (int i = 0; i < 5; i++) {
                try {
                    System.out.println(
                        Thread.currentThread().getName() + ":" + (count++)
                    );
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public void printCount() {
        for (int i = 0; i < 5; i++) {
            try {
                System.out.println(
                    Thread.currentThread().getName() + " count:" + count
                );
                Thread.sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    public void run() {
        String threadName = Thread.currentThread().getName();
        if (threadName.equals("A")) {
            countAdd();
        } else if (threadName.equals("B")) {
            printCount();
        }
    }
}

public class Demo00 {

    public static void main(String args[]) {
        Counter counter = new Counter();
        Thread thread1 = new Thread(counter, "A");
        Thread thread2 = new Thread(counter, "B");
        thread1.start();
        thread2.start();
    }
}

```

可以看见B线程的调用是非synchronized,并不影响A线程对synchronized部分的调用。从上面的结果中可以看出一个线程访问一个对象的synchronized代码块时，别的线程可以访问该对象的非synchronized代码块而不受阻塞。

(3) 指定要给某个对象加锁

```java 
class Account {

    String name;
    float amount;

    public Account(String name, float amount) {
        this.name = name;
        this.amount = amount;
    }

    public void deposit(float amt) {
        amount += amt;
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public void withdraw(float amt) {
        amount -= amt;
        try {
            Thread.sleep(100);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public float getBalance() {
        return amount;
    }
}

class AccountOperator implements Runnable {

    private Account account;

    public AccountOperator(Account account) {
        this.account = account;
    }

    public void run() {
        synchronized (account) {
            account.deposit(500);
            account.withdraw(500);
            System.out.println(
                Thread.currentThread().getName() + ":" + account.getBalance()
            );
        }
    }
}

public class Demo00 {

    public static void main(String args[]) {
        Account account = new Account("zhang san", 10000.0f);
        AccountOperator accountOperator = new AccountOperator(account);
        final int THREAD_NUM = 5;
        Thread threads[] = new Thread[THREAD_NUM];
        for (int i = 0; i < THREAD_NUM; i++) {
            threads[i] = new Thread(accountOperator, "Thread" + i);
            threads[i].start();
        }
    }
}

```

在AccountOperator 类中的run方法里，我们用synchronized 给account对象加了锁。这时，当一个线程访问account对象时，其他试图访问account对象的线程将会阻塞，直到该线程访问account对象结束。也就是说谁拿到那个锁谁就可以运行它所控制的那段代码。   
当有一个明确的对象作为锁时，就可以用类似下面这样的方式写程序：

```
public void method3(SomeObject obj){synchronized(obj)   {   }}
```

当没有明确的对象作为锁，只是想让一段代码同步时，可以创建一个特殊的对象来充当锁：

```
class Test implements Runnable{private byte[] lock = new byte[0];  public void method()   {synchronized(lock) {      }   }public void run() {   }}
```

### 修改一个静态方法

synchronized也可修饰一个静态方法，用法如下：

```
public synchronized static void method() {}
```

静态方法是属于类的而不属于对象的。同样的，synchronized修饰的静态方法锁定的是这个类的所有对象。

```java 
class SyncThread implements Runnable {

    private static int count;

    public SyncThread() {
        count = 0;
    }

    public static synchronized void method() {
        for (int i = 0; i < 5; i++) {
            try {
                System.out.println(
                    Thread.currentThread().getName() + ":" + (count++)
                );
                Thread.sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    public synchronized void run() {
        method();
    }
}

public class Demo00 {

    public static void main(String args[]) {
        SyncThread syncThread1 = new SyncThread();
        SyncThread syncThread2 = new SyncThread();
        Thread thread1 = new Thread(syncThread1, "SyncThread1");
        Thread thread2 = new Thread(syncThread2, "SyncThread2");
        thread1.start();
        thread2.start();
    }
}

```

syncThread1和syncThread2是SyncThread的两个对象，但在thread1和thread2并发执行时却保持了线程同步。这是因为run中调用了静态方法method，而静态方法是属于类的，所以syncThread1和syncThread2相当于用了同一把锁。

### 修饰一个类

Synchronized还可作用于一个类，用法如下：

```java
class ClassName {

    public void method() {
        synchronized (ClassName.class) {}
    }
}

```

```java 
class SyncThread implements Runnable {

    private static int count;

    public SyncThread() {
        count = 0;
    }

    public static void method() {
        synchronized (SyncThread.class) {
            for (int i = 0; i < 5; i++) {
                try {
                    System.out.println(
                        Thread.currentThread().getName() + ":" + (count++)
                    );
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public synchronized void run() {
        method();
    }
}

```

本例的的给class加锁和上例的给静态方法加锁是一样的，所有对象公用一把锁。

### 使用总结

-   无论synchronized关键字加在方法上还是对象上，如果它作用的对象是非静态的，则它取得的锁是对象；如果synchronized作用的对象是一个静态方法或一个类，则它取得的锁是对类，该类所有的对象同一把锁。 
-   每个对象只有一个锁（lock）与之相关联，谁拿到这个锁谁就可以运行它所控制的那段代码。 
-   实现同步是要很大的系统开销作为代价的，甚至可能造成死锁，所以尽量避免无谓的同步控制。

## 三、synchronized的底层实现

谈synchronized的底层实现，就不得不谈数据在JVM内存的存储：Java对象头，以及Monitor对象监视器。

### 对象头

在JVM中，对象在内存中的布局分为三块区域：对象头、实例数据和对齐填充。如下图所示：

![](https://i-blog.csdnimg.cn/blog_migrate/b60f0354eccd05158c1954c978bafc4e.png)

-   实例数据：存放类的属性数据信息，包括父类的属性信息；
-   对齐填充：由于虚拟机要求 对象起始地址必须是8字节的整数倍。填充数据不是必须存在的，仅仅是为了字节对齐；
-   对象头：Java对象头一般占有2个机器码（在32位虚拟机中，1个机器码等于4字节，也就是32bit，在64位虚拟机中，1个机器码是8个字节，也就是64bit），但是如果对象是数组类型，则需要3个机器码，因为JVM虚拟机可以通过Java对象的元数据信息确定Java对象的大小，但是无法从数组的元数据来确认数组的大小，所以用一块来记录数组长度。

synchronized用的锁就是存在Java对象头里的，那么什么是Java对象头呢？Hotspot虚拟机的对象头主要包括两部分数据：Mark Word（标记字段）、Class Pointer（类型指针）。其中 Class Pointer是对象指向它的类元数据的指针，虚拟机通过这个指针来确定这个对象是哪个类的实例，Mark Word用于存储对象自身的运行时数据，它是实现轻量级锁和偏向锁的关键。 Java对象头具体结构描述如下：  
![](https://i-blog.csdnimg.cn/blog_migrate/47840285f035dadc07ec6b7b30734bd3.png)

Mark Word用于存储对象自身的运行时数据，如：哈希码（HashCode）、GC分代年龄、锁状态标志、线程持有的锁、偏向线程 ID、偏向时间戳等。下图是Java对象头 无锁状态下Mark Word部分的存储结构（32位虚拟机）：

![](https://i-blog.csdnimg.cn/blog_migrate/c803e1947567215a206b93198ac84442.png)

对象头信息是与对象自身定义的数据无关的额外存储成本，但是考虑到虚拟机的空间效率，Mark Word被设计成一个非固定的数据结构以便在极小的空间内存存储尽量多的数据，它会根据对象的状态复用自己的存储空间，也就是说，Mark Word会随着程序的运行发生变化，可能变化为存储以下4种数据：

![](https://i-blog.csdnimg.cn/blog_migrate/860694a11d0d9d05f31ed1e8ef18b45c.png)

在64位虚拟机下，Mark Word是64bit大小的，其存储结构如下：

![](https://i-blog.csdnimg.cn/blog_migrate/9aa4a014528d8d976fb3eb4110d8999d.png)

对象头的最后两位存储了锁的标志位，**01是初始状态，未加锁**，其对象头里存储的是对象本身的哈希码，随着锁级别的不同，对象头里会存储不同的内容。**偏向锁存储的是当前占用此对象的线程ID**；**而轻量级则存储指向线程栈中锁记录的指针**。从这里我们可以看到，“锁”这个东西，**可能是个锁记录+对象头里的引用指针**（判断线程是否拥有锁时将线程的锁记录地址和对象头里的指针地址比较)，**也可能是对象头里的线程ID**（判断线程是否拥有锁时将线程的ID和对象头里存储的线程ID比较）。

![](https://i-blog.csdnimg.cn/blog_migrate/6ec18408db9e56aa432f7206b3399c6f.png)

### 对象头中Mark Word与线程中Lock Record

在线程进入同步代码块的时候，如果此同步对象没有被锁定，即它的锁标志位是01，则虚拟机首先在当前线程的栈中创建我们称之为“锁记录（Lock Record）”的空间，用于存储锁对象的Mark Word的拷贝，官方把这个拷贝称为Displaced Mark Word。整个Mark Word及其拷贝至关重要。

Lock Record是线程私有的数据结构，每一个线程都有一个可用Lock Record列表，同时还有一个全局的可用列表。每一个被锁住的对象Mark Word都会和一个Lock Record关联（对象头的MarkWord中的Lock Word指向Lock Record的起始地址），同时Lock Record中有一个Owner字段存放拥有该锁的线程的唯一标识（或者object mark word），表示该锁被这个线程占用。如下图所示为Lock Record的内部结构：

<table><tbody><tr><td>Lock Record</td><td>描述</td></tr><tr><td>Owner</td><td>初始时为NULL表示当前没有任何线程拥有该monitor record，当线程成功拥有该锁后保存线程唯一标识，当锁被释放时又设置为NULL；</td></tr><tr><td>EntryQ</td><td>关联一个系统互斥锁（semaphore），阻塞所有试图锁住monitor record失败的线程；</td></tr><tr><td>RcThis</td><td>表示blocked或waiting在该monitor record上的所有线程的个数；</td></tr><tr><td>Nest</td><td>用来实现 重入锁的计数；</td></tr><tr><td>HashCode</td><td>保存从对象头拷贝过来的HashCode值（可能还包含GC age）。</td></tr><tr><td>Candidate</td><td>用来避免不必要的阻塞或等待线程唤醒，因为每一次只有一个线程能够成功拥有锁，如果每次前一个释放锁的线程唤醒所有正在阻塞或等待的线程，会引起不必要的上下文切换（从阻塞到就绪然后因为竞争锁失败又被阻塞）从而导致性能严重下降。Candidate只有两种可能的值0表示没有需要唤醒的线程1表示要唤醒一个继任线程来竞争锁。</td></tr></tbody></table>

### 监视器（Monitor）

任何一个对象都有一个Monitor与之关联，当且一个Monitor被持有后，它将处于锁定状态。

synchronized在JVM里的实现都是 基于进入和退出Monitor对象来实现方法同步和代码块同步，虽然具体实现细节不一样，但是都可以通过成对的MonitorEnter和MonitorExit指令来实现。

> MonitorEnter指令：插入在同步代码块的开始位置，当代码执行到该指令时，将会尝试获取该对象Monitor的所有权，即尝试获得该对象的锁；
> 
> MonitorExit指令：插入在方法结束处和异常处，JVM保证每个MonitorEnter必须有对应的MonitorExit；

那什么是Monitor？可以把它理解为 一个同步工具，也可以描述为 一种同步机制，它通常被 描述为一个对象。

与一切皆对象一样，所有的Java对象是天生的Monitor，每一个Java对象都有成为Monitor的潜质，因为在Java的设计中 ，每一个Java对象自打娘胎里出来就带了一把看不见的锁，它叫做内部锁或者Monitor锁。

也就是通常说Synchronized的对象锁，MarkWord锁标识位为10，其中指针指向的是Monitor对象的起始地址。在Java虚拟机（HotSpot）中，Monitor是由ObjectMonitor实现的，其主要数据结构如下（位于HotSpot虚拟机源码ObjectMonitor.hpp文件，C++实现的）：

```c++
ObjectMonitor() {
    _header = NULL;
    _count = 0;
    _waiters = 0, _recursions = 0;
    _object = NULL;
    _owner = NULL;
    _WaitSet = NULL;
    _WaitSetLock = 0;
    _Responsible = NULL;
    _succ = NULL;
    _cxq = NULL;
    FreeNext = NULL;
    _EntryList = NULL;
    _SpinFreq = 0;
    _SpinClock = 0;
    OwnerIsThread = 0;
}
```

ObjectMonitor中有两个队列，\_WaitSet 和 \_EntryList，用来保存ObjectWaiter对象列表（ 每个等待锁的线程都会被封装成ObjectWaiter对象 ），\_owner指向持有ObjectMonitor对象的线程，当多个线程同时访问一段同步代码时：

-   首先会进入 \_EntryList 集合，当线程获取到对象的monitor后，进入 \_Owner区域并把monitor中的owner变量设置为当前线程，同时monitor中的计数器count加1；
-   若线程调用 wait() 方法，将释放当前持有的monitor，owner变量恢复为null，count自减1，同时该线程进入 WaitSet集合中等待被唤醒；
-   若当前线程执行完毕，也将释放monitor（锁）并复位count的值，以便其他线程进入获取monitor(锁)；

同时，Monitor对象存在于每个Java对象的对象头Mark Word中（存储的指针的指向），Synchronized锁便是通过这种方式获取锁的，也是为什么Java中任意对象可以作为锁的原因，同时notify/notifyAll/wait等方法会使用到Monitor锁对象，所以必须在同步代码块中使用。

监视器Monitor有两种同步方式：互斥与协作。多线程环境下线程之间如果需要共享数据，需要解决互斥访问数据的问题，监视器可以确保监视器上的数据在同一时刻只会有一个线程在访问。

什么时候需要协作？ 比如：

> 一个线程向缓冲区写数据，另一个线程从缓冲区读数据，如果读线程发现缓冲区为空就会等待，当写线程向缓冲区写入数据，就会唤醒读线程，**这里读线程和写线程就是一个合作关系**。JVM通过Object类的wait方法来使自己等待，在调用wait方法后，该线程会释放它持有的监视器，直到其他线程通知它才有执行的机会。一个线程调用notify方法通知在等待的线程，这个等待的线程并不会马上执行，而是要通知线程释放监视器后，它重新获取监视器才有执行的机会。如果刚好唤醒的这个线程需要的监视器被其他线程抢占，那么这个线程会继续等待。Object类中的notifyAll方法可以解决这个问题，它可以唤醒所有等待的线程，总有一个线程执行。

![](https://i-blog.csdnimg.cn/blog_migrate/0725d7a567f2696c2d64d88b1bcf6f2d.png)

如上图所示，一个线程通过1号门进入Entry Set(入口区)，如果在入口区没有线程等待，那么这个线程就会获取监视器成为监视器的Owner，然后执行监视区域的代码。如果在入口区中有其它线程在等待，那么新来的线程也会和这些线程一起等待。线程在持有监视器的过程中，有两个选择，一个是正常执行监视器区域的代码，释放监视器，通过5号门退出监视器；还有可能等待某个条件的出现，于是它会通过3号门到Wait Set（等待区）休息，直到相应的条件满足后再通过4号门进入重新获取监视器再执行。

注意：当一个线程释放监视器时，在入口区和等待区的等待线程都会去竞争监视器，如果入口区的线程赢了，会从2号门进入；如果等待区的线程赢了会从4号门进入。只有通过3号门才能进入等待区，在等待区中的线程只有通过4号门才能退出等待区，也就是说一个线程只有在持有监视器时才能执行wait操作，处于等待的线程只有再次获得监视器才能退出等待状态。

## 四、synchronized 锁的升级顺序

锁解决了数据的安全性，但是同样带来了性能的下降。hotspot 虚拟机的作者经过调查发现，大部分情况下，加锁的代码不仅仅不存在多线程竞争，而且总是由同一个线程多次获得。所以基于这样一个概率。  
synchronized 在JDK1.6 之后做了一些优化，为了减少获得锁和释放锁来的性能开销，引入了偏向锁、轻量级锁、自旋锁、重量级锁，锁的状态根据竞争激烈的程度从低到高不断升级。

锁主要存在四种状态，依次是：**无锁状态、偏向锁状态、轻量级锁状态、重量级锁状态**，锁可以从偏向锁升级到轻量级锁，再升级的重量级锁。但是锁的升级是单向的，也就是说**只能从低到高升级，不会出现锁的降级**。而且这个过程就是开销逐渐加大的过程。

![](https://i-blog.csdnimg.cn/blog_migrate/18abd42c03f2aabbdc0f32fc44f54dee.png)

## 五、参考

[https://www.jianshu.com/p/e62fa839aa41](https://www.jianshu.com/p/e62fa839aa41)

[https://zhuanlan.zhihu.com/p/262211521](https://zhuanlan.zhihu.com/p/262211521)
