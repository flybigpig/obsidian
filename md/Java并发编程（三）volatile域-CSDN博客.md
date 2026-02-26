相关文章  
[Java并发编程（一）线程定义、状态和属性](http://blog.csdn.net/itachi85/article/details/48878057)  
[Java并发编程（二）同步](http://blog.csdn.net/itachi85/article/details/50130621)  
[Android多线程(一)线程池](http://blog.csdn.net/itachi85/article/details/44874511)  
[Android多线程(二)AsyncTask源码分析](http://blog.csdn.net/itachi85/article/details/45041923)

#### **前言**

有时仅仅为了读写一个或者两个实例域就使用同步的话，显得开销过大，volatile关键字为实例域的同步访问提供了免锁的机制。如果声明一个域为volatile,那么编译器和虚拟机就知道该域是可能被另一个线程并发更新的。再讲到volatile关键字之前我们需要了解一下内存模型的相关概念以及[并发编程](https://so.csdn.net/so/search?q=%E5%B9%B6%E5%8F%91%E7%BC%96%E7%A8%8B&spm=1001.2101.3001.7020)中的三个特性：原子性，可见性和有序性。

#### **1\. java内存模型与原子性，可见性和有序性**

Java内存模型规定所有的变量都是存在主存当中，每个线程都有自己的工作内存。线程对变量的所有操作都必须在工作内存中进行，而不能直接对主存进行操作。并且每个线程不能访问其他线程的工作内存。  
在java中，执行下面这个语句：

```cpp
int i=3;
```

执行线程必须先在自己的工作线程中对变量i所在的缓存行进行赋值操作，然后再写入主存当中。而不是直接将数值3写入主存当中。  
那么Java语言 本身对 原子性、可见性以及有序性提供了哪些保证呢？

##### **原子性**

对基本数据类型的变量的读取和赋值操作是原子性操作，即这些操作是不可被中断的，要么执行，要么不执行。  
来看一下下面的代码：

```cpp
x = 10;        //语句1
y = x;         //语句2
x++;           //语句3
x = x + 1;     //语句4
```

只有语句1是原子性操作，其他三个语句都不是原子性操作。  
语句2实际上包含2个操作，它先要去读取x的值，再将x的值写入工作内存，虽然读取x的值以及 将x的值写入工作内存 这2个操作都是原子性操作，但是合起来就不是原子性操作了。  
同样的，x++和 x = x+1包括3个操作：读取x的值，进行加1操作，写入新的值。  
也就是说，只有简单的读取、赋值（而且必须是将数字赋值给某个变量，变量之间的相互赋值不是原子操作）才是原子操作。  
java.util.concurrent.atomic包中有很多类使用了很高效的机器级指令（而不是使用锁）来保证其他操作的原子性。例如AtomicInteger类提供了方法incrementAndGet和decrementAndGet，它们分别以原子方式将一个整数自增和自减。可以安全地使用AtomicInteger类作为共享计数器而无需同步。  
另外这个包还包含AtomicBoolean，AtomicLong和AtomicReference这些原子类仅供开发并发工具的系统程序员使用，应用程序员不应该使用这些类。

##### **可见性**

可见性，是指线程之间的可见性，一个线程修改的状态对另一个线程是可见的。也就是一个线程修改的结果。另一个线程马上就能看到。  
当一个共享变量被volatile修饰时，它会保证修改的值会立即被更新到主存，所以对其他线程是可见的，当有其他线程需要读取时，它会去内存中读取新值。  
而普通的共享变量不能保证可见性，因为普通共享变量被修改之后，什么时候被写入主存是不确定的，当其他线程去读取时，此时内存中可能还是原来的旧值，因此无法保证可见性。

##### **有序性**

在Java内存模型中，允许编译器和处理器对指令进行重排序，但是重排序过程不会影响到单线程程序的执行，却会影响到多线程并发执行的正确性。  
可以通过volatile关键字来保证一定的“有序性”。另外可以通过synchronized和Lock来保证有序性，很显然，synchronized和Lock保证每个时刻是有一个线程执行同步代码，相当于是让线程顺序执行同步代码，自然就保证了有序性。

#### **2\. volatile关键字**

一旦一个共享变量（类的成员变量、类的静态成员变量）被volatile修饰之后，那么就具备了两层语义：

-   保证了不同线程对这个变量进行操作时的可见性，即一个线程修改了某个变量的值，这新值对其他线程来说是立即可见的。
-   禁止进行指令重排序。

先看一段代码，假如线程1先执行，线程2后执行：  
　　

```java
//线程1
boolean stop = false;
while(!stop){
    doSomething();
}

//线程2
stop = true;
```

很多人在中断线程时可能都会采用这种标记办法。但是事实上，这段代码会完全运行正确么？即一定会将线程中断么？不一定，也许在大多数时候，这个代码能够把线程中断，但是也有可能会导致无法中断线程（虽然这个可能性很小，但是只要一旦发生这种情况就会造成死循环了）。  
为何有可能导致无法中断线程？每个线程在运行过程中都有自己的工作内存，那么线程1在运行的时候，会将stop变量的值拷贝一份放在自己的工作内存当中。那么当线程2更改了stop变量的值之后，但是还没来得及写入主存当中，线程2转去做其他事情了，那么线程1由于不知道线程2对stop变量的更改，因此还会一直循环下去。  
但是用volatile修饰之后就变得不一样了：

-   使用volatile关键字会强制将修改的值立即写入主存；
-   使用volatile关键字的话，当线程2进行修改时，会导致线程1的工作内存中缓存变量stop的缓存行无效；
-   由于线程1的工作内存中缓存变量stop的缓存行无效，所以线程1再次读取变量stop的值时会去主存读取。

##### **volatile保证原子性吗？**

我们知道volatile关键字保证了操作的可见性，但是volatile能保证对变量的操作是原子性吗？

```csharp
public class Test {
    public volatile int inc = 0;  
    public void increase() {
        inc++;
    }

    public static void main(String[] args) {
        final Test test = new Test();
        for(int i=0;i<10;i++){
            new Thread(){
                public void run() {
                    for(int j=0;j<1000;j++)
                        test.increase();
                };
            }.start();
        }
         //保证前面的线程都执行完
        while(Thread.activeCount()>1)  
            Thread.yield();
        System.out.println(test.inc);
    }
}
```

这段代码每次运行结果都不一致，都是一个小于10000的数字，在前面已经提到过，自增操作是不具备原子性的，它包括读取变量的原始值、进行加1操作、写入工作内存。那么就是说自增操作的三个子操作可能会分割开执行。  
假如某个时刻变量inc的值为10，线程1对变量进行自增操作，线程1先读取了变量inc的原始值，然后线程1被阻塞了；然后线程2对变量进行自增操作，线程2也去读取变量inc的原始值，由于线程1只是对变量inc进行读取操作，而没有对变量进行修改操作，所以不会导致线程2的工作内存中缓存变量inc的缓存行无效，所以线程2会直接去主存读取inc的值，发现inc的值时10，然后进行加1操作，并把11写入工作内存，最后写入主存。然后线程1接着进行加1操作，由于已经读取了inc的值，注意此时在线程1的工作内存中inc的值仍然为10，所以线程1对inc进行加1操作后inc的值为11，然后将11写入工作内存，最后写入主存。那么两个线程分别进行了一次自增操作后，inc只增加了1。  
自增操作不是原子性操作，而且volatile也无法保证对变量的任何操作都是原子性的。

##### **volatile能保证有序性吗？**

在前面提到volatile关键字能禁止指令重排序，所以volatile能在一定程度上保证有序性。  
volatile关键字禁止指令重排序有两层意思：

-   当程序执行到volatile变量的读操作或者写操作时，在其前面的操作的更改肯定全部已经进行，且结果已经对后面的操作可见；在其后面的操作肯定还没有进行；
-   在进行指令优化时，不能将在对volatile变量访问的语句放在其后面执行，也不能把volatile变量后面的语句放到其前面执行。

#### **3\. 正确使用volatile关键字**

synchronized关键字是防止多个线程同时执行一段代码，那么就会很影响程序执行效率，而volatile关键字在某些情况下性能要优于synchronized，但是要注意volatile关键字是无法替代synchronized关键字的，因为volatile关键字无法保证操作的原子性。通常来说，使用volatile必须具备以下2个条件：

-   对变量的写操作不依赖于当前值
-   该变量没有包含在具有其他变量的不变式中

第一个条件就是不能是自增自减等操作，上文已经提到volatile不保证原子性。  
第二个条件我们来举个例子它包含了一个不变式 ：下界总是小于或等于上界

```csharp
public class NumberRange {
    private volatile int lower, upper;
    public int getLower() { return lower; }
    public int getUpper() { return upper; }
    public void setLower(int value) { 
        if (value > upper) 
            throw new IllegalArgumentException(...);
        lower = value;
    }
    public void setUpper(int value) { 
        if (value < lower) 
            throw new IllegalArgumentException(...);
        upper = value;
    }
}
```

这种方式限制了范围的状态变量，因此将 lower 和 upper 字段定义为 volatile 类型不能够充分实现类的线程安全，从而仍然需要使用同步。否则，如果凑巧两个线程在同一时间使用不一致的值执行 setLower 和 setUpper 的话，则会使范围处于不一致的状态。例如，如果初始状态是 (0, 5)，同一时间内，线程 A 调用 setLower(4) 并且线程 B 调用 setUpper(3)，显然这两个操作交叉存入的值是不符合条件的，那么两个线程都会通过用于保护不变式的检查，使得最后的范围值是 (4, 3)，这显然是不对的。  
其实就是要保证操作的原子性就可以使用volatile，使用volatile主要有两个场景:

##### **状态标志**

```csharp
volatile boolean shutdownRequested;
...
public void shutdown()
 { 
 shutdownRequested = true;
  }
public void doWork() { 
    while (!shutdownRequested) { 
        // do stuff
    }
}
```

很可能会从循环外部调用 shutdown() 方法 —— 即在另一个线程中 —— 因此，需要执行某种同步来确保正确实现 shutdownRequested 变量的可见性。然而，使用 synchronized 块编写循环要比使用volatile 状态标志编写麻烦很多。由于 volatile 简化了编码，并且状态标志并不依赖于程序内任何其他状态，因此此处非常适合使用 volatile。

##### **双重检查模式 （DCL）**

```java
public class Singleton {  
    private volatile static Singleton instance = null;  
    public static Singleton getInstance() {  
        if (instance == null) {  
            synchronized(this) {  
                if (instance == null) {  
                    instance = new Singleton();  
                }  
            }  
        }  
        return instance;  
    }  
}
```

在这里使用volatile会或多或少的影响性能，但考虑到程序的正确性，牺牲这点性能还是值得的。  
DCL优点是资源利用率高，第一次执行getInstance时单例对象才被实例化，效率高。缺点是第一次加载时反应稍慢一些，在高并发环境下也有一定的缺陷，虽然发生的概率很小。  
DCL虽然在一定程度解决了资源的消耗和多余的同步，线程安全等问题，但是他还是在某些情况会出现失效的问题，也就是DCL失效，在《java并发编程实践》一书建议用以下的代码(静态内部类单例模式）来替代DCL：

```csharp
public class Singleton { 
    private Singleton(){
    }
      public static Singleton getInstance(){  
        return SingletonHolder.sInstance;  
    }  
    private static class SingletonHolder {  
        private static final Singleton sInstance = new Singleton();  
    }  
}
```

关于双重检查可以查看[http://blog.csdn.net/dl88250/article/details/5439024](http://blog.csdn.net/dl88250/article/details/5439024)

#### **4\. 总结**

与锁相比，Volatile 变量是一种非常简单但同时又非常脆弱的同步机制，它在某些情况下将提供优于锁的性能和伸缩性。如果严格遵循 volatile 的使用条件即变量真正独立于其他变量和自己以前的值 ，在某些情况下可以使用 volatile 代替 synchronized 来简化代码。然而，使用 volatile 的代码往往比使用锁的代码更加容易出错。本文介绍了可以使用 volatile 代替 synchronized 的最常见的两种用例，其他的情况我们最好还是去使用synchronized 。