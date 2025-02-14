**本文摘要  
**

本文是Android四大组件系统系列的第六篇文章，通过本文您将了解到Activity“重生”是啥，它的作用是啥，Activity“重生”的原理是啥。(文中代码基于Android13)

注：ATMS是ActivityTaskManagerService的简称，AMS是ActivityManagerService的简称。

四大组件管理系统系列文章：

[开篇----Android四大组件系统系列](https://mp.weixin.qq.com/s?__biz=MzI2MjIyMjczMQ==&mid=2653047449&idx=1&sn=be4f793ad426ba3bd9eeb72b871c9bea&token=82304530&lang=zh_CN&scene=21#wechat_redirect)

[深度解读ActivityManagerService----Android四大组件系统系列](https://mp.weixin.qq.com/s?__biz=MzI2MjIyMjczMQ==&mid=2653047470&idx=1&sn=386e32c14b35b3da6271104421de999a&token=555436730&lang=zh_CN&scene=21#wechat_redirect)

[深入理解ActivityRecord和Task---Activity管理系列--Android四大组件系统系列](https://mp.weixin.qq.com/s?__biz=MzI2MjIyMjczMQ==&mid=2653047484&idx=1&sn=9522562c766fd42d5683687c5fbd3e48&token=82304530&lang=zh_CN&scene=21#wechat_redirect)

[App端框架之谜---Android四大组件系统系列](https://mp.weixin.qq.com/s?__biz=MzI2MjIyMjczMQ==&mid=2653047522&idx=1&sn=10e9850bdb83db2ecd5a2661524e54c4&token=596231474&lang=zh_CN&scene=21#wechat_redirect)

[深度解读ActivityTaskManagerService---Android四大组件系统系列](https://mp.weixin.qq.com/s?__biz=MzI2MjIyMjczMQ==&mid=2653047559&idx=1&sn=f9e151618cf4af32e515905b0047f5fb&token=1138630026&lang=zh_CN&scene=21#wechat_redirect)

## **本文大纲**

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/YicS15heicaRC71m06HDXAkTJwpibFqbdHFJBvic1CaLsEKiaQR6C33Z8GRzInMNvHEA4NcGdFNTF6sPcVFJz8xorMw/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

_1_

Activity“重生”机制？

大家好，我是Activity“重生”机制，Activity“重生”机制是指Activity由于低内存或者配置信息发生变化 (比如系统语言、字体大小、横竖屏等)，导致Activity有可能会被杀掉，如果Activity被杀掉了，当再次进入该Activity时，该Activity会重新启动。这里的重新启动和普通的启动可是如下区别的：

1.  前者会接收到系统保存的一些数据 (如果在Activity被杀掉之前保存了数据)，而后者不会。
    
2.  前者Activity所在的Task与被杀掉之前是一样的，而后者Activity所在的Task是会变的。
    
3.  前者Activity的onCreate、onStart、onResume、onRestoreInstanceState方法会被执行，而后者Activity的onCreate、onStart、onResume会被执行。
    
4.  如果App中打开了多个Activity，前者多个Activity之间的前后关系是不变的，而后者不存在这种情况。
    

当然上面提到的配置信息发生变化并不一定会导致Activity被杀掉，在AndroidManifest文件中对该Activity通过android:configChanges配置了对应的config信息后，该Activity就不会由于对应config信息发生变化导致它被杀掉。

我又把Activity的“重生”分为**App进程被杀掉**和**App进程没被杀掉**两种，如设置中修改字体大小、低内存等App进程会被杀掉，则这时候就是App进程被杀掉的Activity的“重生”，如横竖屏变化则这时候就是App进程没被杀掉的Activity的“重生”。

#### **为啥要设计Activity“重生”机制呢？**

Activity“重生”机制的**完全是****为了有一个更好的用户体验**。那我来举个例子：由于低内存会杀掉处于后台的App进程，如果没有此机制，再次打开该App时，App原先的状态就丢失了，这对于用户来说特别不友好，比如用户正在聊天界面与某人聊天，由于某些原因微信退到了后台，再次打开微信时，用户发现上一次的聊天界面不存在了，你说这种情况对于用户来说是不是要骂娘啊。

而有了此机制后，虽然App进程被杀掉了，但是在重新打开它时，它原先的Activity层级关系及Activity界面状态还可以与之前保持一致，这就是一个好的用户体验。

#### **需要开发者做哪些工作？**

既然有了此机制，对于开发者来说需要做哪些事情呢？其实所做的事情非常简单只需要关注Activity/View的onSaveInstanceState和onRestoreInstanceState方法即可，如下伪代码例子：

```java
//下面是某View的onSaveInstanceState方法
@Override
protected Parcelable onSaveInstanceState() {
    Parcelable superState = super.onSaveInstanceState();
    把需要保存的数据放入superState中
    return superState;
}

//下面是某View的onRestoreInstanceState方法
protected void onRestoreInstanceState(Parcelable state) {
   从state中把保存的数据取出来，并且设置给相应的属性
}

```

如上例子，要想Activity“重生”时在onRestoreInstanceState方法中还能使用到Activity被杀之前的原先数据，则需要在onSaveInstanceState方法中把相应数据保存。而不管View中的这两个方法还是Activity中的这两个方法何时调用，开发者完全不需要关心。是不是非常简单啊。

Activity“重生”用一句话总结：**Activity重新启动后，保持与被杀之前相同的状态，这里的状态指界面上的表现**。关于Activity“重生”机制的概念和好处就介绍到此。那我接下来给大家介绍下原理性的内容，也就是Activity“重生”机制是如何实现的？俗话说知其表不如知其里，只有真正掌握了事情的原理，才能对事情有更深刻的理解。

_2_

Activity如何“重生”？

大家可以思考个问题：如果想要做到Activity在重新启动后，界面内容及界面状态与Activity被杀之前一样，该如何做呢？

答案是数据，数据就是灵魂，数据就是核心，只有数据在被杀之前和重新启动之后能保持一致就可以做到A。举个例子比如微信的朋友圈界面，只要把当前用户看到了朋友圈的哪个范围，View滚动到了何处，当前界面哪些信息被展示了等这些数据记录下来，当Activity重新创建时只需要用这些数据来实例化对应的View是不是就可以做到Activity“重生”了。

这里的数据主要有Activity数据和界面数据两种。

Activity数据又可分为单个Activity数据和多个Activity数据。单个Activity数据主要指启动的Activity信息 (Activity类名、包名、在AndroidManifest中配置的Activity信息) 、Activity的启动者信息、Activity所在的Task信息等。单个Activity数据是被ActivityRecord记录的 (关于ActivityRecord介绍可以看[深度解读ActivityTaskManagerService](https://mp.weixin.qq.com/s?__biz=MzI2MjIyMjczMQ==&mid=2653047559&idx=1&sn=f9e151618cf4af32e515905b0047f5fb&token=1138630026&lang=zh_CN&scene=21#wechat_redirect)这篇文章)。只有知道了单个Activity信息，在Activity“重生”时才能知道是应该让哪个Activity“重生”。

多个Activity数据指若一个App中多个Activity被打开了，则需要记录下启动了几个Activity，以及多个Activity之间的层级关系或者先后关系。那举个例子：比如用户打开了微信首页、朋友圈、公众号详情这三个Activity，假如微信退到后台被杀掉了，则Activity“重生”时，微信首页、朋友圈、公众号详情这三个Activity的前后顺序是需要与被杀之前一致的。多个Activity数据是被Task记录的 (关于Task介绍可以看[深度解读ActivityTaskManagerService](https://mp.weixin.qq.com/s?__biz=MzI2MjIyMjczMQ==&mid=2653047559&idx=1&sn=f9e151618cf4af32e515905b0047f5fb&token=1138630026&lang=zh_CN&scene=21#wechat_redirect)这篇文章)

界面数据可以分为两种内容数据和View状态数据，内容数据比如从网络上拉取的数据或者从本地文件中读取的数据，这些数据被用来填充View，这样View就能把这些数据展示出来了。而View状态数据指View或者多个View处于某个状态或者某些状态下需要的数据，比如RecycleView滚到了哪个位置，复选框选择了哪个选项，EditText输入的内容等都属于View状态数据。当Activity“重生”时，这些数据可以帮助恢复View的原先状态。

因为数据是核心，因此为了Activity的“重生”，在重生之前是需要做一件重要的事情数据保存，只有在Activity被杀之前把数据保存下来，当Activity“重生”时，在使用之前保存的数据进行恢复操作，这个过程称为数据恢复。

数据保存是因，先有了它再去谈Activity“重生”才有意义。那就先从数据保存开始慢慢揭开Activity“重生”的面纱吧。

_3_

Activity“重生”基础--数据保存

Activity“重生”时用到的数据必须进行数据保存这一操作，否则没有数据和巧妇难为无米之炊有啥区别吗，而数据保存又根据上面提到的数据分类分为Activity数据保存和界面数据保存两种，那就从这两种来介绍数据保存操作。

## **3.1 Activity数据保存**

Activity数据的保存是ATMS服务所做的事情，单个Activity数据被保存在ActivityRecord对象中，多个Activity数据被保存在Task对象中，Task具有栈的特性，它是ActivityRecord的容器，ActivityRecord以先进后出的顺序被存储在Task中，因此Task保存了Activity之间的启动顺序关系，如下图展示了Activity、ActivityRecord、Task之间的关系：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/YicS15heicaRC71m06HDXAkTJwpibFqbdHFbfkKOFN6eJyF4tpADRP3P2dsNxAn5hV4RCufcU8z4MicXnUhvYXB83g/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

**图解**

上图中App进程处于前台，它的Activity4是处于显示状态，而App进程对应的Task中包含了ActivityRecord1、ActivityRecord2、ActivityRecord3、ActivityRecord4，它们分别对应自己的Activity。ActivityRecord1处于Task的栈底，说明它对应的Activity是最先被打开的，而ActivityRecord4处于Task的栈顶，说明它对应的Activity是最晚打开的。App1进程和launcher进程也同样有自己的Task，Task中也包含了ActivityRecord。

而一个App进程中的ActivityRecord和Task是由ATMS管理的，ATMS是位于systemserver进程，也就是Task和ActivityRecord是保存在systemserver进程的，当App进程由于低内存或者配置信息发生变化导致App进程死掉，但这时候**有一些非常重要的数据是不会随着App进程的死掉消失的，这就是App原先的Task和Task中存储的ActivityRecord**，它们可是Activity“重生”的重要条件啊，你想啊如果没有它们，那Activity“重生”时到底应该重生哪个Activity，以及Activity之间的启动顺序是啥都完全不知道了，那还谈啥Activity“重生”呢。

App进程Activity的初始化以及Activity生命周期方法的调用都是被动行为，ATMS是命令发送方，ATMS让App启动哪个Activity它就得启动哪个。因此Activity数据被保存下来，ATMS就可以依据这些信息给App发送相应命令了。

## **3.2 界面数据保存**

Activity“重生”机制定义了一套界面数据的保存规则，按**何时保存、收集数据、保存数据**这三个步骤来进行数据的保存，如下是这三个步骤的关系：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/YicS15heicaRC71m06HDXAkTJwpibFqbdHFOmeGVW2I4YhTt5ibsicTkunUumRsqqRBibgRuNQarhibaH3htAZ17Vvauw/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

**图解  
**

何时保存需要确定是否要进行保存数据的操作，如果可以保存，则进行收集数据的操作，数据收集完毕就可以把收集的数据进行保存了。那就结合该图来介绍下这三个步骤是如何配合来进行界面数据保存的。

### **3.2.1 何时保存**

界面数据的保存总得有一个时机吧，而这个时机就是**当Activity的onResume方法已经被执行了，并且用户没有点击返回键或者代码中主动调用finish方法的前提下，Activity的onStop方法被执行，就是保存数据的时机**。是不是听着这些描述有些脑壳疼啊，那我就来解释下。

首先Activity的onResume方法被执行，则代表Activity的内容可以被用户看到了，那这时候的数据是有必要保存的。大家想啊如果Activity的onResume还没执行，它的内容还没有被用户看到，那这时候的数据保存是没有意义的。

其次用户点击返回键或者代码中主动调用了finish方法，这时候肯定是没必要保存数据的，因为这些行为都是主观行为，这时候肯定是没必要保存数据的。

最后当Activity的onStop方法被执行，比如用户按了home按键、或者该Activity启动一个新的Activity，这都会导致Activity的onStop方法被执行，这时候应该把数据进行保存了，要不没可就没机会了。

我可是有铁证的啊，请看下面代码：

```java
//ActivityThread

private void callActivityOnStop(ActivityClientRecord r, boolean saveState, String reason) {
        //shouldSaveState代表是否保存数据，为true则需要保存，r.activity.mFinished为true代表用户点击了返回按钮或者调用了finish方法，r.state == null为true代表Activity的onResume被执行了
        final boolean shouldSaveState = saveState && !r.activity.mFinished && r.state == null
                && !r.isPreHoneycomb();
        final boolean isPreP = r.isPreP();
        //需要保存数据并且Android系统是P之前的版本，则需要在onStop方法之前调用onSaveInstanceState方法
        if (shouldSaveState && isPreP) {
            callActivityOnSaveInstanceState(r);
        }

        try {
            //调用onStop方法
            r.activity.performStop(r.mPreserveWindow, reason);
        }
        省略代码······

        //需要保存数据并且是Android系统是P之后的版本，则需要在onStop方法之后调用onSaveInstanceState方法
        if (shouldSaveState && !isPreP) {
            callActivityOnSaveInstanceState(r);
        }
    }
```

### **3.2.2 收集数据**

在保存数据之前需要提前做的一项工作就是收集数据，我特意绘制了一幅图展示收集数据的过程：(下图只是列了Activity、Fragment、PhoneWindow、Views收集数据的流程)

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/YicS15heicaRC71m06HDXAkTJwpibFqbdHFpK7jxGQk8ibibHpbw4AibofV8hFUhcgpfGL56Ty2DHqPNx8BVwiao6Z0gQ/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

结合上图来介绍下收集数据的流程。

**通知Activity开始收集数据**  

调用Activity的onSaveInstanceState方法就代表开始收集数据了，下面是该方法的定义：

```
//Activity
  protected void onSaveInstanceState(@NonNull Bundle outState) {
        outState.putBundle(WINDOW_HIERARCHY_TAG, mWindow.saveHierarchyState());

        Parcelable p = mFragments.saveAllState();
        if (p != null) {
            outState.putParcelable(FRAGMENTS_TAG, p);
        }
        getAutofillClientController().onSaveInstanceState(outState);
        dispatchActivitySaveInstanceState(outState);
    }
```

如上代码，其中onSaveInstanceState方法的参数outState是Bundle类型的，该参数的意思就是想要保存数据，就把保存的数据都放入outState即可。

如果自己定义的Activity想要保存数据，那就需要重写onSaveInstanceState方法，把数据放入outState对象中即可。

#### **通知PhoneWindow开始收集数据**

每一个Activity都有一个PhoneWindow对象，因此Activity也是非常高兴把收集数据的这个好消息告知PhoneWindow的，请看下面代码：

```
//Activity
protected void onSaveInstanceState(@NonNull Bundle outState) {
        //把从PhoneWindow收集的数据放入以WINDOW_HIERARCHY_TAG为key的outState中
        outState.putBundle(WINDOW_HIERARCHY_TAG, mWindow.saveHierarchyState());
        省略代码······
}

//PhoneWindow
public Bundle saveHierarchyState() {
        Bundle outState = new Bundle();
        if (mContentParent == null) {
            return outState;
        }

        SparseArray<Parcelable> states = new SparseArray<Parcelable>();
        //mContentParent为内容View的根View，下面方法开始对所有的内容View进行数据的收集
        mContentParent.saveHierarchyState(states);
        outState.putSparseParcelableArray(VIEWS_TAG, states);

        // 保存焦点View
        final View focusedView = mContentParent.findFocus();
        if (focusedView != null && focusedView.getId() != View.NO_ID) {
            outState.putInt(FOCUSED_ID_TAG, focusedView.getId());
        }

        省略其他代码······

        return outState;
    }
```

如上代码：

1.  mContentParent是所有内容View的根View，PhoneWindow会通知mContentParent开始收集数据。
    
2.  还会找到焦点View，如果焦点View不为null并且焦点View有id，则也会把焦点View收集起来。
    

#### **通知所有的View开始收集数据**

因为mContentParent是ViewGroup类型的，因此它会通知它所有的子View开始收集数据，如下相关代码：

```java
//ViewGroup
protected void dispatchSaveInstanceState(SparseArray<Parcelable> container) {
        super.dispatchSaveInstanceState(container);
        final int count = mChildrenCount;
        final View[] children = mChildren;
        //遍历它的所有子View
        for (int i = 0; i < count; i++) {
            View c = children[i];
            if ((c.mViewFlags & PARENT_SAVE_DISABLED_MASK) != PARENT_SAVE_DISABLED) {
                //调用子View的dispatchSaveInstanceState方法开始收集数据
                c.dispatchSaveInstanceState(container);
            }
        }
    }

//View
protected void dispatchSaveInstanceState(SparseArray<Parcelable> container) {
        //如果VIew存在id值，并且被允许保存数据，则进入下面逻辑
        if (mID != NO_ID && (mViewFlags & SAVE_DISABLED_MASK) == 0) {
            mPrivateFlags &= ~PFLAG_SAVE_STATE_CALLED;
            //调用onSaveInstanceState方法
            Parcelable state = onSaveInstanceState();
            if ((mPrivateFlags & PFLAG_SAVE_STATE_CALLED) == 0) {
                throw new IllegalStateException(
                        "Derived class did not call super.onSaveInstanceState()");
            }
            if (state != null) {
                // Log.i("View", "Freezing #" + Integer.toHexString(mID)
                // + ": " + state);
                container.put(mID, state);
            }
        }
    }
```

如上代码： mContentParent会通知它的所有子View开始收集数据了，子View如果想要保存数据必须存在id值，否则不可以保存，保存数据是调用View的onSaveInstanceState方法。也就是说自定义子View，如果想要保存数据，那就需要在onSaveInstanceState方法中进行保存数据的处理。

#### **通知所有的Fragment开始收集数据**

如果Activity中存在Fragment，则也会把收集数据的消息发送给它，如下相关代码：

```
//Activity
protected void onSaveInstanceState(@NonNull Bundle outState) {
        省略代码······

        Parcelable p = mFragments.saveAllState();
        if (p != null) {
            outState.putParcelable(FRAGMENTS_TAG, p);
        }
        省略代码······
    }
```

如上代码，在View或者Activity中保存数据只需要在对应的onSaveInstanceState方法中，把需要保存的数据放入Bundle对象中即可，是不是非常简单啊。

#### **小结**

收集数据的流程大致如下：

1.  Activity的onSaveInstanceState方法被调用，代表可以收集数据了。
    
2.  Activity也会通知PhoneWindow收集数据。
    
3.  PhoneWindow也会通知它所有的子View开始收集数据。
    
4.  Activity通知所有的Fragments收集数据。
    

自定义的Activity如果想要保存数据的话，需要重写onSaveInstanceState方法。自定义的子View如果想要保存数据的话，需要有id值并且需要重写onSaveInstanceState方法。

### **3.2.3 保存数据**

数据收集完毕了，剩下的最后一步就是保存数据，那数据保存在何地呢？

那我就来分析下，因为Activity“重生”会存在App进程被杀掉的情况，因此针对此情况数据肯定不能保存在App进程内，那只能保存在systemserver进程了。对的没错，收集到的数据会通过binder通信来到systemserver进程，并且保存在Activity对应的ActivityRecord对象中，如下是相关的代码，请自行取阅：

```
//ActivityRecord

//mIcicle就是App端收集上来的数据，它不会持久化到文件中
private Bundle mIcicle;         // last saved activity state
//mPersistentState也是App端收集上来的数据，只不过该数据会持久化到文件中
private PersistableBundle mPersistentState; // last persistently saved activity state
```

### **3.2.4 小结**

界面数据的保存分为何时保存、收集数据、保存数据这三个步骤：

1.  何时保存规定了只有在Activity的onResume方法被执行并且没有主动调用Activity的finish方法的条件下，Activity的onStop方法被执行之前或者之后可以保存数据。
    
2.  收集数据则规定了从Activity到PhoneWindow，PhoneWindow到所有的子View，收集需要保存数据的一个流程，所有收集的数据放入Bundle对象中。
    
3.  保存数据则会把收集的数据通过binder通信放入Activity对应的ActivityRecord对象中，这样即使App进程死掉了，保存的数据都还依然存在。因为是binder通信传递数据，因此对于保存的数据大小是由限制的，不能保存非常大的数据。
    

有了数据保存作为基础，那就跟随我来揭开Activity“重生”的神秘面纱吧。

_4_

Activity“重生”揭秘

Activity的“重生”可以分为Activity实例创建和界面数据恢复两部分，Activity实例创建是必须先进行的，只有有了Activity实例，才能对Activity实例中PhoneWindow、各种View进行数据恢复，那就先从Activity实例创建谈起。

## **4.1 Activity实例创建**

因为Activity“重生”分为App进程被杀和App进程不被杀两种情况。因此Activity实例创建也分这两种情况，那就从这两种情况来介绍Activity实例创建吧。

### **4.1.1 App进程不被杀**

对于App进程不被杀，Activity实例创建是非常简单的，比如横竖屏切换，ATMS会给App进程中的相应Activity发送relaunch消息，同时会把保存在ActivityRecord中的界面数据也传递过来。对于relaunch消息，当前App进程会把对应Activity先杀掉，杀掉后再次重新创建该Activity的实例。

### **4.1.2 App进程被杀**

而对于App进程被杀这种情况，Activity实例创建就是一个复杂过程了，那就听我细细道来。还记得ActivityRecord和Task吧，请先看下图：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/YicS15heicaRC71m06HDXAkTJwpibFqbdHF7ibXibpKY3ovF1M9OvycYuNznJ5pHbjBbPQnbDRFbsc60RQP1YwFe4SA/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

##### **图解**

左边图存在App2、App1、launcher三个进程，launcher进程位于前台，它的Activity1处于显示状态，每个进程都对应自己的Task，而这些Task存放在TaskDisplayArea。由于App进程2处于后台，现在假设由于低内存导致App2进程被杀掉。

右边图展示了App2进程被杀掉后，原先App2进程对应的Task还依然存在，Task及Task中保存的ActivityRecord就是Activity“重生”的关键。

再来看一幅图：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/YicS15heicaRC71m06HDXAkTJwpibFqbdHFF0VsiaKNIgFsSygmfCYQSxY2m5l5wzc5w1vNkGAjCSqYnLibr3938LiaA/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

##### **图解**

左边图展示了一个App进程没有启动的条件下，从桌面打开该App的主Activity的流程。右边图展示了该App进程退到后台后，由于低内存导致它被杀掉，这时候用户又再次从桌面打开该App的流程，也是Activity“重生”的流程。

上图中，黄色背景边框代表启动主Activity和Activity“重生”的区别，左边图第3步是记录Activity信息，而右边图第3步是找到Task，也就是对于Activity“重生”来说，是不会创建Task和ActivityRecord的，因为原先App进程的Task和Task中的ActivityRecord依然被保存着，因此可以直接使用。

上图中，左边图和右边图第7步都是发送Activity启动信息，但是它们发送的Activity启动信息有可能是不一样的。对于左边图来说发送的是主Activity信息；而右边就不确定了，原因是需要根据Task中栈顶的ActivityRecord是啥，如果是主Activity，则发送它的启动信息；否则发送别的Activity的启动信息。

结合上面两幅图，可以得出App进程由于低内存等原因被杀，由于ATMS依然保存了该App的Task，因此在App重新启动时，会依然使用ATMS中保存的Task，由于Task已经把被杀之前的启动了几个Activity，以及Activity的启动先后顺序是啥都保存了，这样就可以做到App重新启动时，创建的依然是被杀之前的Activity的实例。

## **4.2 界面数据恢复**

ATMS会发送Activity启动信息给App进程，其中的启动信息会包含保存的界面数据，它已经被提前保存在了ActivityRecord的属性中。既然Activity的实例已经创建了，那接下来要做的事情就是使用保存的界面数据开始进行界面数据恢复操作。

我特意绘制了一幅图，展示Activity、PhoneWindow、Views进行数据恢复的操作流程：

![图片](https://mmbiz.qpic.cn/sz_mmbiz_png/YicS15heicaRC71m06HDXAkTJwpibFqbdHFWUxykVtGZEiccp4Pk3kAgqg1sib3V8c75vCjF2T8dickdthHsWUhrXd6Q/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

#### **通知Activity进行数据恢复操作**

会调用Activity的onRestoreInstanceState方法，通知它开始进行数据恢复操作，而数据放在Bundle对象中，如下是相关代码：

```
//Activity
protected void onRestoreInstanceState(@NonNull Bundle savedInstanceState) {
        if (mWindow != null) {
            //根据WINDOW_HIERARCHY_TAG取出windowState数据
            Bundle windowState = savedInstanceState.getBundle(WINDOW_HIERARCHY_TAG);
            if (windowState != null) {
                //调用restoreHierarchyState方法开始进行恢复操作
                mWindow.restoreHierarchyState(windowState);
            }
        }
    }
```

因此自定义的Activity如果想进行数据恢复操作，就需要重写onRestoreInstanceState方法。

#### **通知PhoneWindow进行数据恢复操作**

如下相关代码：

```
//PhoneWindow

public void restoreHierarchyState(Bundle savedInstanceState) {
        if (mContentParent == null) {
            return;
        }

        SparseArray<Parcelable> savedStates
                = savedInstanceState.getSparseParcelableArray(VIEWS_TAG);
        if (savedStates != null) {
            //不为null，则通知mContentParent进行数据恢复操作
            mContentParent.restoreHierarchyState(savedStates);
        }

        // restore the focused view
        int focusedViewId = savedInstanceState.getInt(FOCUSED_ID_TAG, View.NO_ID);
        //找到保存的焦点view 的id
        if (focusedViewId != View.NO_ID) {
            View needsFocus = mContentParent.findViewById(focusedViewId);
            if (needsFocus != null) {
                needsFocus.requestFocus();
            } else {
                Log.w(TAG,
                        "Previously focused view reported id " + focusedViewId
                                + " during save, but can't be found during restore.");
            }
        }

        省略代码······
    }
```

#### **通知所有的Views进行数据恢复操作**

因为mContentParent是所有内容View的顶级父View，并且它是ViewGroup类型的，PhoneWindow会调用mContentParent的restoreHierarchyState方法通知它进行数据恢复操作，如下相关代码：

```
//View
public void restoreHierarchyState(SparseArray<Parcelable> container) {
        dispatchRestoreInstanceState(container);
    }

//ViewGroup
protected void dispatchRestoreInstanceState(SparseArray<Parcelable> container) {
        super.dispatchRestoreInstanceState(container);
        final int count = mChildrenCount;
        final View[] children = mChildren;
        //遍历它所有的子View
        for (int i = 0; i < count; i++) {
            View c = children[i];
            if ((c.mViewFlags & PARENT_SAVE_DISABLED_MASK) != PARENT_SAVE_DISABLED) {
                //调用View的dispatchRestoreInstanceState方法
                c.dispatchRestoreInstanceState(container);
            }
        }
    }

//View
protected void dispatchRestoreInstanceState(SparseArray<Parcelable> container) {
        //要想进行数据恢复操作，mID必须存在
        if (mID != NO_ID) {
            Parcelable state = container.get(mID);
            if (state != null) {
                // Log.i("View", "Restoreing #" + Integer.toHexString(mID)
                // + ": " + state);
                mPrivateFlags &= ~PFLAG_SAVE_STATE_CALLED;
                //调用onRestoreInstanceState方法
                onRestoreInstanceState(state);
                if ((mPrivateFlags & PFLAG_SAVE_STATE_CALLED) == 0) {
                    throw new IllegalStateException(
                            "Derived class did not call super.onRestoreInstanceState()");
                }
            }
        }
    }
```

因此自定义View如果想要进行数据恢复操作就需要重写它的onRestoreInstanceState方法。

#### **小结**

界面数据恢复操作与界面数据保存操作是成对出现的并且也是完全相反的操作，先有界面数据保存才有界面数据恢复。而不管是**界面数据恢复操作，还是界面数据保存操作，开发者都不需要关心何时被调用，只需要实现Activity/View的onRestoreInstanceState和onSaveInstanceState方法即可**。

_5_

总结

Activity的“重生”由于低内存或者配置信息发生变化，导致Activity重新启动。Activity“重生”机制的作用是为了提供一个好的用户体验，当由于低内存等原因App进程被杀掉时，App进程再次重新启动时还依然保持被杀之前的App状态。

数据是Activity“重生”的灵魂，而数据又分为Activity数据和界面数据，要想Activity“重生”，需要在合适的时机把数据保存起来，Activity相关的数据是被ATMS保存在ActivityRecord和Task中，而界面数据最终也是被ATMS保存在ActivityRecord的属性中。

Activity“重生”时，就需要利用之前保存的数据，由于低内存等原因杀掉了App进程，但是App进程对应的Task数据还依然存在，Task中保存了启动几个ActivityRecord，以及ActivityRecord之间启动的前后顺序。在App进程重新启动时，还会依然复用原先的Task，因为Task中保存了App进程被杀之前的Activity的状态，因此Activity“重生”后就可以做到与被杀之前相同的App状态。

自定义Activity/View，要想在Activity“重生”时依然保存原先的状态，重写它们的onRestoreInstanceState和onSaveInstanceState方法即可，开发者完全不需要关心这些方法何时被调用，只需要安心的把自己的业务逻辑加进去即可。

___

最后推荐一下我做的网站，玩Android: _wanandroid.com_ ，包含详尽的知识体系、好用的工具，还有本公众号文章合集，欢迎体验和收藏！

![图片](https://mmbiz.qpic.cn/mmbiz_jpg/MOu2ZNAwZwP4yDt9RiaN89t9lxTz0vZWZy9sYR54YefTFFBPmPLwnAN9PNicI0rZznIYt4r2Q40DbAAiatTS1MlVw/640?wx_fmt=jpeg&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

**扫一扫** 关注我的公众号

如果你想要跟大家分享你的文章，欢迎投稿~

┏(＾0＾)┛明天见！