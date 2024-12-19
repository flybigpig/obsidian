本片文章的主要内容如下：

-   1、Java中的ThreadLocal
-   2、 Android中的ThreadLocal
-   3、Android 面试中的关于ThreadLocal的问题
-   4、ThreadLocal的总结
-   5、思考

###### 这里先说下Java中的ThreadLocal和Android中的ThreadLocal的源代码是不一样的，为了让大家更好的理解Android中的ThreadLocal，我们先从Java中的ThreadLocal开始。

> 注：基于Android 6.0(6.0.1\_r10)/API 23 源码

### 一、 Java中的ThreadLocal：

##### (一) ThreadLocal的前世今生

> 早在JDK1.2的版本中就提供java.lang.ThreadLocal，ThreadLocal为解决多线程程序的并发问题提供了一种新的思路，并在JDK1.5开始支持泛型。这个工具类可以很简洁地编写出优美的多线程程序。当使用ThreadLocal维护变量时，ThreadLocal为每个使用该变量的线程提供独立的变量副本，所以每一个线程都可以独立的改变自己的副本，而不会影响其他线程所对应的副本。从线程的角度来看，目标变量就像是本地变量，这也是类名中"Local"所要表达的意思。所以，在Java中编写线程局部变量的代码相对来说要"笨拙"一些，因此造成了线程局部变量没有在Java开发者得到很好的普及。

##### (二) ThreadLocal类简介

###### 1、Java源码描述

看Java源码中的描述：

> This class provides thread-local variables. These variables differ from their normal counterparts in that each thread that accesses one (via its get or set method) has its own, independently initialized copy of the variable. ThreadLocal instances are typically private static fields in classes that wish to associate state with a thread (e.g., a user ID or Transaction ID).

翻译一下：

> ThreadLocal类用来提供线程内部的局部变量，这种变量在多线程环境下访问(通过get或set方法访问)时能保证各个线程里的变量相对独立于其他线程内的变量。ThreadLocal实例通常来说都是private static 类型，用于关联线程。

如果大家还不明白，我们可以类比一下就是：

> 比如，今天是七夕，我们的计划是，先吃饭、看电影、去开房(你们懂的)，这里吃饭、看电影、开房好比同一个线程的三个函数，我就是一个线程，我完成这三个函数都需要一个东西"支付宝"(用来付钱)。但是支付宝(类比为ThreadLocal)这个东西其实是**支付宝公司**的，我只要吃完饭，看电影买票，开房付房费的时候才会使用，平时都是放在手机里面不动的吧。这样我就可以在何时何地用支付宝付款了。 当然你们会问，为什么不设置为全局变量，这样不也是可以实现何时何地都能去公交卡吗？但是如果有很多人(很多线程)呢？总不能大家都用我的支付宝吧，那样我不就成为雷锋了。这就是ThreadLocal设计的初衷：提供线程内部的局部变量，在本地线程内随时随地可取，隔离其他线程。

总结一下：

> ThreadLocal的作用是提供线程内的局部变量，这种局部变量仅仅在线程的生命周期中起作用，减少同一个线程内多个函数或者组件一些公共变量的传递的复杂度。

###### 2、ThreadLocal类结构

ThreadLocal的类图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/pgf8q4xmmu.png)

ThreadLocal的类图.png

ThreadLocal的结构图：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/m2r9nsszwk.png)

ThreadLocal类结构.png

###### 3、ThreadLocal常用的方法

###### (1)、set方法

设置当前线程的线程局部变量的值 源码如下：

```
   /**
     * Sets the current thread's copy of this thread-local variable
     * to the specified value.  Most subclasses will have no need to
     * override this method, relying solely on the {@link #initialValue}
     * method to set the values of thread-locals.
     *
     * @param value the value to be stored in the current thread's copy of
     *        this thread-local.
     */
    public void set(T value) {
        Thread t = Thread.currentThread();
        ThreadLocalMap map = getMap(t);
        if (map != null)
            map.set(this, value);
        else
            createMap(t, value);
    }
```

代码流程很清晰：

> 先拿到保存键值对的ThreadLocalMap对象实例map，如果map为空(即第一次调用的时候map值为null)，则去创建一个ThreadLocalMap对象并赋值给map，并把键值对保存在map

我们看到首先是拿到当前先线程实例**t**，任何将**t**作为参数构造**ThreadLocalMap**对象，为什么需要通过Threadl来获取**ThreadLocalMap对象**？**Thread**类和**ThreadLocalMap**有什么联系，那我们来看下getMap()方法的具体实现

```
    //ThreadLocal.java
    /**
     * Get the map associated with a ThreadLocal. Overridden in
     * InheritableThreadLocal.
     *
     * @param  t the current thread
     * @return the map
     */
    ThreadLocalMap getMap(Thread t) {
        return t.threadLocals;
    }
```

我们看到getMap实现非常直接，就是直接返回Thread对象的threadLocal字段。Thread类中的ThreadLocalMap字段声明如下：

```
    //Thread.java
    /* ThreadLocal values pertaining to this thread. This map is maintained
     * by the ThreadLocal class. */
    ThreadLocal.ThreadLocalMap threadLocals = null;
```

ok，我们总结一下：

> **ThreadLocal**的**set(T)** 方法中，首先是拿到当前线程**Thread**对象中的**ThreadLocalMap**对象实例**threadLocals**，然后再将需要保存的值保存到threadLocals里面。换句话说，每个线程引用的**ThreadLocal**副本值都是保存在当前**Thread**对象里面的。存储结构为**ThreadLocalMap**类型，**ThreadLocalMap**保存的类型为**ThreadLocal**，值为**副本值**

###### (2)、get方法

该方法返回当前线程所对应的线程局部变量

```
   /**
     * Returns the value in the current thread's copy of this
     * thread-local variable.  If the variable has no value for the
     * current thread, it is first initialized to the value returned
     * by an invocation of the {@link #initialValue} method.
     *
     * @return the current thread's value of this thread-local
     */
    public T get() {
        Thread t = Thread.currentThread();
        ThreadLocalMap map = getMap(t);
        if (map != null) {
            ThreadLocalMap.Entry e = map.getEntry(this);
            if (e != null)
                return (T)e.value;
        }
        return setInitialValue();
    }
```

> 同样的道理，拿到当前线程**Thread**对象实例中保存的**ThreadLocalMap**对象**map**，然后从**ma**p中读取键为**this**(即ThreadLocal类实例)对应的值。

如果map不是null，直接从map里面读取就好了，如果map==null，那么我们需要对当前线程Thread对象实例中保存ThreadLocalMap对象new一下。即通过setInitialValue()方法来创建，setInitialValue()方法的具体实现如下：

```
    /**
     * Variant of set() to establish initialValue. Used instead
     * of set() in case user has overridden the set() method.
     *
     * @return the initial value
     */
    private T setInitialValue() {
        T value = initialValue();
        Thread t = Thread.currentThread();
        ThreadLocalMap map = getMap(t);
        if (map != null)
            map.set(this, value);
        else
            createMap(t, value);
        return value;
    }
```

代码很清晰，通过createMap来创建ThreadLocalMap对象，前面set(T)方法里面的ThreadLocalMap也是通过createMap来的，我们看看createMap具体实现：

```
    /**
     * Create the map associated with a ThreadLocal. Overridden in
     * InheritableThreadLocal.
     *
     * @param t the current thread
     * @param firstValue value for the initial entry of the map
     * @param map the map to store.
     */
    void createMap(Thread t, T firstValue) {
        t.threadLocals = new ThreadLocalMap(this, firstValue);
    }
```

这样我们就对ThreadLocal的存储机制彻底清楚了。

###### (3)、remove()方法

```
         ThreadLocalMap m = getMap(Thread.currentThread());
         if (m != null)
             m.remove(this);
```

代码很简单，就是直接将当前线程局部变量的值删除，目的是为了减少内存的占用，该方法是JDK1.5新增的方法。需要指出的，当线程结束以后，对该应线程的局部变量将自动被垃圾回收，所以显示调用该方法清除线程的局部变量并不是必须的操作，但是它可以加速内存回收的数据。

###### 3、内部类ThreadLocalMap

先来看下内部类的注释

```
    /**
     * ThreadLocalMap is a customized hash map suitable only for
     * maintaining thread local values. No operations are exported
     * outside of the ThreadLocal class. The class is package private to
     * allow declaration of fields in class Thread.  To help deal with
     * very large and long-lived usages, the hash table entries use
     * WeakReferences for keys. However, since reference queues are not
     * used, stale entries are guaranteed to be removed only when
     * the table starts running out of space.
     */
    static class ThreadLocalMap {}
```

简单翻译一下：

> ThreadLocalMap是一个适用于维护线程本地值的自定义哈希映射(hash map)，没有任何操作可以让它超出ThreadLocal这个类的范围。该类是私有的，允许在Thread类中声明字段。为了更好的帮助处理常使用的，hash表条目使用了WeakReferences的键。但是，由于不实用引用队列，所以，只有在表空间不足的情况下，才会保留已经删除的条目

###### (1)、存储结构

通过注释我们知道ThreadLocalMap中存储的是ThreadLocalMap.Entry(后面用Entry代替)对象。因此，在ThreadLocalMap中管理的也就是Entry对象。也就是说，ThreadLocalMap里面大部分函数都是针对Entry。

首先ThreadlocalMap需要一个"[容器](https://cloud.tencent.com/product/tke?from_column=20065&from=20065)"来存储这些Entry对象，ThreadLocalMap中定义了额Entry数据实例table，用于存储Entry

```
        /**
         * The table, resized as necessary.
         * table.length MUST always be a power of two.
         */
        private Entry[] table;
```

也就是说，ThreadLocalMap维护一张哈希表(一个数组)，表里存储Entry。既然是哈希表，那肯定会涉及到加载因子，即当表里面存储的对象达到容量的多少百分比的时候需要扩容。ThreadLocalMap中定义了threshold属性，当表里存储的对象数量超过了threshold就会扩容。如下：

```
/**
 * The next size value at which to resize.
 */
private int threshold; // Default to 0

/**
 * Set the resize threshold to maintain at worst a 2/3 load factor.
 */
private void setThreshold(int len) {
    threshold = len * 2 / 3;
}
```

从上面代码可以看出，加载银子设置为2/3。即每次容量超过设定的len2/3时，需要扩容。

###### (2)、存储Entry对象

> hash散列的数据在存储过程中可能会发生碰撞，大家知道HashMap存储的是一个Entry链，当hash发生冲突后，将新的的Entry存放在链表的最前端。但是ThreadLocalMap不一样，采用的是index+1作为重散列的hash值写入。另外有一点需要注意key出现null的原因由于Entry的key是继承了弱引用，在下一次GC是不管它有没有被引用都会被回收。当出现null时，会调用replaceStaleEntry()方法循环寻找相同的key，如果存在，直接替换旧值。如果不存在，则在当前位置上重新创新的Entry。

看下代码：

```
    /**
         * Set the value associated with key.
         *
         * @param key the thread local object
         * @param value the value to be set
         */
        //设置当前线程的线程局部变量的值
        private void set(ThreadLocal key, Object value) {

            // We don't use a fast path as with get() because it is at
            // least as common to use set() to create new entries as
            // it is to replace existing ones, in which case, a fast
            // path would fail more often than not.

            Entry[] tab = table;
            int len = tab.length;
            int i = key.threadLocalHashCode & (len-1);

            for (Entry e = tab[i];
                 e != null;
                 e = tab[i = nextIndex(i, len)]) {
                ThreadLocal k = e.get();
                //替换掉旧值
                if (k == key) {
                    e.value = value;
                    return;
                }
                //和HashMap不一样，因为Entry key 继承了所引用，所以会出现key是null的情况！所以会接着在replaceStaleEntry()重新循环寻找相同的key
                if (k == null) {
                    replaceStaleEntry(key, value, i);
                    return;
                }
            }

            tab[i] = new Entry(key, value);
            int sz = ++size;
            if (!cleanSomeSlots(i, sz) && sz >= threshold)
                rehash();
        }


       /**
         * Increment i modulo len.
         */
        private static int nextIndex(int i, int len) {
            return ((i + 1 < len) ? i + 1 : 0);
        }
```

从上面源码中可以看出，通过key(ThreadLocal类型)的hashcode来计算存储的索引位置 i 。如果 i 位置已经存储了对象，那么就向后挪一个位置以此类推，直到找到空的位置，再讲对象存放。另外，在最后还需要判断一下当前的存储的对象个数是否已经超出了反之(threshold的值)大小，如果超出了，需要重新扩充并将所有的对象重新计算位置(rehash函数来实现)。那么我们看看rehash方法如何实现的。

###### (2.1)、rehash()方法

```
        /**
         * Re-pack and/or re-size the table. First scan the entire
         * table removing stale entries. If this doesn't sufficiently
         * shrink the size of the table, double the table size.
         */
        private void rehash() {
            expungeStaleEntries();

            // Use lower threshold for doubling to avoid hysteresis
            if (size >= threshold - threshold / 4)
                resize();
        }
```

看到，rehash函数里面先是调用了expungeStaleEntries()函数，然后再判断当前存储对象的小时是否超出了阀值的3/4。如果超出了，再扩容。不过这样有点混乱，为什么不直接扩容并重新摆放对象?

上面我们提到了，ThreadLocalMap里面存储的Entry对象本质上是一个WeakReference<ThreadLcoal>。也就是说，ThreadLocalMap里面存储的对象本质是一个队ThreadLocal的弱引用，该ThreadLocal随时可能会被回收！即导致ThreadLocalMap里面对应的 value的Key是null。我们需要把这样的Entry清除掉，不要让他们占坑。

expungeStaleEntries函数就是做这样的清理工作，清理完后，实际存储的对象数量自然会减少，这也不难理解后面的判断的约束条件为阀值的3/4，而不是阀值的大小。

###### (2.2)、expungeStaleEntries()与expungeStaleEntry()方法

```
        /**
         * Expunge all stale entries in the table.
         */
        private void expungeStaleEntries() {
            Entry[] tab = table;
            int len = tab.length;
            for (int j = 0; j < len; j++) {
                Entry e = tab[j];
                if (e != null && e.get() == null)
                    expungeStaleEntry(j);
            }
        }
```

expungeStaleEntries()方法很简单，主要是遍历table，然后调用expungeStaleEntry()，下面我们来主要讲解下这个函数expungeStaleEntry()函数。

###### (2.3) expungeStaleEntry()方法

> ThreadLocalMap中的expungeStaleEntry(int)方法的可能被调用的处理有：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/e3gb19def9.png)

expungeStaleEntry的调用.png

通过上面的图，不难看出，这个方法在ThreadLocal的set、get、remove时都会被调用。

```
        /**
         * Expunge a stale entry by rehashing any possibly colliding entries
         * lying between staleSlot and the next null slot.  This also expunges
         * any other stale entries encountered before the trailing null.  See
         * Knuth, Section 6.4
         *
         * @param staleSlot index of slot known to have null key
         * @return the index of the next null slot after staleSlot
         * (all between staleSlot and this slot will have been checked
         * for expunging).
         */
        private int expungeStaleEntry(int staleSlot) {
            Entry[] tab = table;
            int len = tab.length;

            // expunge entry at staleSlot
            tab[staleSlot].value = null;
            tab[staleSlot] = null;
            size--;

            // Rehash until we encounter null
            Entry e;
            int i;
            for (i = nextIndex(staleSlot, len);
                 (e = tab[i]) != null;
                 i = nextIndex(i, len)) {
                ThreadLocal k = e.get();
                if (k == null) {
                    e.value = null;
                    tab[i] = null;
                    size--;
                } else {
                    int h = k.threadLocalHashCode & (len - 1);
                    if (h != i) {
                        tab[i] = null;

                        // Unlike Knuth 6.4 Algorithm R, we must scan until
                        // null because multiple entries could have been stale.
                        while (tab[h] != null)
                            h = nextIndex(h, len);
                        tab[h] = e;
                    }
                }
            }
            return i;
        }
```

> 从上面代码中，可以看出先清理指定的Entry，再遍历，如果发现有Entry的key为null，就清理。Key==null，也就是ThreadLocal对象是null。所以当程序中，将ThreadLocal对象设置为null，在该线程继续执行时，如果执行另一个ThreadLocal时，就会触发该方法。就有可能清理掉Key是null的那个ThreadLocal对应的值。

所以说expungStaleEntry()方法清除线程ThreadLocalMap里面所有key为null的value。

###### (3)、获取Entry对象getEntry()

```
        /**
         * Get the entry associated with key.  This method
         * itself handles only the fast path: a direct hit of existing
         * key. It otherwise relays to getEntryAfterMiss.  This is
         * designed to maximize performance for direct hits, in part
         * by making this method readily inlinable.
         *
         * @param  key the thread local object
         * @return the entry associated with key, or null if no such
         */
        private Entry getEntry(ThreadLocal key) {
            int i = key.threadLocalHashCode & (table.length - 1);
            Entry e = table[i];
            if (e != null && e.get() == key)
                return e;
            else
                return getEntryAfterMiss(key, i, e);
        }
```

getEntry()方法很简单，直接通过哈希码计算位置 i ，然后把哈希表对应的 i 的位置Entry对象拿出来。如果对应位置的值为null，这就存在如下几种可能。

-   key 对应的值为null
-   由于位置冲突，key对应的值存储的位置并不是 i 位置上，即 i 位置上的null并不属于 key 值

因此，需要一个函数去确认key对应的value的值，即getEntryAfterMiss()方法

###### (3.1)、getEntryAfterMiss()函数

```
   /**
         * Version of getEntry method for use when key is not found in
         * its direct hash slot.
         *
         * @param  key the thread local object
         * @param  i the table index for key's hash code
         * @param  e the entry at table[i]
         * @return the entry associated with key, or null if no such
         */
        private Entry getEntryAfterMiss(ThreadLocal key, int i, Entry e) {
            Entry[] tab = table;
            int len = tab.length;

            while (e != null) {
                ThreadLocal k = e.get();
                if (k == key)
                    return e;
                if (k == null)
                    expungeStaleEntry(i);
                else
                    i = nextIndex(i, len);
                e = tab[i];
            }
            return null;
        }
```

所以ThreadlocalMap的getEntry()方法的整体流程如下：

> 第一步，从ThreadLocal的直接索引位置(通过ThreadLocal.threadLocalHashCode&(len-1)运算得到)获取Entry e，如果e不为null，并且key相同则返回e。 第二步，如果e为null或者key不一致则向下一个位置查询，如果下一个位置的key和当前需要查询的key相等，则返回应对应的Entry，否则，如果key值为null，则擦除该位置的Entry，否则继续向一个位置查询。

ThreadLocalMap整个get过程中遇到的key为null的Entry都被会擦除，那么value的上一个引用链就不存在了，自然会被回收。set也有类似的操作。这样在你每次调用ThreadLocal的get方法去获取值或者调用set方法去设置值的时候，都会去做这个操作，防止内存泄露，当然最保险的还是通过手动调用remove方法直接移除

###### (4)、ThreadLocalMap.Entry对象

前面很多地方都在说ThreadLocalMap里面存储的是ThreadLocalMap.Entry对象，那么ThreadLocalMap.Entry独享到底是如何存储键值对的?同时有是如何做到的对ThreadLocal对象进行弱引用？ 代码如下：

```
        /**
         * The entries in this hash map extend WeakReference, using
         * its main ref field as the key (which is always a
         * ThreadLocal object).  Note that null keys (i.e. entry.get()
         * == null) mean that the key is no longer referenced, so the
         * entry can be expunged from table.  Such entries are referred to
         * as "stale entries" in the code that follows.
         */
        static class Entry extends WeakReference<ThreadLocal> {
            /** The value associated with this ThreadLocal. */
            Object value;

            Entry(ThreadLocal k, Object v) {
                super(k);
                value = v;
            }
        }
```

从源码的继承关系可以看到，Entry是继承WeakReference<ThreadLocal>。即Entry本质上就是WeakReference<ThreadLocal>，换而言之，Entry就是一个弱引用，具体讲，Entry实例就是对ThreadLocal某个实例的弱引用。只不过，Entry同时还保存了value

整体模型如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/zlnoi0gv8z.png)

模型1.png

![](https://ask.qcloudimg.com/http-save/yehe-2957818/7gqtyo1l0f.png)

模型2.png

###### 4、总结

> ThreadLocal是解决线程安全的一个很好的思路，它通过为每个线程提供了一个独立的变量副本解决了额变量并发访问的冲突问题。在很多情况下，ThreadLocal比直接使用synchronized同步机制解决线程安全问题更简单，更方便，且结果程序拥有更高的并发性。ThreadLocal和synchronize用一句话总结就是一个用存储拷贝进行空间换时间，一个是用锁机制进行时间换空间。

其实补充知识:

-   ThreadLocal官方建议已定义成private static 的这样让Thread不那么容易被回收
-   真正涉及共享变量的时候ThreadLocal是解决不了的。它最多是当每个线程都需要这个实例，如一个打开[数据库](https://cloud.tencent.com/product/tencentdb-catalog?from_column=20065&from=20065)的对象时，保证每个线程拿到一个进而去操作，互不不影响。但是这个对象其实并不是共享的。

### 二、 Android中的ThreadLocal

##### (一) ThreadLocal的作用

> Android中的ThreadLocal和Java 的ThreadLocal是一致的，每个线程都有自己的局部变量，一个线程的本地变量对其他线程是不可见的，ThreadLocal不是用与解决共享变量的问题，不是为了协调线程同步而存在，而是为了方便每个线程处理自己的状态而引入的一个机制

##### (二) 源码跟踪

ThreadLocal的源代码在[ThreadLocal.java](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Flibcore%2Fluni%2Fsrc%2Fmain%2Fjava%2Fjava%2Flang%2FThreadLocal.java&objectId=1199400&objectType=1&isNewArticle=undefined)

###### 1、ThreadLocal的结构

![](https://ask.qcloudimg.com/http-save/yehe-2957818/j3phqt7b8d.png)

ThreadLocal的结构.png

可以直观地看到在android中的ThreadLocal类提供了一些方法和一个静态内部类Value，其中Values主要是用来保存线程的变量的一个类，它相当于一个容器，存储保存进来的变量。

###### 2、ThreadLocal的内部具体实现

首先看下成员变量

###### (1)、ThreadLocal的成员变量

```
    /** Weak reference to this thread local instance. */
    private final Reference<ThreadLocal<T>> reference
            = new WeakReference<ThreadLocal<T>>(this);

    /** Hash counter. */
    private static AtomicInteger hashCounter = new AtomicInteger(0);

    /**
     * Internal hash. We deliberately don't bother with #hashCode().
     * Hashes must be even. This ensures that the result of
     * (hash & (table.length - 1)) points to a key and not a value.
     *
     * We increment by Doug Lea's Magic Number(TM) (*2 since keys are in
     * every other bucket) to help prevent clustering.
     */
    private final int hash = hashCounter.getAndAdd(0x61c88647 * 2);
```

我们发现成员变量主要有3个，那我们就依次来说下

-   **reference**：通过弱引用存储ThreadLocal本身，主要是防止线程自身所带的数据都无法释放，避免OOM。
-   **hashCounter**：是线程安全的加减操作，getAndSet(int newValue)取当前的值，并设置新的值
-   **hash**：里面的0x61c88647 \* 2的作用是：在Value存在数据的主要存储数组table上，而table被设计为下标为0，2，4...2n的位置存放key，而1，3，5...(2n-1)的位置存放的value，0x61c88647\*2保证了其二进制最低位为0，也就是在计算key的下标时，一定是偶数位。

###### (2)、ThreadLocal的方法

ThreadLocal除了构造函数一共6个方法，我们就依次说下

###### ① get()方法

该方法返回线程局部变量的当前线程副本中的值

```
    /**
     * Returns the value of this variable for the current thread. If an entry
     * doesn't yet exist for this variable on this thread, this method will
     * create an entry, populating the value with the result of
     * {@link #initialValue()}.
     *
     * @return the current value of the variable for the calling thread.
     */
    @SuppressWarnings("unchecked")
    public T get() {
        // Optimized for the fast path.
        // 获取当前线程
        Thread currentThread = Thread.currentThread();
        // 获取当前线程的Value 实例
        Values values = values(currentThread);
        if (values != null) {
            Object[] table = values.table;
            //如果键值的key的索引为index，则所对应到的value索引为index+1
            //所以 hash&value.mask获取就是key的索引值
            int index = hash & values.mask;
            if (this.reference == table[index]) {
                return (T) table[index + 1];
            }
        } else {
           // 如果当前Value实例为空，则创建一个Value实例
            values = initializeValues(currentThread);
        }
        return (T) values.getAfterMiss(this);
    }
```

> 通过代码，我们得出**get()**是通过value.table这个数据通过索引值来找到值的

上面代码中调用了额initializeValues(Thread)方法，下面我们就来看一下

###### ② initializeValues(currentThread)方法

```
    /**
     * Creates Values instance for this thread and variable type.
     */
    Values initializeValues(Thread current) {
        return current.localValues = new Values();
    }
```

> 通过上面代码我们知道initializeValues(Thread)方法主要是直接new出一个新的Values对象。

###### ③ initialValue()方法

该方法返回此线程局部变量的当前线程的"初始值"

```
    /**
     * Provides the initial value of this variable for the current thread.
     * The default implementation returns {@code null}.
     *
     * @return the initial value of the variable.
     */
    protected T initialValue() {
        return null;
    }
```

> 也就是默认值为Null，当没有设置数据的时候，调用get()的时候，就返回Null,可以在创建ThreadLocal的时候复写initialValue()方法可以定义初始值。

###### ④ initialValue()方法

将此线程局部变量的当前线程副本中的值设置为指定值。

```
    /**
     * Sets the value of this variable for the current thread. If set to
     * {@code null}, the value will be set to null and the underlying entry will
     * still be present.
     *
     * @param value the new value of the variable for the caller thread.
     */
    public void set(T value) {
        //获取当前线程
        Thread currentThread = Thread.currentThread();
        // 获取 当前线程的Value
        Values values = values(currentThread);
        if (values == null) {
            values = initializeValues(currentThread);
        }
        // 将数据设置到Value
        values.put(this, value);
    }
```

###### ⑤ initialValue()方法

```
    /**
     * Removes the entry for this variable in the current thread. If this call
     * is followed by a {@link #get()} before a {@link #set},
     * {@code #get()} will call {@link #initialValue()} and create a new
     * entry with the resulting value.
     *
     * @since 1.5
     */
    public void remove() {
        Thread currentThread = Thread.currentThread();
        Values values = values(currentThread);
        if (values != null) {
            values.remove(this);
        }
    }
```

移除此线程局部变量的当前线程的值。

###### ⑥ initialValue()方法

```
    /**
     * Gets Values instance for this thread and variable type.
     */
    Values values(Thread current) {
        return current.localValues;
    }
```

> 通过线程获取Values对象。

###### 3、ThreadLocal静态类ThreadLocal.Values

ThreadLocal内部类Value是被设计用来保存线程的变量的一个类，它相当于一个容器，存储保存进来的变量。

###### 3.1 ThreadLocal.Values的结构

![](https://ask.qcloudimg.com/http-save/yehe-2957818/yyt90w6zws.png)

结构.png

主要原理是Values将[数据存储](https://cloud.tencent.com/product/cdcs?from_column=20065&from=20065)在数组table(Object\[\] table)中，那么键值对如何存放？就是table被设计为下标为0，2，4...2n的位置存放key，而1，3，5...(2n+1)的位子存放value。取数据的时候可以直接通过下标存取线程变量。

###### 3.2 ThreadLocal.Values的具体实现

###### (1) ThreadLocal.Values的成员变量

成员变量

```
        /**
         * Size must always be a power of 2.
         */
        private static final int INITIAL_SIZE = 16;

        /**
         * Placeholder for deleted entries.
         */
        private static final Object TOMBSTONE = new Object();

        /**
         * Map entries. Contains alternating keys (ThreadLocal) and values.
         * The length is always a power of 2.
         */
        private Object[] table;

        /** Used to turn hashes into indices. */
        private int mask;

        /** Number of live entries. */
        private int size;

        /** Number of tombstones. */
        private int tombstones;

        /** Maximum number of live entries and tombstones. */
        private int maximumLoad;

        /** Points to the next cell to clean up. */
        private int clean;
```

我们就来一次看一下：

-   INITIAL\_SIZE：默认的初始化容量，所以初始容量为16
-   TOMBSTONE: 表示被删除的实体标记，移除变量时它是把对应的key的位置赋值为TOMBSTONE
-   table：保存变量的地方，长度必须是2的N次方的值
-   mask：计算下标的掩码，它的值是table的长度-1
-   size：存放及拿来的实体的数量
-   tombstones：表示被删除的实体的数量
-   maximumLoad：阀值，用来判断是否需要进行rehash
-   clean：表示下一个要进行清理的位置点

###### (2) ThreadLocal.Values的无参构造函数

```
        /**
         * Constructs a new, empty instance.
         */
        Values() {
            //创建32为容量的容器，为什么是32不是16
            initializeTable(INITIAL_SIZE);
            this.size = 0;
                        this.tombstones = 0;
        }
```

我们知道INITIAL\_SIZE=16，那为什么是创建容量是32而不是16，那我们就来看一下initializeTable(int) 方法

###### (3) initializeTable(int) 方法

```
        /**
         * Creates a new, empty table with the given capacity.
         */
        private void initializeTable(int capacity) {
            //通过capacity创建table容器
            this.table = new Object[capacity * 2];
            //下标的掩码
            this.mask = table.length - 1;
            this.clean = 0;
            // 2/3的 最大负载因子
            this.maximumLoad = capacity * 2 / 3; // 2/3
        }
```

###### (4) ThreadLocal.Values的有参构造函数

```
        /**
         * Used for InheritableThreadLocals.
         */
        Values(Values fromParent) {
            this.table = fromParent.table.clone();
            this.mask = fromParent.mask;
            this.size = fromParent.size;
            this.tombstones = fromParent.tombstones;
            this.maximumLoad = fromParent.maximumLoad;
            this.clean = fromParent.clean;
            inheritValues(fromParent);
        }
```

通过代码我们知道，就是传入一个Value，然后根据这个传入的Value进行相应的值复制而已。

结合上面的无参的构造函数我们得出如下结论：ThreadLocal.Values有两个构造函数，一个是普通的构造函数，一个是类似于继承的那种，从一个父Values对象来生成新的Values。

###### (5) add(ThreadLocal,Object)方法

```
        /**
         * Adds an entry during rehashing. Compared to put(), this method
         * doesn't have to clean up, check for existing entries, account for
         * tombstones, etc.
         */
        void add(ThreadLocal<?> key, Object value) {
            for (int index = key.hash & mask;; index = next(index)) {
                Object k = table[index];
                if (k == null) {
                     //在index的位置上放入一个"键"
                    table[index] = key.reference;
                    //在 index+1的位置上放入一个"值"
                    table[index + 1] = value;
                    return;
                }
            }
        }
```

上面的代码向我们展示了table存储数据的方法，它是以一种类似于map的方法来存储的，在index处存入map的键，在index的下一位存入键对应的值，而这个键则是ThreadLocal的引用，这里毫无问题。但是有一个地方问题则是大大的有：int index=key.hash&mask。大家都明白这行代码的作用是获得可用的索引，可是到底是怎么获得的？为什么要通过这这种方式来获得？要解决这个问题，我们要先知道何为"可用的索引"，通过分析观察，我总结出了一些条件：

-   要是偶数
-   不能越界
-   尽可能能分散(尽可能的新产生的所以不要是已经出现过的数，不然table的空间不能充分的利用，而且观察上面的代码，会发现如果新产生的索引是已经出现过的数的话数据根本存不进去)

好的，现在我们来看看这个int index=key.hash&mask 究竟能不能搞定这些个问题。先来看下mask：

```
this.mask = table.length - 1;
```

> mask的值是table的长度减一，而我前面说过，table的长度是2N，也就说mask总是等于2N-1，这意味着mask的二进制表示总是N个1，那么这有说明什么？key.hash&mask的结果其实就是key.hash后面的n位！这样一来，首先上面的第二个条件已经满足了，因为N位无符号的二进制的范围是0~(2N-1)，刚好在table的范围之内。

这时候我们再来看下key.hash：

```
//1.5之后加入的一个类，它所持有的Integer可以原子的增加，其本身是线程安全的
private static AtomicInteger hashCounter = new AtomicInteger(0);
/**
 * Internal hash. We deliberately don't bother with #hashCode().
 * Hashes must be even. This ensures that the result of
 * (hash & (table.length - 1)) points to a key and not a value.
 *
 * We increment by Doug Lea's Magic Number(TM) (*2 since keys are in
 * every other bucket) to help prevent clustering.
 */
private final int hash = hashCounter.getAndAdd(0x61c88647 * 2);
```

hash的初始值是0，然后每次调用hash的时候，它都会先返回给你它的当前值，然后将当前值加上一个(0x61c88647 \* 2)。别的先不看，这样一来这个hash肯定是满足上面的第一个条件的：乘以2相当于二进制里左移一位，那么最后一位就肯定是0了，这样的话它与mask的与运算的结果肯定最后一位是0，也就是换成十进制之后肯定是偶数。这现在就剩第三个条件，其实肯定是满足的。

> 这里面的关键就是这个奇怪的数字**0x61c88647**，根源就在它的身上。它转换成为有符号32位二进制数是**0110 0001 1100 1000 1000 0110 0100 0111**，那么它的复数就是**1110 0001 1100 1000 1000 0110 0100 0111**（为什么这里可以用它的负数？因为其实它和它的负数在运算的时候结果是一样的，已面访根本到不了符号就已经内存溢出的，一方面在ThreadLocal里运算的时候有一个_2，所以它的符号其实已经没有了），运用计算的一些知识，我们知道在底层运算的时候其实用的就是它的补码，即_ _1001 1110 0011 0111 0111 1001 1011 1001_\*，这个数据十进制是2654435769，而2654435769这个数就有意思了。

![](https://ask.qcloudimg.com/http-save/yehe-2957818/i3lcqinln7.png)

公式 通过这个数，我们可以得到斐波拉契散列——这个散列中的数是绝对分散且不重复的——也就是说上面条件的第三点也已经满足了，如果有想进一步探索的同学可以自行研究。

###### (6) put(ThreadLocal,Object)方法

```
        /**
         * Cleans up after garbage-collected thread locals.
         */
        private void cleanUp() {
            if (rehash()) {
                // 如果rehash()方法返回的是true，就不需要继续clean up
                // If we rehashed, we needn't clean up (clean up happens as
                // a side effect).
                return;
            }

            //如果table的size是0的话，也不需要clean up ，因为都没有什么键值在里面好clean的
            if (size == 0) {
                // No live entries == nothing to clean.
                return;
            }

            // Clean log(table.length) entries picking up where we left off
            // last time.

             // 默认的clean是0，下一次在clean的时候是从上一次clean的地方继续。
            int index = clean;
            Object[] table = this.table;
            for (int counter = table.length; counter > 0; counter >>= 1,
                    index = next(index)) {
                Object k = table[index];
                //如果已经删除，则删除下一个
                if (k == TOMBSTONE || k == null) {
                    continue; // on to next entry
                }

                // The table can only contain null, tombstones and references.
                // 这个table只可能存储null，tombstones和references键
                @SuppressWarnings("unchecked")
                Reference<ThreadLocal<?>> reference
                        = (Reference<ThreadLocal<?>>) k;
                //如果ThreadLocal已经不存在了，则释放Value数据
                if (reference.get() == null) {
                    // This thread local was reclaimed by the garbage collector.
                    // 已经被释放
                    table[index] = TOMBSTONE;
                    table[index + 1] = null;
                    //被删除的数量
                    tombstones++;
                    size--;
                }
            }

            // Point cursor to next index.
            clean = index;
        }
```

cleanUp()方法只做了一件事，就是把失效的键放上TOMBSTONE去占位，饭后释放它的值。那么rehash()是干什么的其实已经显而易见了：

-   从字面意思来也知道是重新规划table的大小。 -联想clenUp()的作用，它都已经把失效键放上TOMBSTONE，然后肯定是想办法干掉这些TOMBSTONE。

cleanUp()方法只做了一件事，就是把失效的键放上TOMBSTONE去占位，然后释放它的值。那么rehash()是干什么的其实已经很显而易见了：

-   从字面意思来也知道是重新规划table的大小。
-   联想cleanUp()的作用，它都已经把失效键放上TOMBSTONE，接下来呢？显然是想办法干掉这些TOMBSTONE，还我内存一个朗朗乾坤喽。

###### (7) put(ThreadLocal,Object)方法

```
        /**
         * Sets entry for given ThreadLocal to given value, creating an
         * entry if necessary.
         */
        void put(ThreadLocal<?> key, Object value) {
             // 清理废弃的元素
            cleanUp();

            // Keep track of first tombstone. That's where we want to go back
            // and add an entry if necessary.
            int firstTombstone = -1;

            // 通过key获取索引index
            for (int index = key.hash & mask;; index = next(index)) {
                // 通过索引获取ThreadLocal
                Object k = table[index];
                // 如果ThreadLocal是存在，直接返回
                if (k == key.reference) {
                    // Replace existing entry.
                    table[index + 1] = value;
                    return;
                }

                // 找到firstTombstone这个索引值，然后赋值对应的key和value
                if (k == null) {
                    if (firstTombstone == -1) {
                        // Fill in null slot.
                        table[index] = key.reference;
                        table[index + 1] = value;
                        size++;
                        return;
                    }

                    // Go back and replace first tombstone.
                    table[firstTombstone] = key.reference;
                    table[firstTombstone + 1] = value;
                    tombstones--;
                    size++;
                    return;
                }

                // Remember first tombstone.
                if (firstTombstone == -1 && k == TOMBSTONE) {
                    firstTombstone = index;
                }
            }
        }
```

> 可以看到，put()方法里面的逻辑其实很简单，就是在想方设法的把传进来的兼职对给存进去——其中对获得的index的值进行了一些判断，以决定如何进行存储——总之是想要尽可能的节省空间。另外，值得注意的是，在遇到相同索引处存放着同一个键的时候，其采取的方式是新值换旧值。

###### (8) remove(ThreadLocal)方法

```
       /**
         * Removes entry for the given ThreadLocal.
         */
        void remove(ThreadLocal<?> key) {
            // 先把table清理一下
            cleanUp();

            for (int index = key.hash & mask;; index = next(index)) {
                Object reference = table[index];
                //把那个引用的用TOMBSTONE占用 
                if (reference == key.reference) {
                    // Success!
                    table[index] = TOMBSTONE;
                    table[index + 1] = null;
                    tombstones++;
                    size--;
                    return;
                }

                if (reference == null) {
                    // No entry found.
                    return;
                }
            }
        }
```

> 这个方法很简答， 就是把传进来的那个键对应的值给清理掉

###### (9) getAfterMiss(ThreadLocal)方法

```
        /**
         * Gets value for given ThreadLocal after not finding it in the first
         * slot.
         */
        Object getAfterMiss(ThreadLocal<?> key) {
            Object[] table = this.table;
            //通过散列算法得到ThreadLocal的first slot的索引值
            int index = key.hash & mask;

            // If the first slot is empty, the search is over.
            if (table[index] == null) {
                // 如果 first slot 上没有存储 则将ThreadLocal的弱引用和本地数据存储到table数组的相邻位置并返回本地数据对象的引用。
                Object value = key.initialValue();

                // If the table is still the same and the slot is still empty...
                if (this.table == table && table[index] == null) {
                    table[index] = key.reference;
                    table[index + 1] = value;
                    size++;

                    cleanUp();
                    return value;
                }
                // The table changed during initialValue().
                // 遍历table数组，根据不同判断将ThreadLocal的弱引用和本地数据对象引用存储到数组的相应位置
                put(key, value);
                return value;
            }

            // Keep track of first tombstone. That's where we want to go back
            // and add an entry if necessary.
            int firstTombstone = -1;

            // Continue search.
            for (index = next(index);; index = next(index)) {
                Object reference = table[index];
                if (reference == key.reference) {
                    return table[index + 1];
                }

                // If no entry was found...
                if (reference == null) {
                    Object value = key.initialValue();

                    // If the table is still the same...
                    if (this.table == table) {
                        // If we passed a tombstone and that slot still
                        // contains a tombstone...
                        if (firstTombstone > -1
                                && table[firstTombstone] == TOMBSTONE) {
                            table[firstTombstone] = key.reference;
                            table[firstTombstone + 1] = value;
                            tombstones--;
                            size++;

                            // No need to clean up here. We aren't filling
                            // in a null slot.
                            return value;
                        }

                        // If this slot is still empty...
                        if (table[index] == null) {
                            table[index] = key.reference;
                            table[index + 1] = value;
                            size++;

                            cleanUp();
                            return value;
                        }
                    }

                    // The table changed during initialValue().
                    put(key, value);
                    return value;
                }

                if (firstTombstone == -1 && reference == TOMBSTONE) {
                    // Keep track of this tombstone so we can overwrite it.
                    firstTombstone = index;
                }
            }
        }
```

> getAfterMiss函数根据不同的判断将ThreadLocal的弱引用和当前线程的本地对象以类似map的方式，存储在table数据的相邻位置，其中散列的所以hash值是通过hashCounter.getAndAdd(0x61c88647 \* 2) 算法来得到。

### 三、 Android 面试中的关于ThreadLocal的问题

-   问题1 ThreadLocal内部实现原理，怎么保证数据中仅被当前线程持有？
-   问题2 ThreadLocal修饰的变量一定不能被其他线程访问吗？
-   问题3 ThreadLocal的对象存放在哪里？
-   问题4 ThreadLocal会存在内存泄露吗？

##### (一) ThreadLocal内部实现原理，怎么保证数据中仅被当前线程持有？

> ThreadLocal在进行放值时的代码如下：

```
    /**
     * Sets the value of this variable for the current thread. If set to
     * {@code null}, the value will be set to null and the underlying entry will
     * still be present.
     *
     * @param value the new value of the variable for the caller thread.
     */
    public void set(T value) {
        Thread currentThread = Thread.currentThread();
        Values values = values(currentThread);
        if (values == null) {
            values = initializeValues(currentThread);
        }
        values.put(this, value);
    }
```

> ThreadLocal的值放入了当前线程的一个Values实例中，所以只能在本线程访问，其他线程无法访问。

##### (二) ThreadLocal修饰的变量一定不能被其他线程访问吗？

> 不是，对于子线程是可以访问父线程中的ThreadLocal修饰的变量的。如果在主线程中创建一个InheritableThreadLocal实例，那么在子线程中就可以得到InheritableThreadLocal实例，并获取相应的值。在ThreadLocal中的**inheritValues(Values fromParent)**方法获取父线程中的值

##### (三) ThreadLocal的对象存放在哪里？

> 是在堆上，在Java中，线程都会有一个栈内存，栈内存属于单个线程，其存储的变量只能在其所属线程中可见。但是ThreadLocal的值是被线程实例所有，而线程是由其创建的类型所持有，所以ThreadLocal实例实际上也是被其他创建的类所持有的，故它们都存在于堆上。

##### (四) ThreadLocal会存在内存泄露吗？

> 是不会，虽然ThreadLocal实例被线程中的Values实例所持有，也可以被看成是线程所持有，若使用线程池，之前的线程实例处理完后出于复用的目的依然存在，但Values在选择key时，并不是直接选择ThreadLocal实例，而是ThreadLocal实例的弱引用：

```
Reference<ThreadLocal<?>> reference = (Reference<ThreadLocal<?>>) k;
ThreadLocal<?> key = reference.get();
```

在get()方法中也是采用弱引用：

```
private final Reference<ThreadLocal<T>> reference
= new WeakReference<ThreadLocal<T>>(this);
if (this.reference == table[index]) {
return (T) table[index + 1];
```

这样能保存如查到当前thread被销毁时，ThreadLocal也会随着销毁被GC回收。

### 四、 ThreadLocal的总结

分析到这里，整个ThreadLocal的源码就分析的差不多了。在这里我们简单的总结一下这个类：

-   这个类之所以能够存储每个thread的信息，是因为它的内部有一个Values内部类，而Values中有一个Object组。
-   Objec数组是以一种近似于map的形式来存储数据的，其中偶数位存ThreadLocal的弱引用，它的下一位存值。
-   在寻址的时候，Values采用一种很神奇的方式——斐波拉契散列寻址Values里面的getAfterMiss()方式让人觉得很奇怪

### 五、思考

###### 这里大家思考一下，谷歌的Android团队为什么要重写ThreadLocal，而不是直接使用Java层面的ThreadLocal？

大概一两个月后我会更新这篇文章，发表我关于这个问题的思考

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2017.09.04 ，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除