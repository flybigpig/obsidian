今天跟大家介绍一个Android 15系统中的新特性，预测性返回手势。

说实话，这个特性真的难称得上是新特性了，因为它最早在Android 13系统中就已经被引入了。

但是Android 13并没有默认启用它，Android 14也没有默认启用它，直到最新的Android 15，预测性返回手势这个功能终于被默认启用了。

那么今天，我就来跟大家介绍一下这个不太为人所熟知的新特性吧。

## 什么是预测性返回手势？

首先要解决的第一个问题就是，什么是预测性返回手势？

简单来说，就是在你想要触发返回操作时，让你能够提前预知即将返回到哪个界面。

在Android系统中，用户是可以通过点击返回键，或使用返回手势来回到上一个界面的。但上一个界面具体是什么？我们可能并不知道，通常得等点完了返回键之后才能知道具体会返回到哪个界面。

那么预测性返回手势是怎么优化这部分体验的呢？貌似我们也基本没怎么见过这种能提前预知即将返回到哪个界面的效果吧。

正如前面所说，这是因为Android 13和Android 14系统都没有默认启用这个功能。它被藏得很深，以至于除了专业的[Android开发](https://so.csdn.net/so/search?q=Android%E5%BC%80%E5%8F%91&spm=1001.2101.3001.7020)者，普通用户几乎是不可能体验到这个功能的。

下面我们就来学习一下如何将这个功能打开。

首先第一步，如果你的设备是Android 15系统的话，则可以跳过这一步，因为预测性返回手势默认已经启用了。而如果你的设备是Android 13或14系统的话，需要在开发者模式当中将“预见式返回动画”这个选项打开，如下图所示。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/c20679e3bcb444a3a8cd5e58f0121543.png#pic_center)

然后第二步，确保你的手机当前使用的导航模式是手势导航，因为预测性返回手势这个功能只在手势导航模式下才能工作，传统的3按钮导航模式是无法体验这个功能的，如下图所示。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/8439e3769f2540869846206d03ad2fbc.png#pic_center)

最后，即使你完成了前面两步，也不代表就能成功体验到预测性返回手势功能，因为这个功能还需要每个App去进行开发适配才行。

具体如何适配我会在接下来的内容中进行介绍，不过由于这个功能已经迭代了3个Android系统版本了，所以目前已经有很多App都完成了适配，因此多尝试几个App你应该就能触发出预测性返回手势的效果了。

下面我们来看下具体预测性返回手势的效果是什么样子的。

首先传统的手势返回操作是这个样子的，当你的手指在屏幕左右两侧拖动时，会触发一个系统的返回键头，松开手指时，触发返回事件，如下图所示。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/6a2a0c0afe3242b8bd5d1c797531e80c.gif#pic_center)

可以看出，在我们松开手指之前，是无法知道即将返回到哪个界面的。

而启用了预测性返回手势功能，返回事件的效果就变成了这个样子。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/bf663876bf6848af9c0419fff634c501.gif#pic_center)

也就是说，我们现在可以有一个预览界面，能够提前知道即将返回到哪个界面。

那么有的小伙伴这里就产生疑惑了，为什么我需要提前知道即将返回的是哪个界面呢？

按照Google的说法，过去的这种返回模式会存在比较高的误操作率，即返回了之后才发现这并不是我想要去的界面。而预测性返回手势则可以大大降低这种误操作率。

但是我个人对这种误操作的感受并不怎么深，好像在我使用Android手机的时候并没有怎么受到这种返回误操作的困扰。

而我认为，预测性返回手势最[主要的](https://so.csdn.net/so/search?q=%E4%B8%BB%E8%A6%81%E7%9A%84&spm=1001.2101.3001.7020)一个用处就是可以提升用户体验。

可能有的朋友会觉得，只是能够提前知晓即将返回到哪个界面而已，怎么就能提升用户体验了？

这里我来给大家举一个例子，相信大家就能立刻明白了。

现在我们先把返回键放在一旁，我们先来想一想Home键的使用体验是什么样子的。不管是Android还是iOS，在这部分的体验上相信大家都是非常熟悉的，即手指在屏幕的底部轻轻向上滑动，就可以触发Home键，也就是回到桌面的操作了，如下图所示。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/d7f39fe4363947e88aa8e1f200742fc9.gif#pic_center)

这个效果大家肯定是再熟悉不过了，我敢说几乎每个人每天都在使用着。

而如果某天，Android系统将回到桌面的效果改成下图这个样子，你还能接受吗？

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/d61b91e1346346d3a0f80a25b62b23eb.gif#pic_center)

这个我相信连想都不用想，不可能有那个用户愿意接受这种回到桌面效果的退化。

由此也就证明了，能够提前预览即将返回到的界面，也是可以提升用户体验的。

## 为什么预测性返回手势没有默认启用？

既然预测性返回手势可以提升用户体验，同时按照Google的说法，还可以降低返回事件的误操作率，那么这么好的功能，为什么却一直没有默认启用呢？

刚才已经说了，预测性返回手势功能最早是在Android 13系统中引入的，当时普遍预测Google会在Android 14系统中默认启用这个功能。

然而实际情况是，Google直到Android 15系统发布才正式启用了预测性返回手势这个功能。

一个功能居然经历了3年的系统版本迭代才正式推出，以至于一开始还有许多用户和开发者非常期待这个功能，到后面都逐渐忘却这个功能了。

那么到底是什么原因让预测性返回手势这个功能这么难产呢？

这里我用一个非常简单的例子来跟大家解释一下，观察下面这张图。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/fcebc22bc0ba4452872b9e47f4845b26.png#pic_center)

请问大家，当我的手指从屏幕底部向上滑动时，我们即将前往哪个界面？

答案很明显，那自然是前往手机的桌面了。

那么再观察下面这张图。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/9960fabf10af41aa8012889db8f5e902.png#pic_center)

请问大家，当我的手指从屏幕的两侧向内滑动时，我们即将前面哪个界面？

答案是，没有人能知道。因为每个App的返回逻辑是完全可以由这些App自定义的。

也正是由于这个原因，导致预测性返回手势这个功能拖了非常久才得以正式上线，因为预测性返回手势是需要第三方App适配才能完成的功能。

## 让App支持预测性返回手势

现在我们已经从概念上对预测性返回手势有了比较全面的理解，接下来要学习的部分自然就是如何让我们的App能够支持预测性返回手势。

这个其实非常简单，只需要一行代码就可以实现了，那就是在AndroidManifest.xml文件中将enableOnBackInvokedCallback这个属性设置为true，如下图所示。

```
    <application
        ...
        android:enableOnBackInvokedCallback="true">
        ...
    </application>
```

这里我们可以尝试一下，新建一个空的项目，然后将enableOnBackInvokedCallback属性设置为true，运行程序即可。

注意记得要先将你的手机设置成手势导航模式，另外如果你是运行在Android 13或14设备上，还需要在开发者模式当中将“预见式返回动画”启用，如果是Android 15设备则可以跳过这一步。

现在观察我们刚开发的这个程序的返回功能，如下图所示。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/b4a1fedae5064007acf2620b9b5c1c64.gif#pic_center)

可以看到，只需要一行代码，我们就成功启用了预测性返回手势的效果了。

不过这个例子之所以能这么顺利是因为它非常的简单，没有App内对返回事件进行处理的需求。我们再来观察下面这个例子。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/ba9fc5f9d9254d3790b4d31de7dca083.gif#pic_center)

这个例子就可以明显看出问题来了。

初始状态下我们正在浏览一个新闻列表界面，当我对某个新闻感兴趣的时候，点击该新闻可以阅读该新闻的具体内容。

而当我完成阅读想要返回时，你会发现我无法返回到上一级新闻列表界面，而是直接触发了预测性返回手势效果回到桌面了。

这种情况就代表着我们需要进行适配了。

要想对返回事件进行处理，大家应该都知道需要去重写Activity的onBackPressed()函数。但是如果你去查看一下onBackPressed()函数的最新文档，会发现它在Android 13的时候就已经被废弃了。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/934b71777a9448f28a33ba95876fa098.png#pic_center)

废弃的原因其实就是因为从Android 13开始有了预测性返回手势，所以Google推荐我们使用OnBackInvokedCallback或androidx.activity.OnBackPressedCallback这两个新的API来处理返回事件，因为这两个API能够自动完成预测性返回手势的适配。

那么为什么是有两个API来处理返回事件呢？

因为，OnBackInvokedCallback是随着Android 13系统发布的API，这意味着我们只能在Android 13及以上系统使用它，那么在Android 12及以下系统就还要使用以前的onBackPressed()函数来处理返回事件。保持着两套返回事件的处理逻辑，会让我们的代码变得更加臃肿和难以维护。

于是Google在AndroidX库中又推出了OnBackPressedCallback这个API，它保持了和OnBackInvokedCallback一模一样的用法，同时又在所有Android系统版本中都可以使用。

所以建议大家直接使用OnBackPressedCallback这个API来处理返回事件就可以了。

下面我们来学习一下OnBackPressedCallback具体如何使用。

```
val callback = onBackPressedDispatcher.addCallback(this) {
    // 在此处理返回事件
}

// 控件是否拦截拦截返回事件
callback.isEnabled = shouldEnableOrDisable
```

它的用法主要就是两部分。

第一，在Activity中可以调用onBackPressedDispatcher.addCallback()来启用返回事件拦截。只要调用这行代码之后，系统的返回事件就会回调到我们添加的Callback中去处理。

第二，可以调用callback.isEnabled来控制Callback的启用状态，如果设置成false则表示我们添加的Callback不启用，那就还是会走系统默认的返回逻辑。

了解了这些内容之后，现在我们就可以来解决刚才预测性返回手势的适配问题了，代码如下。

```
class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ...
        val callback = onBackPressedDispatcher.addCallback(this) {
            webview.goBack()
        }
        webview.webViewClient = object : WebViewClient() {
            override fun doUpdateVisitedHistory(view: WebView, url: String, isReload: Boolean) {
                callback.isEnabled = webview.canGoBack()
            }
        }
    }
}
```

我们通过重写WebViewClient的doUpdateVisitedHistory()函数来监听WebView的前进和后退事件。

当发现WebView无法再后退的时候，说明它已经回到最初始的新闻列表界面了。此时将Callback设置为禁用，这样返回事件就会交还给系统去处理。

现在重新运行一下程序，效果就正常了。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/c3b580d7a2ad4baba8de93cd3500e248.gif#pic_center)

## Android 14&15的预测性返回手势

预测性返回手势最早在Android 13中被引入的时候，功能还比较单一，只有最基本的提前预览返回桌面的效果。

后来Google在Android 14中为预测性返回手势加入了更加丰富的功能，这些功能在Android 14及以上的系统都是支持的，下面我们就来学习一下。

首先，Android 13的预测性返回手势只支持即将返回桌面时的预览效果，我们的App内部如果有多个Activity，它们之间的返回是不会有预览效果的，如下图所示。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/ef8b027b2384473abc4c412ccfeb6ca6.gif#pic_center)

而Android 14在这方面则做了进一步的扩展，即使是App内部的Activity之间的返回，也会有返回预览的效果。这个功能在Android 14及以上系统中是自动开启的，效果如下图所示。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/afa4900d9a6d42f1943654ed93baecf4.gif#pic_center)

当然，或许这并不是你想要的效果。也许你就是想要Android 13系统那样，只在即将返回桌面时才支持返回预览效果。

不用担心，Android对此也是支持的。因为从Android 14开始，我们可以精确到Activity级别来控制是否启用预测性返回手势，示例代码如下。

```
<manifest ...>
    <application ...
        android:enableOnBackInvokedCallback="true">
        <activity
            android:name=".MainActivity"
            ...>
        </activity>
        <activity
            android:name=".SecondActivity"
            android:enableOnBackInvokedCallback="false"
            ...>
        </activity>
    </application>
</manifest>
```

可以看到，这里在标签上将enableOnBackInvokedCallback设置为true，表示在全局范围启用了预测性返回手势。但是我们仍然可以针对具体某一个Activity，比如说这里的SecondActivity，将它的enableOnBackInvokedCallback设置成false，这样就可以在SecondActivity触发返回操作时，禁用提前返回预览的功能。

除此之外，Android 14还允许我们对Activity之间的返回预览动画效果进行自定义。

自定义Activity之间的跳转动画相信大家都有用过，就是调用Activity的overridePendingTransition()函数，然后传入enter和exit动画即可。

但是如果你查看一下最新的Android文档，你会发现这个函数在Android 14中也被废弃了。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/1f3c076885d74666aca9cdbab153efbf.png#pic_center)

废弃的原因不用多说，自然也是因为预测性返回手势。

现在Android 14及以上系统推荐使用overrideActivityTransition()函数来自定义Activity之间的跳转动画，因为这个函数可以自动适配预测性返回手势功能。

下面我来定义一个Activity横向滑入滑出的动画效果，分别对应4个动画文件。

```
slide_in_left.xml
<set xmlns:android="http://schemas.android.com/apk/res/android">
<translate
    android:duration="1000"
    android:fromXDelta="-100%p"
    android:toXDelta="0" />
</set>

slide_in_right.xml
<set xmlns:android="http://schemas.android.com/apk/res/android">
<translate
    android:duration="1000"
    android:fromXDelta="100%p"
    android:toXDelta="0" />
</set>

slide_out_left.xml
<set xmlns:android="http://schemas.android.com/apk/res/android">
<translate
    android:duration="1000"
    android:fromXDelta="0"
    android:toXDelta="-100%p" />
</set>

slide_out_right.xml
<set xmlns:android="http://schemas.android.com/apk/res/android">
<translate
    android:duration="1000"
    android:fromXDelta="0"
    android:toXDelta="100%p" />
</set>
```

接下来在MainActivity和SecondActivity中分别加入如下代码。

```
class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ...
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            overrideActivityTransition(OVERRIDE_TRANSITION_OPEN, R.anim.slide_in_right, R.anim.slide_out_left)
        }
    }
}

class SecondActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ...
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            overrideActivityTransition(OVERRIDE_TRANSITION_CLOSE, R.anim.slide_in_left, R.anim.slide_out_right)
        }
    }
}
```

然后重新运行程序，效果如下图所示。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/9c72582a69974782a4cfd27bbe34353e.gif#pic_center)

由于增加了Activity之间的跳转动画，你会发现从MainActivity跳转到SecondActivity现在就有了横向滑动动画的效果。

但更加令人惊喜的是，你会发现当我们进行返回操作时，系统会根据我们刚才自定义的动画效果，自动适配到预测性返回手势上面。

有了这套机制，其实讲道理我们就可以实现任意类型的预测性返回手势动画效果了，比如这个是Android官网上给出的一个非常炫酷的返回动画效果图。

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/5d404af9c1aa464db0370fc92c69b0fb.gif#pic_center)

当然这个是在同一个Activity中触发返回事件的动画效果，和我们上面举的例子又不太一样。不过放心，在Android 14及以上系统，这些功能都是被支持的。

想要学习上图效果具体如何实现，以及预测性返回手势所支持的更多动画类型，都可以去参考Android官网的文档，链接如下：

[https://developer.android.com/guide/navigation/custom-back/support-animations](https://developer.android.com/guide/navigation/custom-back/support-animations)

我们本篇文章的篇幅毕竟有限，上面链接里的内容这里就不再展开了。

那么我们下篇原创文章再见。

___

如果想要学习Kotlin和最新的Android知识，可以参考我写的书 **《第一行代码 第3版》**，[点击此处查看详情](https://guolin.blog.csdn.net/article/details/105233078)。