---
created: 2025-04-15T14:19:55 (UTC +08:00)
tags: []
source: https://jekton.github.io/2018/04/12/binder-service-registration-part1/
author: 
---

# binder 情景分析 - service 的注册（上） | Jekton

> ## Excerpt
> 本篇为 service 注册的第一篇，主要讲述一些原理性的东西。service 注册部分只讲到获取 IServiceManager 对象。

---
本篇为 service 注册的第一篇，主要讲述一些原理性的东西。service 注册部分只讲到获取 `IServiceManager` 对象。

## [](https://jekton.github.io/2018/04/12/binder-service-registration-part1/#RPC-%E5%8E%9F%E7%90%86%E7%AE%80%E8%BF%B0 "RPC 原理简述")RPC 原理简述

在开始之前，我们先来了解一下基本的原理。

[![principle-of-RPC](https://jekton.github.io/2018/04/12/binder-service-registration-part1/principle-of-RPC.png)](https://jekton.github.io/2018/04/12/binder-service-registration-part1/principle-of-RPC.png "principle-of-RPC")principle-of-RPC

不同进程之间进行通信时，本质上还是只能够交换一下数据。方法、函数的调用是能够跨进程的。为了实现跨进程的函数调用，我们在原有 client 和 service 的基础上，增加两个对象——proxy 和 stub。客户端调用的，其实是 proxy 的函数。proxy 通过某些 IPC 通道，告知 stub。stub 读取 proxy 发生的数据，得知需要调用的函数后，再回调 service 对应的函数。

这样，从 client 和 service 的角度看，就好像是 client 调了 service 的函数。

  

## [](https://jekton.github.io/2018/04/12/binder-service-registration-part1/#%E5%9F%BA%E6%9C%AC%E7%9A%84%E6%9C%8D%E5%8A%A1%E6%B3%A8%E5%86%8C%E6%B5%81%E7%A8%8B "基本的服务注册流程")基本的服务注册流程

从应用的角度，服务的注册其实非常简单：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br></pre></td><td><pre><span>sp&lt;IServiceManager&gt; manager = defaultServiceManager();</span><br><span>manager-&gt;addService(<span>"serv_name"</span>, mBinder);</span><br></pre></td></tr></tbody></table>

这样，就成功注册了一个叫 `serv_name` 的服务。下面，我们就详细了解一下，在两个简单的调用背后，到底发生了什么。

> 注：以下 framework 源码使用 oreo-release 分支，kernel 部分使用 common 的 android-4.9-o-release 分支。部分代码为了可读性，在不影响结果的情况下作了删改。

  

## [](https://jekton.github.io/2018/04/12/binder-service-registration-part1/#%E8%8E%B7%E5%8F%96-context-manager-%E7%9A%84-IBinder "获取 context manager 的 IBinder")获取 context manager 的 `IBinder`

我们先来看看 `defaultServiceManager()` 函数：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br></pre></td><td><pre><span></span><br><span>sp&lt;IServiceManager&gt; defaultServiceManager() {</span><br><span>    <span>if</span> (gDefaultServiceManager != <span>NULL</span>) <span>return</span> gDefaultServiceManager;</span><br><span></span><br><span>    {</span><br><span>        AutoMutex _l(gDefaultServiceManagerLock);</span><br><span>        <span>while</span> (gDefaultServiceManager == <span>NULL</span>) {</span><br><span>            gDefaultServiceManager = interface_cast&lt;IServiceManager&gt;(</span><br><span>                ProcessState::self()-&gt;getContextObject(<span>NULL</span>));</span><br><span>            <span>if</span> (gDefaultServiceManager == <span>NULL</span>)</span><br><span>                sleep(<span>1</span>);</span><br><span>        }</span><br><span>    }</span><br><span></span><br><span>    <span>return</span> gDefaultServiceManager;</span><br><span>}</span><br></pre></td></tr></tbody></table>

这里关键的一句，便是：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br></pre></td><td><pre><span>gDefaultServiceManager = interface_cast&lt;IServiceManager&gt;(</span><br><span>    ProcessState::self()-&gt;getContextObject(<span>NULL</span>));</span><br></pre></td></tr></tbody></table>

`getContextObject(NULL)` 返回的，便是指向 `context manager` 的 `sp<IBinder>`：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br></pre></td><td><pre><span></span><br><span>sp&lt;IBinder&gt; ProcessState::getContextObject(<span>const</span> sp&lt;IBinder&gt;&amp; ) {</span><br><span>    <span>return</span> getStrongProxyForHandle(<span>0</span>);</span><br><span>}</span><br><span></span><br><span></span><br><span>sp&lt;IBinder&gt; ProcessState::getStrongProxyForHandle(<span>int32_t</span> handle) {</span><br><span>    sp&lt;IBinder&gt; result;</span><br><span></span><br><span>    AutoMutex _l(mLock);</span><br><span></span><br><span>    handle_entry* e = lookupHandleLocked(handle);</span><br><span></span><br><span>    <span>if</span> (e != <span>NULL</span>) {</span><br><span>        IBinder* b = e-&gt;binder;</span><br><span>        <span>if</span> (b == <span>NULL</span> || !e-&gt;refs-&gt;attemptIncWeak(<span>this</span>)) {</span><br><span>            b = <span>new</span> BpBinder(handle);</span><br><span>            e-&gt;binder = b;</span><br><span>            <span>if</span> (b) e-&gt;refs = b-&gt;getWeakRefs();</span><br><span>            result = b;</span><br><span>        } <span>else</span> {</span><br><span>            result.force_set(b);</span><br><span>            e-&gt;refs-&gt;decWeak(<span>this</span>);</span><br><span>        }</span><br><span>    }</span><br><span></span><br><span>    <span>return</span> result;</span><br><span>}</span><br></pre></td></tr></tbody></table>

这里的 `handle` 类似于文件描述符，通过这个 handle，binder 驱动就可以找到对应的 `struct binder_node`，而 `binder_node` 则关联着对应的服务。而 `handle = 0` 特指 context manager。

`lookupHandleLocked()` 会先查找本地的缓存。如果已经为对应的 `handle` 生成过 `BpBinder`，则直接返回。即便没有，`lookupHandleLocked()` 也会创建一个 `handle_entry`，但是 `e->binder` 为空。接下来 `new BpBinder(handle)`，并把新生成的对象放到缓存中。

  

## [](https://jekton.github.io/2018/04/12/binder-service-registration-part1/#%E5%90%84%E4%B8%AA%E7%B1%BB%E4%B9%8B%E9%97%B4%E7%9A%84%E5%85%B3%E7%B3%BB "各个类之间的关系")各个类之间的关系

`BpBinder` 实际上是 `IBinder` 的类。在本篇中会涉及到的类之间的继承关系如下：

[![relativeship-between-IBinder-IInterface](https://jekton.github.io/2018/04/12/binder-service-registration-part1/relativeship-between-IBinder-IInterface.png)](https://jekton.github.io/2018/04/12/binder-service-registration-part1/relativeship-between-IBinder-IInterface.png "relativeship-between-IBinder-IInterface")relativeship-between-IBinder-IInterface

前面我们获取的 `BpBinder` 其实是 `IBinder` 的子类。以 `BpXXX` 方式命名的，都是运行在客户端的代理。

`IBinder` 和 `IInterface` 之间的转换关系下：

[![transform-between-IBinder-IInterface](https://jekton.github.io/2018/04/12/binder-service-registration-part1/transform-between-IBinder-IInterface.png)](https://jekton.github.io/2018/04/12/binder-service-registration-part1/transform-between-IBinder-IInterface.png "transform-between-IBinder-IInterface")transform-between-IBinder-IInterface

`IInterface` 在下面一节开始说明。

  

## [](https://jekton.github.io/2018/04/12/binder-service-registration-part1/#%E8%8E%B7%E5%8F%96-IServiceManager-%E4%BB%A3%E7%90%86%E5%AF%B9%E8%B1%A1 "获取 IServiceManager 代理对象")获取 `IServiceManager` 代理对象

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br></pre></td><td><pre><span>gDefaultServiceManager = interface_cast&lt;IServiceManager&gt;(</span><br><span>    ProcessState::self()-&gt;getContextObject(<span>NULL</span>));</span><br></pre></td></tr></tbody></table>

再次回到一开始的这里，现在我们知道，`ProcessState::self()->getContextObject(NULL)` 会返回一个指向 `BpBinder` 的 `sp<IBinder>`。

下面是 `interface_cast`：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br></pre></td><td><pre><span></span><br><span><span>template</span>&lt;<span>typename</span> INTERFACE&gt;</span><br><span><span>inline</span> sp&lt;INTERFACE&gt; interface_cast(<span>const</span> sp&lt;IBinder&gt;&amp; obj)</span><br><span>{</span><br><span>    <span>return</span> INTERFACE::asInterface(obj);</span><br><span>}</span><br></pre></td></tr></tbody></table>

展开模板后是这样：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br></pre></td><td><pre><span><span>inline</span> sp&lt;IServiceManager&gt; interface_cast(<span>const</span> sp&lt;IBinder&gt;&amp; obj)</span><br><span>{</span><br><span>    <span>return</span> IServiceManager::asInterface(obj);</span><br><span>}</span><br></pre></td></tr></tbody></table>

而 `IServiceManager::asInterface()` 是用宏生成的：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br></pre></td><td><pre><span></span><br><span>IMPLEMENT_META_INTERFACE(ServiceManager, <span>"android.os.IServiceManager"</span>);</span><br><span></span><br><span></span><br><span></span><br><span><span>#<span>define</span> IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                       \</span></span><br><span>    <span>const</span> ::android::String16 I##INTERFACE::descriptor(NAME);           \</span><br><span>    <span>const</span> ::android::String16&amp;                                          \</span><br><span>            I##INTERFACE::getInterfaceDescriptor() <span>const</span> {              \</span><br><span>        <span>return</span> I##INTERFACE::descriptor;                                \</span><br><span>    }                                                                   \</span><br><span>    ::android::sp&lt;I##INTERFACE&gt; I##INTERFACE::asInterface(              \</span><br><span>            <span>const</span> ::android::sp&lt;::android::IBinder&gt;&amp; obj)               \</span><br><span>    {                                                                   \</span><br><span>        ::android::sp&lt;I##INTERFACE&gt; intr;                               \</span><br><span>        <span>if</span> (obj != <span>NULL</span>) {                                              \</span><br><span>            intr = <span>static_cast</span>&lt;I##INTERFACE*&gt;(                          \</span><br><span>                obj-&gt;queryLocalInterface(                               \</span><br><span>                        I##INTERFACE::descriptor).get());               \</span><br><span>            <span>if</span> (intr == <span>NULL</span>) {                                         \</span><br><span>                intr = <span>new</span> Bp##INTERFACE(obj);                          \</span><br><span>            }                                                           \</span><br><span>        }                                                               \</span><br><span>        <span>return</span> intr;                                                    \</span><br><span>    }                                                                   \</span><br><span>    I##INTERFACE::I##INTERFACE() { }                                    \</span><br><span>    I##INTERFACE::~I##INTERFACE() { }                                   \</span><br></pre></td></tr></tbody></table>

展开后，`IServiceManager::asInterface()` 是这样的：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br></pre></td><td><pre><span>sp&lt;IServiceManager&gt; IServiceManager::asInterface(<span>const</span> sp&lt;::android::IBinder&gt;&amp; obj)</span><br><span>{</span><br><span>    sp&lt;IServiceManager&gt; intr;</span><br><span>    <span>if</span> (obj != <span>NULL</span>) {</span><br><span>        intr = <span>static_cast</span>&lt;IServiceManager*&gt;(</span><br><span>            obj-&gt;queryLocalInterface(IServiceManager::descriptor).get());</span><br><span>        <span>if</span> (intr == <span>NULL</span>) {</span><br><span>            intr = <span>new</span> BpServiceManager(obj);</span><br><span>        }</span><br><span>    }</span><br><span>    <span>return</span> intr;</span><br><span>}</span><br></pre></td></tr></tbody></table>

从上面的类关系，我们知道，`BpBinder` 继承了 `IBinder`。同时，它也继承了 `queryLocalInterface` 的实现：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br></pre></td><td><pre><span></span><br><span>sp&lt;IInterface&gt;  IBinder::queryLocalInterface(<span>const</span> String16&amp; )</span><br><span>{</span><br><span>    <span>return</span> <span>NULL</span>;</span><br><span>}</span><br></pre></td></tr></tbody></table>

上面的代码最终会执行 `intr = new BpServiceManager(obj);`，返回的是一个 `BpServiceManager` 对象。

到这里，我们就完成了服务注册的第一步——获取一个 `IServiceManager`，还知道了它实际上是一个 `BpServiceManager`。

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br><span>22</span><br><span>23</span><br><span>24</span><br><span>25</span><br><span>26</span><br><span>27</span><br><span>28</span><br><span>29</span><br><span>30</span><br><span>31</span><br><span>32</span><br><span>33</span><br><span>34</span><br><span>35</span><br><span>36</span><br><span>37</span><br><span>38</span><br><span>39</span><br></pre></td><td><pre><span></span><br><span><span><span>class</span> <span>BpServiceManager</span> :</span> <span>public</span> BpInterface&lt;IServiceManager&gt;</span><br><span>{</span><br><span><span>public</span>:</span><br><span>    BpServiceManager(<span>const</span> sp&lt;IBinder&gt;&amp; impl)</span><br><span>          : BpInterface&lt;IServiceManager&gt;(impl)</span><br><span>    {</span><br><span>    }</span><br><span></span><br><span>    </span><br><span>}</span><br><span></span><br><span></span><br><span><span>template</span>&lt;<span>typename</span> INTERFACE&gt;</span><br><span><span>inline</span> BpInterface&lt;INTERFACE&gt;::BpInterface(<span>const</span> sp&lt;IBinder&gt;&amp; remote)</span><br><span>    : BpRefBase(remote)</span><br><span>{</span><br><span>}</span><br><span></span><br><span></span><br><span></span><br><span>BpRefBase::BpRefBase(<span>const</span> sp&lt;IBinder&gt;&amp; o)</span><br><span>    : mRemote(o.get()), mRefs(<span>NULL</span>), mState(<span>0</span>)</span><br><span>{</span><br><span>    extendObjectLifetime(OBJECT_LIFETIME_WEAK);</span><br><span></span><br><span>    <span>if</span> (mRemote) {</span><br><span>        mRemote-&gt;incStrong(<span>this</span>);           </span><br><span>        mRefs = mRemote-&gt;createWeak(<span>this</span>);  </span><br><span>    }</span><br><span>}</span><br><span></span><br><span></span><br><span><span><span>class</span> <span>BpRefBase</span> :</span> <span>public</span> <span>virtual</span> RefBase</span><br><span>{</span><br><span><span>protected</span>:</span><br><span>    <span><span>inline</span> IBinder* <span>remote</span><span>()</span> </span>{ <span>return</span> mRemote; }</span><br><span>    <span><span>inline</span> IBinder* <span>remote</span><span>()</span> <span>const</span> </span>{ <span>return</span> mRemote; }</span><br><span>}</span><br></pre></td></tr></tbody></table>

在 `BpServiceManager` 的构造函数里，它只是简单地将 `BpBinder` 传递给了父类 `BpInterface`。`BpInterface` 有继续传给父类，最终到了 `BpRefBase`，存在了成员变量 `mRemote` 里。

通过 `remote()` 函数，我们就可以重新拿到底层的 `BpBinder` 对象。

现在，我们终于完成了第一部分——获取一个 `IServiceManager` 对象。下一篇，我们再继续第二部分——服务的注册。
