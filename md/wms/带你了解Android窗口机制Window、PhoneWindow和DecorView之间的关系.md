![](http://pic.rmb.bdstatic.com/bjh/news/ca054e533844dc927c24e57c1713f3ad.gif)

作者 | Android开发编程 责编 | 欧阳姝黎

> 在Android框架中，每个应用界面，都有一个应用级的window。
> 
> 常用的activity、dialog、Toast等都是通过通过创建window、PhoneWindow来实现，所以其实window我们一直都见到，只是不知道那就是window。
> 
> 了解window的机制原理，可以更好地了解window，进而更好地了解android是怎么管理屏幕上的view。
> 
> 这样，当我们需要使用dialog或者popupWindow的时候，可以懂得他背后究竟做了什么，才能够更好的运用dialog、popupWindow等。

![](https://pics3.baidu.com/feed/a9d3fd1f4134970a9cdbeebe17266dc0a5865de8.png@f_auto?token=2652bc21ad1c54634d1982bb85cf2b63)

**window、phonewindow、DecorView 关系**

先看图

![](https://pics0.baidu.com/feed/aa18972bd40735fa1d10dcb91abdb3bb0d2408c7.jpeg@f_auto?token=9d9b5ec43bb113c1f29811ac5e23625e)

**1、每一个 Activity 都持有一个 Window 对象，**  

```
public class Activity extends ContextThemeWrappe{
```

但是 Window 是一个抽象类，这里 Android 为 Window 提供了唯一的实现类 PhoneWindow。也就是说 Activity 中的 window 实例就是一个 PhoneWindow 对象。

**2、但是 PhoneWindow 终究是 Window，它并不具备多少 View 相关的能力。**不过 PhoneWindow 中持有一个 Android 中非常重要的一个 View 对象 Decor(装饰)View，它在 PhoneWindow 中的定义如下：

```
public class PhoneWindow extends Window{
```

**3、查看 DecorView 继承关系得知，DecorView 继承自 FrameLayout**

```
public class DecorView extends FrameLayout {
```

现在的关系就很明确了，每一个 Activity 持有一个 PhoneWindow 的对象，而一个 PhoneWindow 对象持有一个 DecorView 的实例，所以 Activity 中 View 相关的操作其实大都是通过 DecorView 来完成。

![](https://pics7.baidu.com/feed/bd315c6034a85edf7d71a4f6cbb8b52bdf54755f.png@f_auto?token=7e0539418d2f9f50ef2951b76de7b647)

**Window**

> Android手机中所有的视图都是通过Window来呈现的，像常用的Activity，Dialog，PopupWindow，Toast，他们的视图都是附加在Window上的，所以可以这么说 ——Window是View的直接管理者。

源代码如下  

![](https://pics0.baidu.com/feed/e824b899a9014c087f54d5c98397be007af4f460.jpeg@f_auto?token=9afd57f9ece71a5d63d2cfb07c743336)

```
public abstract class Window {
```

**1、Window的type属性**

> window是有分类的，不同类别的显示高度范围不同。window也是一样按照高度范围进行分类，他也有一个变量Z-Order，决定了window的高度，Z-Order越大，window越靠近用户，也就显示越高，高度高的window会覆盖高度低的window。window一共可分为三类：
> 
> -   应用程序窗口：应用程序窗口一般位于最底层，Z-Order在1-99
>     
> -   子窗口：子窗口一般是显示在应用窗口之上，Z-Order在1000-1999
>     
> -   系统级窗口：系统级窗口一般位于最顶层，不会被其他的window遮住，如Toast，Z-Order在2000-2999。如果要弹出自定义系统级窗口需要动态申请权限。
>     

Window的flags参数

```
// 当 Window 可见时允许锁屏
```

**2、window的其他属性**

-   x与y属性：指定window的位置
    
-   alpha：window的透明度
    
-   gravity：window在屏幕中的位置，使用的是Gravity类的常量
    
-   format：window的像素点格式，值定义在PixelFormat中
    

window属性赋值

```
WindowManager.LayoutParams windowParams = new WindowManager.LayoutParams();
```

![](https://pics5.baidu.com/feed/503d269759ee3d6d434f5274c1fad12a4d4adefd.png@f_auto?token=43736e0c62a4754fde6b7675d939d9c3)

**PhoneWindow**

> 继承于Window类，是Window类的具体实现，即我们可以通过该类具体去绘制窗口。并且，该类内部包含了一个DecorView对象，该DectorView对象是所有应用窗口(Activity界面)的根View。
> 
> 简而言之，PhoneWindow类是把一个FrameLayout类即DecorView对象进行一定的包装，将它作为应用窗口的根View，并提供一组通用的窗口操作接口。它是Android中的最基本的窗口系统，每个Activity 均会创建一个PhoneWindow对象，是Activity和整个View系统交互的接口。

![](https://pics2.baidu.com/feed/f2deb48f8c5494eedb7111e3af195cf69b257ee9.jpeg@f_auto?token=bfe0b4f8a449904d5717cf78e5acdd92)

```
public class PhoneWindow extends Window implements MenuBuilder.Callback {
```

它是Android中的最基本的窗口系统，每个Activity 均会创建一个PhoneWindow对象，是Activity和整个View系统交互的接口。

![](https://pics2.baidu.com/feed/21a4462309f79052de34b2ca771f6bc27acbd514.png@f_auto?token=8c4d4ee679bfc47750eae1a97c04942c)

**DecorView**

> 作为顶级View,DecorView一般情况下它内部会包含一个竖直方向的LinearLayout，上面的标题栏(titleBar)，下面是内容栏。
> 
> 通常我们在Activity中通过setContentView所设置的布局文件就是被加载到id为android.R.id.content的内容栏里(FrameLayout)

![](https://pics0.baidu.com/feed/6c224f4a20a4462363f309e3e4cece060df3d76f.jpeg@f_auto?token=604629cdb808becef2e04666da937790)

```
/** @hide */
```

DecorView它主要有以下功能总结：

-   作为顶级View,DecorView一般情况下它内部会包含一个竖直方向的LinearLayout，上面的标题栏(titleBar)，下面是内容栏。通常我们在Activity中通过setContentView所设置的布局文件就是被加载到id为android.R.id.content的内容栏里(FrameLayout)；
    
-   Dispatch ViewRoot分发来的key、touch、trackball等外部事件；
    
-   DecorView有一个直接的子View，我们称之为SystemLayout,这个View是从系统的Layout.xml中解析出的，它包含当前UI的风格，如是否带title、是否带processbar等。可以称这些属性为Window decorations；
    
-   作为PhoneWindow与ViewRoot之间的桥梁，ViewRoot通过DecorView设置窗口属性。//可以这样获取 View = getWindow().getDecorView();
    
-   DecorView只有一个子元素为LinearLayout。代表整个Window界面，包含通知栏，标题栏，内容显示栏三块区域。DecorView里面TitleView：标题，可以设置requestWindowFeature(Window.FEATURE\_NO\_TITLE)取消掉ContentView：是一个id为content的FrameLayout。我们平常在Activity使用的setContentView就是设置在这里，也就是在FrameLayout上；
    
-   每一个Activity都包含一个Window对象(dialog，toast 等也是新添加的window对象)，而Window是一个抽象类，具体实现是PhoneWindow。在Activity中的setContentView实际上是调用PhoneWindow的setContentView方法。并且PhoneWindow中包含着成员变量DecorView。
    

![](https://pics2.baidu.com/feed/4ec2d5628535e5dde4cddc160b2a1be7cc1b620e.jpeg@f_auto?token=f64a2defcdddd20ee486d08aa5e51b1a)

**总结：**  

Window类相当于一幅画 ，PhoneWindow为一副山水画(具体概念，我们知道了是谁的、什么性质的画)，DecorView则为该山水画的具体内容。DecorView呈现在PhoneWindow上。

![](http://pic.rmb.bdstatic.com/bjh/news/729af9e75dc16e618f058644183a7d79.gif)