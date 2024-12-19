这里强烈建议把前面两篇文章看一遍，因为前面两篇文章对后面大家对android的IPC的理解帮助很大，本片文章主要内容如下

-   1、Android IPC简介
-   2、Android 多进程模式
-   3、Serializable和Parcelable接口
-   4、Parcel类详解
-   5、智能指针

### 前言

进程Process 是程序的一个运行实例，以区别于"程序" 这一个静态概念；而线程(Thread) 则是CPU调度的单位。当前大部分操作系统都是支持多任务运行，这一特性让用户感到计算机好像可以同时处理很多事情。显然在只有一个CPU核心的情况下，这种"同时"是一种假象。它是操作系统先采用分时的方法，为正在运行的多任务分配合理的、单独的CPU来实现的。比如当前系统有5个任务，如果采用"平均分配"法且时间为10ms的话，那么各个任务每隔40ms就能被执行一次。只要机器速度足够快，用户的感觉就是所有任务都在同步运行。

### 一、Android IPC简介

##### (一) 什么是IPC

> IPC是Inter-Process Communication的缩写，含义为进程间通信或者跨进程通信，是指两个进程之间进行数据交换的过程。

这里我们不得不提下什么是进程，什么事线程，进程和线程是截然不同的歹念。按照操作系统中的描述，线程是CPU调度的最小单元，同时线程是一种有限的系统资源。而进程一般指一个执行单元，在PC和移动设备上指一个程序或者一个应用。一个进程可以包含多个线程，因此进程和线程是包含与被包含的关系。最简单的情况下，一个进中可以只有一个线程，即主线程，在Android里面主线程也叫UI线程，在UI线程里面才能操作界面。很多时候，一个进程中需要执行大量耗时任务，如果这些任务放到主线程中去执行就会造成界面无法响应，严重影响用户体验，这种情况在PC系统和移动系统中都存在，在Android中就叫ANR(Application Not Responding)，即应用无响应。解决这个问题就需要用到线程，把一些耗时的任务放在线程即可。

> IPC不是Android独有的，任何一个操作系统都需要响应的IPC机制，比如Windows上可以通过剪切板、管道等来进行进程间通信；Linux可以通过命名管道、共享内存、信号量等来进行进程间通信。可以看到不同的操作系统平台有着不同的进程间通信方式，对于Android来说，它是一种基于Linux内核的移动操作系统，它的进程间通信方式并不完全继承自Linux，相反它有自己的进程间通信方式。在Android中最有特色的进程间通信方式就是Binder了，通过Binder可以轻松地实现进程间通信。除了Binder，Android还支持Socket，通过Socket也可以实现任意两个终端之间的通信，当然同一个设备上的两个进程通过Socket自然也可以实现。

说到IPC的使用场景就必须提到多线程，只有面对多进程这种场景下，才需要考虑进程间通信。多进程的场景一般分为两种

-   1、一个app因为某些原因子什么需要采用多进程模式来实现的，至于原因可能很多，比如为了加大一个应用可使用的内存所以需要多进程来获取更多份的内存空间。Android对单个应用的内存做了最大限制，早期有16M，后面也有64M，不同设备有不同的大小。
-   2、一个app需要其他app提供的数据，由于是两个app，所以必须采用跨进程的方式来获取数据，甚至我们可以通过系统的ContentPorvider去查询数据的时候，其实也是一种跨进程通信方式，只不过通信的细节被系统内部屏蔽了，我们无法感知而已。后面会有单独的文章介绍ContentProvider。总之，不管由于何种原因，我么采用了多进程的设计方法，那么应用中就必须妥善地处理进程间通信的各种问题。

### 二、Android 多进程模式

我们先来了解下Android中的多进程模式。通过给四大组件指定**android:process** 属性，我们可以轻易地开启多进程模式，这看起来很简单，但是实际上使用中却问题很多，多进程远远没有我们想的那么简单，有些时候我们多进程中遇到的“坑”远远大于使用多进程带来的"好处"。那我们就来详细了解下

##### (一)、如何开启多进程模式

> 正常情况下，在Android中多进程是指一个应用中存在多个进程的情况下，因此这里不讨论两个应用之间的多进程情况。首先，在Android中使用多进程只有一种方式，那就是给四大组件(Activity、Service、Receiver、ContentProvider)在AndroidMenifest中添加 **"android:process"** 属性，除此之外没有其他方法，也就是说我们无法给一个线程或者一个实体类指定其运行时所在的进程。其实还有一种非常规的多进程方法，那就是通过JNI在native层去fork一个新的进程(5.0之前，我们 **进程保活**常用的 伎俩)，但是这种方法比较特殊，也不是常用的创建多进程方式，因此我们暂时不考虑这种方式。

举例如下：

```
        <activity
            android:name="com.test.process.demo.TestActivity1"
            android:process=":process_test1"
            android:screenOrientation="portrait" />

        <activity
            android:name="com.test.process.demo.TestActivity2"
            android:process="com.test.demo.process_test2"
            android:screenOrientation="portrait" />
```

> 上面的两个案例分别为TestActivity1和TestActivity2设置了 **"android:process"** 属性。所以这意味着当前应用增加了两个新的进程，假设当前包名为："com.test.process.demo",当TestActivity1被启动的时候会，系统会为它创建一个单独的进程，进程名为"com.test.process.demo.process\_test1"，当TestActivity2启动的时候，会为它创建一个单独的进程，进程名为"com.test.demo.process\_test2"。如果入口Activity是MainActivity，没有为它设置" android:process"所以默认它的进程名是包名。

大家仔细看代码会发现TestActivity1和TestActivity2在设置**"android:process"** 并不一样，那么这两种方式有区别吗？其实是有区别的，首先 \*\*":" \*\* 代表的是 要在当前进程名附加上当前的包名，这是一种简写的方式，对TestActivity1他的进程名为"com.test.process.demo.process\_test1"。对TestActivity2来说，它的声明方式是一种完整的命名方式，不会附加包的信息；其次，进程名以":"开头的进程属于当前应用的私有进程，其他应用通过ShareUID方式可以和它跑在同一个进程中。

> 我们知道Android系统会为每一个应用分配一个唯一的UID，具有相同的UID应用才能共享数据。这里要说明的是，两个应用通过ShareUID跑在同一个进程中是有要求的，需要这两个应用具有相同的ShareUID并且签名相同才可以。在这种情况下，它们可以相互访问对它们跑在同一个进程中，那么除了能共享data目录、组件信息，还可以共享内存数据，或者说它们看来就像是一个应用的两个部分。

##### (二)、多进程模式的运行机制

如果大家之前没有用过多进程模式，如果第一次运行多进程模式，会有很多"坑"。而且这些"坑"，打死大家也想不到。一般问题如下

-   1、静态成员和单利模式完全失效
-   2、线程同步机制完全失效
-   3、SharedPreferences的可靠性下降
-   4、Application会多次创建

咱们来简单说下

###### 1、静态成员和单利模式完全失效

我们来简单说下静态成员，比如在平时开发中我们会切换环境，开发环境和测试环境，正式环境中相互切换。代码入下：

```
public class URLConstant {
     // 1是开发环境，2是测试环境，3是正式环境
     public static String EnvironmentType = 1;  
}
```

假设默认是开发环境，我们在切换界面把环境变更为测试环境，在代码中把EnvironmentType设为2。这时候我们在TestActivity1里面获取EnvironmentType，我们会发现EnvironmentType仍然为1，而不是2。为什么会这样？

> 出现上面的问题是TestActivity1运行在一个单独的进程里面，我们知道Android系统为每一个应用分配了一个独立的虚拟机，或者说为每个进程都分配了一个独立的虚拟机，不同的虚拟机在内存上有不同的地址空间，这就导致在不同的虚拟机中访问同一个类对象会产生多分副本，我们上门的例子，进程com.test.process.demo.process\_test1和进程com.test.demo.process\_test2都存在一个类是URLConstant，且这两个类不相互影响，所以这就解释了为什么我们在切换界面修改了EnvironmentType，但是在TestActivity1里面取到的值还是1，因为TestActivity1里面的值还没有变化。

###### 2、线程同步机制完全失效

> 这个问题本质和上一个问题类似，既然不是一块内存了，那么不管是所对象还是锁全局类都无法保证同步，因为不同进程锁的不是同一个对象。所以同步机制一定会出问题。

###### 3、SharedPreferences的可靠性下降

> 是因为SharePreference不支持两个进程同时去执行写操作，否则会导致一定几率的数据丢失，这是因为SharedPreferences底层是通过读/写XML文件来实现的，并发写显然会出问题的，甚至并发读/写都有可能出问题。

###### 4、Application会多次创建

> 大家知道，当一个组件跑在一个新的进程中，由于系统要再创建新的进程同时分配独立的虚拟机，所以这个过程其实就是启动一个应用的过程。因此，相当于系统又把这个应用重新启动了一遍，既然重新启动了，那么自认会创建新的Applicatiaon。

### 三、Serializable和Parcelable接口

本节主要讲解三方面的内容Serializable接口和Parcelable接口以及Binder，只有熟悉这这两个接口后，我们才能在后面更好地理解跨进程通信。Serializable和Parcelable接口可以完成对象的序列化的过程，当我们需要通过Intent和Binder传输数据时就需要使用Parcelable或者Serializable，有时候我们还需要把对象持久化到存储设备上或者通过网络传输给其他客户端，这个时候也需要使用Seriazable来完成对象的持久化

##### (一) Serializable接口

###### 1、Serializable简介

Serializable 是Java所提供的一个序列化接口，它是一个空接口，为对象提供标准的序列化和反序列化操作。使用Serializable来实现序列化相当简单，只需要类在声明中指定一个类似下面的标示即可实现默认的序列化过程。

```
private static final long seriaVersionUID=1L;
```

所以如果想让一个类实现序列化，只需要将这个类实现Serializable接口，并声明一个seriaVersionUID即可，实际上，甚至这个seriaVersionUID也不是必需的，我们不声明这个serialVersionUID即可，实际上，甚至这个seriaVersionUID也不是必需的，我们不声明这个serialVersionUID，同样也可以实现反序列化，但是这将会对反序列化过程产生影响，具体影响我们后面介绍

###### 2、Serializable序列化和反序列化

我们举一个例子吧，Person类是一个实现了Serializable接口的类，它有3个字段，name，sex,age

```
public class Persion implements Serializable{
  private static final  long serialVersionUID=1L;
  public String name;
  public String sex;
  public int age;

  public Persion(String name,String sex,int age){
         this.name=name;
         this.sex=sex;
         this.age=age;
  }
}
```

通过Serializable方式来实现对象的序列化，实现起来非常简单，几乎所有工作都被系统自动完成了。如何进程对象的序列化和反序列化也非常简单，只需要采用ObjectOutputStream和ObjectInputStream即可轻松实现。代码如下：

```
//序列化过程
Person person=new Person("张三","男",23);
ObjectOutputStream out=new ObjectOutputStream(new FileOutStream("cache.txt"));
out.writeObject(person);
out.close();

//反序列化过程
ObjectInputStream in=new ObjectInputStream(new FileInputStream("cache.txt"));
Person newPerson=(Person)in.readObejct();
in.close();
```

上面的代码演示了采用Serializable方式序列化对象的典型过程，很简单，只需要把实现了Serializable接口的User对象写到文件中就可以快速恢复了，恢复后的对象newPerson和person内容完全一样，但是两者并不是同一个对象。

###### 3、serialVersionUID的作用

即使不指定serialVersionUID也可以实现序列化，那到底要不要指定呢？serialVersionUID后面的数字有什么含义？

-   这个serialVersionUID是用来辅助序列化和反序列化的过程。原则上序列化后的数据中的serialVersionUID只有和当前类的serialVersionUID一致才能成功的反序列化。
-   serialVersionUID的详细工作机制是这样的：序列化的时候系统会把当前类的serialVersionUID写入序列化的文件中(也可能是其他中介)，当反序列化的时候系统会去检测文件中的serialVersionUID，看它是否和当前类的serialVersionUID一致，如果一致就说明序列化的类的版本和当前类的版本是相同的，这个时候可以成功反序列化；否则就说明当前类和序列化的类相比发生了某些变换，比如成员变量的数量、类型可能会发生变化，这时候就无法正常的反序列化。会报如下错误:

```
java.io.InvalidClassException
```

所以一般来说，我们应该手动去指定serialVersionUID的值，比如"1L",也可以让IDE根据当前类的结构去生成对应的hash值，这样序列化和反序列化时两者的serialVersionUID是相同的，因此可以正常的进程反序列化。如果不不设置serialVersionUID，系统在序列化的时候默认会根据类的结构在生成对应的serialVersionUID，在反序列化的时候，如果当天类有变化，比如增加或者减少字段，这时候当前的类的serialVersionUID和序列化的时候的serialVersionUID就不一样了，就会出现反序列化失败，如果没有捕获异常会导致crash

###### 所以当我们手动制订了它之后，就可以很大程度上避免了泛学历化过程的失败。

比如当版本升级以后，我们可能删除了某个成员变量也可能增加一些新的成员变量，这个时候我们的反序列化过程仍然可以成功，程序仍然能够最大限度地恢复数据。相反 如果我们没有指定serialVersionUID的话，程序就会挂掉。当然我们也要考虑到另外一种情况，如果类结构发生了给常规性的改变，比如修改了类名，修改了成员变量的类型，这个时候尽管serialVersionUID验证通过了，但是反序列化过程还是会失败，因为类的而机构有了重大改变，根本无法从老版本的数据还原出一个新的类结构对象。

###### 4、补充

通过上面的分析，我们知道给serialVersionUID指定为1L或者采用IDE根据当前类结构去生成的hash值，这两者并没有本质区别，效果完全一样。再补充三点：

-   1、静态成员变量属于类，不属于对象，所以不会参与序列化的过程
-   2、用transient关键字编辑的成员变量不参与序列化的过程。
-   3、可以通过重写writeObject()和readObject()两个方法来重写系统默认的序列化和反序列化的过程。不过本人并不推荐

##### (二) Parcelable接口

Parcelable也是一个接口，只要实现了这个接口，一个类的对象就可以实现序列化和并且通过Intent和Binder传递。我们就借用上面Person类来看下代码：

```
public class Person implements Parcelable{
    private static final  long serialVersionUID=1L;
    public String name;
    public String sex;
    public int age;

    public Person(String name,String sex,int age){
        this.name=name;
        this.sex=sex;
        this.age=age;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(name);
        dest.writeString(sex);
        dest.writeInt(age);
    }
    
    public static final Parcelable.Creator<Person> CREATOR= new Creator<Person>() {
        @Override
        public Person createFromParcel(Parcel source) {
            return new Person(source);
        }

        @Override
        public Person[] newArray(int size) {
            return new Person[size];
        }
    };
    
    private Person(Parcel source){
        name=source.readString();
        sex=source.readString();
        age=source.readInt();
    }
}
```

这里先说一下Parcel，Parcel内部包装了可序列化的数据，可以在Binder中自由传输，从上面的代码我们可以看出，在序列化的过程中，需要实现的功能有序列化，反序列化的和内容描述。

-   1、序列化功能由writeToParcel来完成，最终是通过Parcel中的一些列write方法来完成的。
-   2、反序列化是由CREATOR来完成，其内部标明了如何创建序列化对象和数组，并通过Parcel的一些列read方法来完成反序列化过程。
-   3、内容描述功能由describeContents方法来完成，几乎在所有情况下这个方法都应该返回0，仅当前对象中存在文件描述符时，此方法返回1。

Parcelable的方法说明：

|  |  |  |
| --- | --- | --- |
| 
createFromParcel(Parcel source)



 | 

从序列化后的对象中创建原始对象



 |
| 

newArray



 | 

创建指定长度的原始对象数组



 |
| 

Person(Parcel source)



 | 

从序列化后的对象中创建原始对象



 |
| 

writeToParcel(Parcel dest, int flags)



 | 

从当前对象吸入序列化结构中，其中flag标识有两种值0或者1，为1时标识当前对象需要作为返回值返回，不能立刻释放资源，几乎所有情况都为0



 | 

Parcelable.PARCELABLE\_WRITE\_RETURN\_VALUE



 |
| 

describeContents



 | 

返回当前对象的内容描述，如果含有文件描述符，返回1，否则返回0，几乎所有情况都是返回0



 | 

Parcelable.CONTENTS\_FILE\_DESCRIPTOR



 |

系统已经为我们提供了许多实现了Parcelable接口的类，它们都是可以直接序列化的，比如Intent，Bundle，Bitmap等，同时List和Map页可以序列化，不过要求它们的每一个元素都是可以序列化的。

##### (三) Serializable 和Parcelable的区别

###### 1、平台区别

-   Serializable是属于 **Java** 自带的，表示一个对象可以转换成可存储或者可传输的状态，序列化后的对象可以在网络上进行传输，也可以存储到本地。
-   Parcelable 是属于 **Android** 专用。不过不同于Serializable，Parcelable实现的原理是将一个完整的对象进行分解。而分解后的每一部分都是Intent所支持的数据类型。

###### 2、编写上的区别

-   Serializable代码量少，写起来方便
-   Parcelable代码多一些，略复杂

###### 3、选择的原则

-   1、如果是仅仅在内存中使用，比如activity、service之间进行对象的传递，强烈推荐使用Parcelable，因为Parcelable比Serializable性能高很多。因为Serializable在序列化的时候会产生大量的临时变量， 从而引起频繁的GC。
-   2、如果是持久化操作，推荐Serializable，虽然Serializable效率比较低，但是还是要选择它，因为在外界有变化的情况下，Parcelable不能很好的保存数据的持续性。

###### 4、本质的区别

-   1、Serializable的本质是使用了反射，序列化的过程比较慢，这种机制在序列化的时候会创建很多临时的对象，比引起频繁的GC、
-   2、Parcelable方式的本质是将一个完整的对象进行分解，而分解后的每一部分都是Intent所支持的类型，这样就实现了传递对象的功能了。

### 四、Parcel类详解

##### (一)、故事

关于在进程间传递数据，举两个例子

###### 例子1：快递包裹

比如从上海快递一件衣服，给北京的朋友，显眼途径很多，无论是陆海空的物流或者快递，在整个传递过程中"衣服"本身始终没有变过——朋友拿到的衣服还是原来的那件

###### 例子2：email发送图片

假设你在上海通过电子邮件给北京的朋友，发送一张"衣服的图片"，那么当朋友看到它时，已经无法估计这张图片数据，在传输过程中被复制了多少次了——但可以肯定的是，他看到的图像和原始图像绝对是一模一样的。

很显然，进程间的数据传递和例2属于同一种情况，不过略有区别。

> 如果只是一个int型数值，不断复制知道目标进程即可。但是如果是某个对象呢？我们可以想象下，同一进程间的对象传递都是通过引用来做的，因而本质上就是传递了一个内存的地址。这种方式明显在跨进程情况下就不行了。由于采用了虚拟内存机制，两个进程都有自己独立的内存地址空间，所以进程间传递地址值是无效的。而在进程间传递数据是Binder机制的重要一环，大家不要着急，下一节就是要讲解Binder了。Android系统中负责跨进程通信的就是Parcel。那我们就来看下Parcel

##### (二)、Parcel简介

> Parcel翻译过来就是打包的意思，其实就是包装了我们需要传输的数据，然后在Binder中传输，用于跨进程传输数据。就上问说的，直接传送内存地址是不行的，那么如果把进程A中的内存相关数据打包起来，然后寄送到进程B中，有B在自己的进程"复现"这个对象。Parcel在Android中扮演了跨进程传输的集装箱的角色，是序列化的一种手段。Parcel提供了一套机制，可以将序列化之后的数据写入一个共享内存中，其他进程通过Parcel可以从这块共享内存读出字节流，并反序列化成对象，如下图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/g8o7frwvbj.png)

Parcel模型.png

所以说：Parcel是一个存放读取数据的[容器](https://cloud.tencent.com/product/tke?from_column=20065&from=20065)，系统中的binder进程间通信(IPC)就使用了Parcel类来进行客户端与服务端数据交互，而且AIDL的数据也是通过Parcel来交互的。在Java层和C++层都实现了Parcel，由于它在C/C++中，直接使用了内存来读取数据，因此，它更有效率。

> 其实Parcel是内存中的结构的是一块连续的内存，会自动根据需要自动扩展大小(这个设计比较赞，一些对性能要求不是太高或者小数据的地方，可以不用费脑向分配多大的空间)。Parcel传递数据，可以分为3种，传递方式也不一样：

-   小型数据：从用户空间(源进程)copy到kernel空间(Binder驱动中)再写回用户空间(目标进程，binder驱动负责寻找目标进程)
-   大型数据：使用android 的匿名共享内存 (Ashmem)传递
-   binder对象：Kernel binder驱动专门处理

##### (二)、Parcel.java类介绍

由于篇幅限制，我之类就不详细讲解Parcel类，主要讲解两个部分，一个是类注释，一个是核心方法。

###### 1、Parcel.java类注释

我直接上注释了，就不贴出源码了

-   Parcel是一个消息(数据和对象的引用)的容器，IBinder是android用于进程间通信的方式(IPC)。Parcel可以携带序列化后(flattened/marshalled/serialized，通过使用多种类型的writing函数或者Parcelable接口)的数据，在IPC的另外一个反序列化数据(变回反序列化的对象)，Parcel也可以携带活的IBinder对象，传递到Parcel中与原本IBinder相连的代理Binder中。
-   Parcel不是通用的序列化机制(Android特有的，Java的序列化机制是Serilizable，在效率上不如Parcel)。这个class(以及相应的ParcelableAPI，用于将任意对象转换为Parcel)被设计为高性能的的IPC传输方式。因此将Parcel数据放置在持久化存储位置是不合适的，任何基于Parcel中数据前世的变化都会导致旧数据不可读。

后面就是关于支持的类型，总结一下如下：

-   与Parcel相关的API有多种读写不同类型的方式，主要的6种类型如：
-   Primitives 基本数据类型
-   Primitives Arrays 基本数据类型数组
-   Parcelables 实现了Parcelables接口的对象
-   Bundle类
-   Active Objects 类 主要包括 Binder对象和FileDescriptor对象
-   Untyped Containers 容器，比如List、Map、SparseArray

###### 2、常用方法

###### (1)、Parcel设置相关

母庸质疑，存入的数据越多，Parcel所占用的内存空间越大。我们可以通过以下方法来进行相关设置。

-   dataSize()：得到当前Parcel对象的实际存储空间
-   setDataCapacity(int size)：设置Parcel的空间大小，显然存储的数据不能大于这个值
-   setDataPosition(int pos)：改变Parcel中的读写位置(我喜欢叫偏移量)，必须介于0和dataSize()间
-   dataAvail()：当前Parcel中的可读数据大小。
-   dataCapacity()：当前Parce的存储能力
-   dataPosition()：数据的当前位置值(偏移量)，有点类似于游标
-   dataSize()：当前Parcel所包含的数据大小

PS:如果写入数据时，系统发现已经超出了Parcel的存储能力，它会自动申请所需要的内存空间，并扩展dataCapacity；并且每次写入都是从dataPosition()开始的。

###### (2)、Primitives

原始类型数据的读写操作。比如：

-   writeInt(int) ：写入一个整数
-   writeFloat(float) ：写入一个浮点数(单精度)
-   writeDouble(double)：写入一个双精度
-   writeString(string)：写入一个字符串
-   readInt(int) ：读出一个整数
-   readFloat(float) ：读出一个浮点数(单精度)
-   readDouble(double)：写入一个双精度
-   readString(string)：写入一个字符串

从中也可以看出读写操作是配套的，用哪种方式写入的数据就是要用相应的方式正式读取。另外，数据是按照host cup的字节序来读写的

###### (3)、Primitives Arrays

原始数据类型的数组读写操作通常是先用4个字节表示数据的大小值，接着才写入数据本身。另外用户既可以选择将数据读入现有的数据空间中，也可以让Parcel返回一个新的数组，此类方法如下：

-   writeBooleanArray(boolean\[\])：写入布尔数组
-   readBooleanArray(boolean\[\])：读出布尔数组
-   boolean\[\] createBooleanArray()：读取并返回一个布尔数组
-   writeByteArray(byte\[\] ):写入字节数组
-   writeByteArray(byte\[\],int , int ) 和上面几个不同的是，这个函数最后面的两个参数分别表示数组中需要被写入的数据起点以及需要写入多少。
-   readByteArray(byte\[\])：读取字节数组
-   byte\[\] createByteArray()：读取并返回一个数组

###### (4)、Parcelables

遵循Parcelable协议的对象可以通过Parcel来存取，如开发人员经常用的的bundle就是实现Parcelable的，与此类对象相关的Parcel操作包括：

-   writeParcelable(Parcelable，int)：将这个Parcel类的名字和内容写入Parcel中，实际上它是通过回调此Parcelable的writeToParcel()方法来写入数据的。
-   readParcelable(ClassLoader)：读取并返回一个新的Parcelable对象
-   writeParcelableArray(T\[\]，int)：写入Parcelable对象数组。
-   readParcelable(ClassLoader)：读取并返回一个Parcelable数组对象

###### (5)、Bundle

上面已经提到了，Bundle继承自Parcelable，是一种特殊的type-safe的容器。Bundle的最大特点是采用键值对的方式存储数据，并在一定程度上优化了读取效率。这个类型的Parcel操作包括：

-   writeBundle(Bundle)：将Bundle写入parcel
-   readBundle()：读取并返回一个新的Bundle对象
-   readBundle(ClassLoader)：读取并返回一个新的Bundle对象，ClassLoader用于Bundle获取对应的Parcelable对象。

###### (6)、Activity Object

Parcel的另一个强大武器就是可以读写Active Object，什么是Active Object？通常我们存入Parcel的是对象的内容，而Active Object 写入的则是他们的特殊标志引用。所以在从Parcel中读取这些对象时，大家看到的并不是重新创建的对象实例，而是原来那个被写入的实例。可以猜想到，能够以这种方式传输的对象不会很多，目前主要有两类

-   1、Binder：Binder一方面是Android系统IPC通信的核心机制之一，另一方面也是一个对象。利用Parcel将Binder写入，读取时就能得到原始的Binder对象，或者是它的特殊代理实现(最终操作的还是原始Binder对象)，与此相关的操作包括：
    -   writeStrongBinder(IBinder)
    -   writeStrongInterface(IInterface)
    -   readStrongBinder()
-   2、FileDescriptor：FileDescriptor是Linux中的文件描述符，可以通过Parcel如下方法进行传递：
    -   writeFileDescriptor(FileDescriptor)
    -   readFileDescriptor()

因为传递后的对象仍然会基于和原对象相同的文件流进行操作，因而可以认为是Active Object的一种

###### (7)、Untyped Containers

它用于读写标准的任意类型的java容器。包括：

-   writeArray(Object\[\])
-   readArray(ClassLoader)
-   writeList(List)
-   readList(List, ClassLoader)
-   readArrayList(ClassLoader)
-   writeMap(Map)
-   readMap(Map, ClassLoader)
-   writeSparseArray(SparseArray)
-   readSparseArray(ClassLoader)

###### (8)、Parcel创建与回收

-   obtain()：获取一个Parcel，可以理解new了一个对象，其实是有一个Parcel池
-   recyle()：清空，回收Parcel对象的内存

###### (9)、异常的读写

-   writeException()：在Parcel队头写入一个异常
-   readException()：在Parcel队头读取，若读取值为异常，则抛出该异常；否则程序正常运行

###### 3、创建Parcl对象

###### (1)、obtain()方法

app可以通过Parcel。obtain()接口来获取一个Parcel对象。

```
    /**
     * Retrieve a new Parcel object from the pool.
     * 从Parcel池中取出一个新的Parcel对象
     */
    public static Parcel obtain() {
        //系统预先产出的一个Parcel池(其实就是一个Parcel数组)，大小为6
        final Parcel[] pool = sOwnedPool;
        synchronized (pool) {
            Parcel p;
            for (int i=0; i<POOL_SIZE; i++) {
                p = pool[i];
                if (p != null) {
                    //引用置为空，这样下次就知道这个Parcel已经被占用了
                    pool[i] = null;
                    if (DEBUG_RECYCLE) {
                        p.mStack = new RuntimeException();
                    }
                    return p;
                }
            }
        }
        //如果Parcel池中已经空，就直接新建一个。
        return new Parcel(0);
    }
```

> 在这里，我们要注意到这里的池其实是一个数组，从里面提取对象的时候，从头扫描到尾，找不到为null的手，直接new一个Parcle对象并返回。

那我们就继续看下他的构造函数

###### (2)、Parcel构造函数

我们来看下它的构造函数

```
private Parcel(long nativePtr) {
        if (DEBUG_RECYCLE) {
            mStack = new RuntimeException();
        }
        //Log.i(TAG, "Initializing obj=0x" + Integer.toHexString(obj), mStack);
        init(nativePtr);
    }
```

通过上面代码，我们知道，构造函数里面什么都没做，只是调用了init()函数，注意传入的是nativePtr是0

```
    private void init(long nativePtr) {
        if (nativePtr != 0) {
             //如果nativePtr不是0
            mNativePtr = nativePtr;
            mOwnsNativeParcelObject = false;
        } else {
            //如果nativePtr是0
            // nativeCreate() 为本地层代码准备的指针
            mNativePtr = nativeCreate();
            mOwnsNativeParcelObject = true;
        }
    }

  private long mNativePtr; // used by native code
  private static native long nativeCreate();
```

我们知道最终是调用了nativeCreate()方法

###### (3)、nativeCreate()方法

根据上篇文章讲解的关于JNI的方法，我们得知nativeCreate对应的是JNI层的/frameworks/base/core/jni中，实际上Parcel.java只是一个简单的中介，最终所有类型的读写操作都是通过本地代码实现的: 在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_Parcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined)中

```
//frameworks/base/core/jni/android_os_Parcel.cpp    551行
static jlong android_os_Parcel_create(JNIEnv* env, jclass clazz)
{
    Parcel* parcel = new Parcel();
    return reinterpret_cast<jlong>(parcel);
}
```

所以说mNativePtr变量实际上是一个本底层的Parcel(C++)对象

这里顺便说一下Parcel-Native的构造函数 代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 343行

```
Parcel::Parcel()
{
    LOG_ALLOC("Parcel %p: constructing", this);
    initState();
}
```

构造函数很简单就是调用了 **initState()** 函数，让我们来看下 **initState()**函数。

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 1901行

```
void Parcel::initState()
{
    LOG_ALLOC("Parcel %p: initState", this);
    mError = NO_ERROR;
    mData = 0;
    mDataSize = 0;
    mDataCapacity = 0;
    mDataPos = 0;
    ALOGV("initState Setting data size of %p to %zu", this, mDataSize);
    ALOGV("initState Setting data pos of %p to %zu", this, mDataPos);
    mObjects = NULL;
    mObjectsSize = 0;
    mObjectsCapacity = 0;
    mNextObjectHint = 0;
    mHasFds = false;
    mFdsKnown = true;
    mAllowFds = true;
    mOwner = NULL;
    mOpenAshmemSize = 0;
}
```

初始话很简单，几乎都是初始化为 0（NULL） 的

###### (4)、Parcel.h:

> 其实每一个Parcel对象都有一个Native对象相对应(以后均简称Parcel-Native对象，而Parcel对象均指Java层)，该Native对象就是实际的写入读出的一个对象，java端对它的引用是上面**mNativePtr**。

对应的Native层的Parcel定义是在 [/frameworks/native/inlcude/binder/Parcel.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Finclude%2Fbinder%2FParcel.h&objectId=1199093&objectType=1&isNewArticle=undefined)

```
class Parcel {
public:
    ...
    int32_t             readInt32() const; // 举个例子
    ...
    status_t            writeInt32(int32_t val);
    ...
private:
    uint8_t*            mData;
    size_t              mDataSize;
    size_t              mDataCapacity;
    mutable size_t      mDataPos;
}
```

> Parcel对象的数据读取、写入操作都是最终通过Parcel-Native对象搞定的，这里有几个需要注意的重要成员，mData就是[数据存储](https://cloud.tencent.com/product/cdcs?from_column=20065&from=20065)的起始指针，mDataSize总数据大小，mDataCapacity是指mData总空间 (包括已用和可用)大小，这个空间是变长的，mDataPos是指当前数据可写入的内存其实位置，mData总是一块连续的内存地址，每一次其总空间大小增长都会通过realloc进行内存分配，如果数据量过大、内存碎片过多导致内存分配失败就会报错。

###### (5)、总结:

使用Parcel一般是通过Parcel.obtain()从对象池中获取一个新的Parcel对象，如果对象池中没有则直接new的Parcel则直接创建新的一个Parcel对象，并且会自动创建一个Parcel-Native对象。

那我们就来看下回收的工作

###### 3、Parcell对象回收

###### (1)、 Parcel-Native对象的回收

上面说了创建Parcel对象的时候，会自动创建一个Parcel-Native对象。那这个Parcel-Native是什么时候回收的那？答案是：在Parcel对象销毁时，即finalize()时回收的，代码如下：

```
// Parcel.java
    @Override
    protected void finalize() throws Throwable {
        if (DEBUG_RECYCLE) {
            if (mStack != null) {
                Log.w(TAG, "Client did not call Parcel.recycle()", mStack);
            }
        }
        destroy();
    }

    private void destroy() {
        if (mNativePtr != 0) {
            if (mOwnsNativeParcelObject) {
                nativeDestroy(mNativePtr);
                updateNativeSize(0);
            }
            mNativePtr = 0;
        }
    }

    private static native void nativeDestroy(long nativePtr);
```

我们看到在finalize()方法里面调用了 destroy()方法，而在destroy()方法里面调用了nativeDestroy(long)方法，那我们就继续跟踪下

```
//frameworks/base/core/jni/android_os_Parcel.cpp      567行
static void android_os_Parcel_destroy(JNIEnv* env, jclass clazz, jlong nativePtr)
{
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    delete parcel;
}
```

我们知道了直接delete了 parcel对象

###### (2)、 recycle()方法

> sOwneredPool算是一个Parcel对象池(sHolderPool也是，二者区别在于sOwnerPool用于保存Parcel-Native对象生命周期需要由自己管理的Parcel对象)，可以复用之前创建好的Parcel对象，我们在使用完Parcel对象后，可以通过recycle方法回收到对象池中

直接上代码

```
    /**
     * Put a Parcel object back into the pool.  You must not touch
     * the object after this call.
     * 将Parcel对象放回池中。 这次调用之后，你就无法继续联系他了
     */
    public final void recycle() {
        if (DEBUG_RECYCLE) mStack = null;
        //Parcel-Native 对象的数据清空
        freeBuffer();

        final Parcel[] pool;
        if (mOwnsNativeParcelObject) {
            //选择合适的对象池
            pool = sOwnedPool;
        } else {
            mNativePtr = 0;
            pool = sHolderPool;
        }

        synchronized (pool) {
            for (int i=0; i<POOL_SIZE; i++) {
                if (pool[i] == null) {
                   //如果有位置就直接加入对象池。
                    pool[i] = this;
                    return;
                }
            }
        }
    }
```

加入对象池之后的对象除非重新靠obtain()启动，否则不能直接使用，因为它时刻都可能被其他地方获取使用导致数据错误。

###### 4、存储空间

> Parcel内部的存储区域主要有两个，是mData和mObjects，mData存储是基本数据类型，mObjects存储Binder数据类型。Parcel提供了针对各种数据写入和读出的操作函数。这两块区域都是使用malloc分配出来的。

###### 4.1、flat\_binder\_object

在Parcel的序列化中，Binder对象使用flat\_binder\_object结构体保存。同时提供了flatten\_binder和unflatten\_binder函数用于序列化和反序列化。 结构体代码在[Linux的binder.h](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttps%3A%2F%2Fgithub.com%2Ftorvalds%2Flinux%2Fblob%2Fmaster%2Finclude%2Fuapi%2Flinux%2Fandroid%2Fbinder.h&objectId=1199093&objectType=1&isNewArticle=undefined) 68行

```
/*
 * This is the flattened representation of a Binder object for transfer
 * between processes.  The 'offsets' supplied as part of a binder transaction
 * contains offsets into the data where these structures occur.  The Binder
 * driver takes care of re-writing the structure type and data as it moves
 * between processes.
 */
struct flat_binder_object {
    struct binder_object_header hdr;
    __u32               flags;

    /* 8 bytes of data. */
    union {
        binder_uintptr_t    binder; /* local object */
        __u32           handle; /* remote object */
    };
    /* extra data associated with local object */
    binder_uintptr_t    cookie;
};
```

这里先说下，Binder实体或引用在传递时，被表示为flat\_binder\_object，flat\_binder\_object的type域表示 表示传输Binder的类型。

-   跨进程的时候：flat\_binder\_object的type为BINDER\_TYPE\_HANDLE 跨进程的时候
-   非跨进程的时候：flat\_binder\_object的type为BINDER\_TYPE\_BINDER

###### 5、关于偏移量

###### (1)、基本类型

看到了了前面的内容，相信大家对Parcel有了初步的了解，那么Parcel内部存储机制是怎么样的？偏移量又是什么东西？我们一起回忆一下基本数据类型的取值范围：

| 类型      | bit数量  | 字节   |
| ------- | ------ | ---- |
| boolean | 1 bit  | 1字节  |
| char    | 16bit  | 2字节  |
| int     | 32bit  | 4字节  |
| long    | 64 bit | 8 字节 |
| float   | 32 bit | 4 字节 |
| double  | 64bit  | 8字节  |

如果大家对C语言熟悉的话，C语言中结构体的内存对齐和Parcel采用的内存存放机制一样，即读取最小字节为32bit，也即4个字节。高于4个字节的，以实际数据类型进行存放，但得为4byte的倍数。基本公式如下：

-   实际存放字节： 辨别一：32bit (<=32bit) **例如：boolean，char，int** 辨别二：实际占用字节(>32bit) **例如：long，float，String 等**
-   实际读取字节： 辨别一：32bit (<=32bit) **例如：boolean，char，int** 辨别二：实际占用字节(>32bit) **例如：long，float，String 组等**

所以，当我们写入/读取一个数据时，偏移量至少为4byte(32bit)，于是，偏移量的公式如下：

事实上，我们可以通过setDataPosition(int position)来直接操作我们欲读取数据时的偏移量。毫无疑问，你可以设置任何便宜量，但是所读取值的类型是有误的。因此设置便宜量读取值的时候，需要小心。

###### (2)、注意事项

> PS: 我们在writeXXX()和readXXX()时，导致的偏移量时共用的，例如我们在writeIn(23)后，此时的dataposition=4，如果我想读取它，简单的通过readInt()是不行的，只能得到0，这时我们只能通过setDataPosition(0)设置为起始偏移量，从起始位置读取四个字节，即可得23。因此，在读取某个值时，需要使用setDataPosition(int)使偏移量偏移到我们的指定位置。

###### (3)、取值规范

由于可能存在读取值的偏差，一个默认的取值规范为：

-   1、读取复杂对象时：对象匹配时，返回当前偏移位置的对象；对象不匹配时，返回null。
-   2、读取简单对象时：对象匹配时，返回当前偏移位置的对象：对象不匹配时，返回0。

###### (4)、存放空间图

下面，给出一张浅显的Parcel的存放空间图

![](https://ask.qcloudimg.com/http-save/yehe-2957818/2rftci68wt.png)

Parcel空间存放图.png

###### 6 、Int类型数据写入

我们以writeInt()为例进行数据写入的跟踪

时序图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/6n01h1qjgc.jpeg)

writeInt.jpeg

###### (1) Parcel.writeInt(int)

```
    /**
     * Write an integer value into the parcel at the current dataPosition(),
     * growing dataCapacity() if needed.
     */
    public final void writeInt(int val) {
        nativeWriteInt(mNativePtr, val);
    }
```

方法翻译如下：

> 在当前的dataPosition()中，将一个int的值写入Parcel中，如果空间不足，则扩容。

通过代码我们知道其实调用的nativeWriteInt(long nativePtr, int val)，我们知道nativeWriteInt(long nativePtr, int val)其实对应的是JNI的方法

###### (2) android\_os\_Parcel\_writeInt(JNIEnv\*,jclass,jlong,jint)函数

代码在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_Parcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 233行

```
static void android_os_Parcel_writeInt(JNIEnv* env, jclass clazz, jlong nativePtr, jint val) {
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        const status_t err = parcel->writeInt32(val);
        if (err != NO_ERROR) {
            signalExceptionForError(env, clazz, err);
        }
    }
}
```

我们看到实际上是调用的 Parcel-Native类的writeInt32(jint)函数

###### (3) Parcel::writeInt32(int32\_t val)函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 748行

```
status_t Parcel::writeInt32(int32_t val)
{
    return writeAligned(val);
}
```

我们看到实际上是调用的 Parcel-Native类的writeAligned()函数

###### (4) Parcel::writeAligned(T val)函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 1148行

```
template<class T>
status_t Parcel::writeAligned(T val) {
    COMPILE_TIME_ASSERT_FUNCTION_SCOPE(PAD_SIZE_UNSAFE(sizeof(T)) == sizeof(T));
    if ((mDataPos+sizeof(val)) <= mDataCapacity) {
restart_write:
        //分支二
        *reinterpret_cast<T*>(mData+mDataPos) = val;
        return finishWrite(sizeof(val));
    }
    //分支一  刚刚创建Parcel-Native对象
    status_t err = growData(sizeof(val));
    if (err == NO_ERROR) goto restart_write;
    return err;
}
```

writeAligned看名字就知道是内存对齐，第一句我没太搞明白，好像是验证是否内存对齐，好像能够根据编译选项判断，应该是打开某个编译选项，如果size没有内存对齐，直接报错，内存对齐都是搞一些位运算(这里是4个字节对齐)。

> writeAligned()函数判断当前pos+要写入的数据占用的内存 是否比mDataCapacity大，如果打，就是空间不足，需要自动增长空间，走growData()

这里我们假设刚刚创建了 Parcel-Native对象。这时候mDataCapacity=0,mDataPos=0,mData=0,mDataSizeDataSize=0,所这时候不走**分支二**，走**分支一** ,在**分支一** 里面调用了growData()函数 分配内存

###### (5) Parcel::growData(size\_t len)函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 1683行

```
status_t Parcel::growData(size_t len)
{
     //如果超过int的最大值
    if (len > INT32_MAX) {
        // don't accept size_t values which may have come from an
        // inadvertent conversion from a negative int.
        return BAD_VALUE;
    }

    size_t newSize = ((mDataSize+len)*3)/2;
    //通过上面的代码newSize一般来说都会大于mDataSize
    return (newSize <= mDataSize)
            ? (status_t) NO_MEMORY
            : continueWrite(newSize);
}
```

> PS:这里是parcel的增长算法，((mDataSize+len)\*3/2)；带有一定预测性的增长，避免频繁的空间调整(每次调整都需要重新malloc内存的，频繁的话会影响效率)。然后这里有个newSize< mDataSize就认为NO\_MEMORY。这是如果溢出了(是负数)，就认为申请不到内存了。

因为newSize一般来说都会大于mDataSize，所以函数最后走到了continueWrite(newSize)函数里面去了

###### (6) Parcel::continueWrite(size\_t desired)函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 1743行

```
status_t Parcel::continueWrite(size_t desired)
{
    if (desired > INT32_MAX) {
        // don't accept size_t values which may have come from an
        // inadvertent conversion from a negative int.
        return BAD_VALUE;
    }

    // If shrinking, first adjust for any objects that appear
    // after the new data size.
    size_t objectsSize = mObjectsSize;
    if (desired < mDataSize) {
        if (desired == 0) {
            objectsSize = 0;
        } else {
            while (objectsSize > 0) {
                if (mObjects[objectsSize-1] < desired)
                    break;
                objectsSize--;
            }
        }
    }
    //分支一
    if (mOwner) {
        // If the size is going to zero, just release the owner's data.
        if (desired == 0) {
            freeData();
            return NO_ERROR;
        }

        // If there is a different owner, we need to take
        // posession.
        uint8_t* data = (uint8_t*)malloc(desired);
        if (!data) {
            mError = NO_MEMORY;
            return NO_MEMORY;
        }
        binder_size_t* objects = NULL;

        if (objectsSize) {
            objects = (binder_size_t*)calloc(objectsSize, sizeof(binder_size_t));
            if (!objects) {
                free(data);

                mError = NO_MEMORY;
                return NO_MEMORY;
            }

            // Little hack to only acquire references on objects
            // we will be keeping.
            size_t oldObjectsSize = mObjectsSize;
            mObjectsSize = objectsSize;
            acquireObjects();
            mObjectsSize = oldObjectsSize;
        }

        if (mData) {
            memcpy(data, mData, mDataSize < desired ? mDataSize : desired);
        }
        if (objects && mObjects) {
            memcpy(objects, mObjects, objectsSize*sizeof(binder_size_t));
        }
        //ALOGI("Freeing data ref of %p (pid=%d)", this, getpid());
        mOwner(this, mData, mDataSize, mObjects, mObjectsSize, mOwnerCookie);
        mOwner = NULL;

        LOG_ALLOC("Parcel %p: taking ownership of %zu capacity", this, desired);
        pthread_mutex_lock(&gParcelGlobalAllocSizeLock);
        gParcelGlobalAllocSize += desired;
        gParcelGlobalAllocCount++;
        pthread_mutex_unlock(&gParcelGlobalAllocSizeLock);

        mData = data;
        mObjects = objects;
        mDataSize = (mDataSize < desired) ? mDataSize : desired;
        ALOGV("continueWrite Setting data size of %p to %zu", this, mDataSize);
        mDataCapacity = desired;
        mObjectsSize = mObjectsCapacity = objectsSize;
        mNextObjectHint = 0;
    //分支二 
    } else if (mData) {
        if (objectsSize < mObjectsSize) {
            // Need to release refs on any objects we are dropping.
            const sp<ProcessState> proc(ProcessState::self());
            for (size_t i=objectsSize; i<mObjectsSize; i++) {
                const flat_binder_object* flat
                    = reinterpret_cast<flat_binder_object*>(mData+mObjects[i]);
                if (flat->type == BINDER_TYPE_FD) {
                    // will need to rescan because we may have lopped off the only FDs
                    mFdsKnown = false;
                }
                release_object(proc, *flat, this, &mOpenAshmemSize);
            }
            binder_size_t* objects =
                (binder_size_t*)realloc(mObjects, objectsSize*sizeof(binder_size_t));
            if (objects) {
                mObjects = objects;
            }
            mObjectsSize = objectsSize;
            mNextObjectHint = 0;
        }

        // We own the data, so we can just do a realloc().
        if (desired > mDataCapacity) {
            uint8_t* data = (uint8_t*)realloc(mData, desired);
            if (data) {
                LOG_ALLOC("Parcel %p: continue from %zu to %zu capacity", this, mDataCapacity,
                        desired);
                pthread_mutex_lock(&gParcelGlobalAllocSizeLock);
                gParcelGlobalAllocSize += desired;
                gParcelGlobalAllocSize -= mDataCapacity;
                pthread_mutex_unlock(&gParcelGlobalAllocSizeLock);
                mData = data;
                mDataCapacity = desired;
            } else if (desired > mDataCapacity) {
                mError = NO_MEMORY;
                return NO_MEMORY;
            }
        } else {
            if (mDataSize > desired) {
                mDataSize = desired;
                ALOGV("continueWrite Setting data size of %p to %zu", this, mDataSize);
            }
            if (mDataPos > desired) {
                mDataPos = desired;
                ALOGV("continueWrite Setting data pos of %p to %zu", this, mDataPos);
            }
        }
    //分支三
    } else {
        // This is the first data.  Easy!
        uint8_t* data = (uint8_t*)malloc(desired);
        if (!data) {
            mError = NO_MEMORY;
            return NO_MEMORY;
        }

        if(!(mDataCapacity == 0 && mObjects == NULL
             && mObjectsCapacity == 0)) {
            ALOGE("continueWrite: %zu/%p/%zu/%zu", mDataCapacity, mObjects, mObjectsCapacity, desired);
        }

        LOG_ALLOC("Parcel %p: allocating with %zu capacity", this, desired);
        pthread_mutex_lock(&gParcelGlobalAllocSizeLock);
        gParcelGlobalAllocSize += desired;
        gParcelGlobalAllocCount++;
        pthread_mutex_unlock(&gParcelGlobalAllocSizeLock);

        mData = data;
        mDataSize = mDataPos = 0;
        ALOGV("continueWrite Setting data size of %p to %zu", this, mDataSize);
        ALOGV("continueWrite Setting data pos of %p to %zu", this, mDataPos);
        mDataCapacity = desired;
    }
    return NO_ERROR;
}
```

PS: mOwner是个函数指针。 其实这个方法写的不是很好，那么多分支，应该增加几个工具函数，这样函数代码量就不会这么大了。简单说下三个主要分支

-   分支一：如果设置了release函数指针(即mOwener)，调用release函数进行处理
-   分支二：没有设置release函数指针，但是mData中存在数据，需要在原来的数据的基础上扩展存储空间。
-   分支三：没有设置release函数指针，并且mData不存在数据(就是注释上说的第一次使用)，调用malloc申请内存块，保存mData。设置相应的设置capacity、size、pos、object的值。

> 承接上文，这里应该走分支分支三，分支三很简单，主要是调用malloc()方法分配一块(mDataSize+size(val))\*3/2大小的内存，然后让mData指向该内存，并且将这里可以归纳一下，growData()方法只是分配了一内存。

根据返回值，又回到了Parcel::writeAligned(T val)中，由于返回值是NO\_ERROR，所以就走到了**goto restart\_write** ,这样就又到了Parcel::writeAligned(T val) 分支二中

###### (7) Parcel::writeAligned(T val)函数 分支二

分之二代码就两行，如下

```
        *reinterpret_cast<T*>(mData+mDataPos) = val;
        return finishWrite(sizeof(val));
```

-   1、\* reinterpret\_cast<T\*>(mData+mDataPos) = val; 这行代码是直接获取当前地址强制转化指针类型，然后赋值。
-   2、调用finishWrite()函数

###### (8) Parcel::finishWrite(size\_t len)函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 642行

```
status_t Parcel::finishWrite(size_t len)
{
    if (len > INT32_MAX) {
        // don't accept size_t values which may have come from an
        // inadvertent conversion from a negative int.
        return BAD_VALUE;
    }

    //printf("Finish write of %d\n", len);
    mDataPos += len;
    ALOGV("finishWrite Setting data pos of %p to %zu", this, mDataPos);
    if (mDataPos > mDataSize) {
        mDataSize = mDataPos;
        ALOGV("finishWrite Setting data size of %p to %zu", this, mDataSize);
    }
    //printf("New pos=%d, size=%d\n", mDataPos, mDataSize);
    return NO_ERROR;
}
```

这个方法比较简单，主要内容如下：

> mDataPos增加到刚刚写入数据的末尾，并且进行一个判断，如果mDataPos>mDataSize的话，就将mDataSize=mDataPos;而实际上mDataSize在赋值前还是0，所以会进行这个赋值操作，因此我们可以知道，其实mDataSize是记录当前mData中写入数据的大小。

###### 7 、Int类型数据读出

我们以readInt()为例进行数据写入的跟踪

时序图如下：

![](https://ask.qcloudimg.com/http-save/yehe-2957818/83n8d92r6w.jpeg)

readInt.jpeg

###### (1) Parcel.readInt(int)

```
    /**
     * Read an integer value from the parcel at the current dataPosition().
     */
    public final int readInt() {
        return nativeReadInt(mNativePtr);
    }

    private static native int nativeReadInt(long nativePtr);
```

方法注释翻译如下：

> 从当前dataPosition()的位置上读取一个Interger值

通过代码我们知道其实是调用的nativeReadInt(long nativePtr)，我们知道nativeReadInt(long nativePtr)其实对应的是JNI的方法

###### (2) android\_os\_Parcel\_readInt(JNIEnv\* env, jclass clazz, jlong nativePtr) 函数

代码在[android\_os\_Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fbase%2Fcore%2Fjni%2Fandroid_os_Parcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 379行

```
static jint android_os_Parcel_readInt(JNIEnv* env, jclass clazz, jlong nativePtr)
{
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        return parcel->readInt32();
    }
    return 0;
}
```

这个代码也简单，主要是直接调用了Parcel-Native的readInt32()函数

###### (3) Parcel::readInt32() 函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 1168行

```
int32_t Parcel::readInt32() const
{
    return readAligned<int32_t>();
}
```

这个代码也很简单，内部调用了readAligned<int32\_t>() 函数

###### (4) readAligned<int32\_t>() 函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 1138行

```
template<class T>
T Parcel::readAligned() const {
    T result;
    if (readAligned(&result) != NO_ERROR) {
        result = 0;
    }
    return result;
}
```

其实内部是有调用了Parcel::readAligned(T \*pArg)函数

> 注意：Parcel::readAligned(T \*pArg)和 Parcel::readAligned()的区别，一个是有入参的，一个是无入参的。

###### (5) readAligned<int32\_t>() 函数

代码在[Parcel.cpp](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flink.jianshu.com%2F%3Ft%3Dhttp%3A%2F%2Fandroidxref.com%2F6.0.1_r10%2Fxref%2Fframeworks%2Fnative%2Flibs%2Fbinder%2FParcel.cpp&objectId=1199093&objectType=1&isNewArticle=undefined) 1124行

```
template<class T>
status_t Parcel::readAligned(T *pArg) const {
    COMPILE_TIME_ASSERT_FUNCTION_SCOPE(PAD_SIZE_UNSAFE(sizeof(T)) == sizeof(T));

    if ((mDataPos+sizeof(T)) <= mDataSize) {
        const void* data = mData+mDataPos;
        mDataPos += sizeof(T);
        *pArg =  *reinterpret_cast<const T*>(data);
        return NO_ERROR;
    } else {
        return NOT_ENOUGH_DATA;
    }
```

这个就是根据 mData+mDataPos 和具体的类型，进行强制类型转化获取对应的值。

###### (6) 注意事项:

同进程情况下，数据读取过程跟写入几乎一致，由于使用的是同一个Parcel对象，mDataPos首先要调整到0之后才能读取，同进程数据写入/读取并不会有什么效率提高，仍然会进行内存的考虑和分配，所以一般来说尽量避免同进程使用Parcel传递[大数据](https://cloud.tencent.com/product/bigdata-class?from_column=20065&from=20065)。