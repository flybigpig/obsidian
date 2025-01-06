# 一、前言

android的WindowManagerService（简称wms）是系统框架一个非常庞大复杂的一个系统模块，它主要由三大块组成：wms数据结构，wms大遍历，wms的窗口动画

  

![](https://upload-images.jianshu.io/upload_images/26874665-b9e5e869c918f60c.png?imageMogr2/auto-orient/strip|imageView2/2/w/720/format/webp)

wms总体图.png

wms数据结构就是wms的所有WindowState（继承windowcontainer）集合的数据结构，比如有ActivityRecord（包含1个或者多个WindowState），比如有WindowState，其中ActivityRecord具体表现实例就是Activity，WindowState具体表现实例有状态栏、导航键、输入法等。

  

![](https://upload-images.jianshu.io/upload_images/26874665-f02b4fd2a63d403c.png?imageMogr2/auto-orient/strip|imageView2/2/w/746/format/webp)

window.png

wms大遍历（performSurfacePlacement）就是对当前所有存在的window进行窗口大小计算和窗口绘制状态更新，最后把窗口Surface更新到surfaceflinger。

wms的窗口动画是其中一个比较重要的子功能，wms的窗口动画负责窗口间的切换动画的实现。

接下来我们从android动画原理开始来逐步介绍wms的窗口动画

# 二、android动画的一个demo

android动画主要有三种类型：view的动画、window的动画、画布对象的动画（ondraw里面的画图api）  
首先我们来看一个android动画的简单实现的demo

```java
Choreographer mChoreographer = Choreographer.getInstance();
Animation mAnimation = null;

public void start(Animation anim) {
    mAnimation = anim;
    scheduleAnimation();
}

private void scheduleAnimation() {
    mChoreographer.postFrameCallback(Choreographer.CALLBACK_ANIMTION, mUpdateRunnable, null);
}

private Runnable mUpdateRunnable = new Runnable() {
    @Override
    public void run() {
        if (mAnimation != null) {
            long time = SystemClock.uptimeMillis();
            Transformation transform = new Transformation();
            //根据当前time计算transform
            boolean more = mAnimation.getTransformation(time, transform);
            
            //根据transform进行渲染，改变view的属性（大小、位置、透明度等）？改变窗口的属性（大小、位置、透明度等）？
            PERFORM_RENDER_WITH_TRANSFORMATION(transform)；
            
            //通过time的计算可以计算出动画是否继续还是结束
            if (more) {
                scheduleAnimation();
            } else {
                mAnimation = null;
            }
        }
    }
};
```

从这个例子可以看出android的动画就是借用Choreographer来通过vsync原理逐帧控制动画的播放（需要对Choreographer有一定的了解），中间update变量transform包含了动画的基本元素：Matrix、透明度，然后根据这两个元素对显示对象（view或者画布对象或者window？）进行当前时间的绘制，逐帧显示，最终用户看到的就是一个动画，从systrace可以看到

  

![](https://upload-images.jianshu.io/upload_images/26874665-cb7ed6a90f6a1ea7.png?imageMogr2/auto-orient/strip|imageView2/2/w/919/format/webp)

动画systrace.png

ValueAnimator属性动画的实现原理也是类似于这个demo的实现

# 三、WindowManagerService窗口动画机制

android的WindowManagerService窗口动画机制一直在优化进步，主要体现在：  
1、在androidP以前的版本，主要是通过WindowAnimator主动画类中的mChoreographer来通过vsync原理逐帧控制窗口动画的播放  
具体窗口的动画变化由WindowStateAnimator的stepAnimationLocked来控制，通过改变窗口的大小、位置、透明度（通过SurfaceControl代理实现对surfaceflinger的调用），来最终达到窗口动画的实现

  

![](https://upload-images.jianshu.io/upload_images/26874665-fc9fc238adf6ad7c.png?imageMogr2/auto-orient/strip|imageView2/2/w/662/format/webp)

wms历史版本动画时序图.png

有兴趣的可以去仔细研究下这部分代码的实现，虽然是历史版本的旧代码，但是这个对wms的学习理解有很大的帮助。

```cpp
/*frameworks/base/services/core/java/com/android/wm/WindowAnimator.java */

/** Locked on mService.mWindowMap. */
private void animateLocked(long frameTimeNs) {
```

这个方案有个很大的缺陷，那就是动画的所有实现的代码都包含在wms的主锁mGlobalLock里面，从动画主要方法的命名后缀locked可以得知，那么意味动画会跟wms其他所有流程抢CPU资源，就容易导致wms主锁的卡顿，在某些复杂的用户场景下，容易导致手机的卡顿，给用户带来糟糕的体验。

2、在androidP及之后的版本，google对窗口动画进行了重构，主要思想是通过ValueAnimator属性动画来播放窗口动画，把窗口动画播放从wms主锁脱离出来，这样动画就不会占用wms资源，从而达到优化系统框架运行速度的效果，同时把部分动画放到app远端播放（比如状态栏、导航键动画，比如多任务动画），达到系统和APP双端协调播放复杂的跨端动画效果

3、wms的新窗口动画主要分为两种类型，LocalAnimationAdapter和RemoteAnimationAdapter，分别实现了wms本地窗口动画和远程窗口动画。远程窗口动画机制，主要是为了实现android的两个新功能特意开发的机制，一个是从桌面点击app图标进入app的入场动画和app退出的出场动画，一个是在app界面，通过拖动底部指示条进入桌面的滑动效果动画，这两个动画效果最先是iphone实现的，google为了仿iphone的实现，所以开发了远程动画机制，最终能达到iphone的动画效果，提高了android手机的复杂动画效果

4、本文主要介绍android新动画的流程和实现的原理，主要介绍了LocalAnimationAdapter本地窗口动画实现原理

# 四、新动画机制Local窗口动画流程

本地窗口动画具体场景：可以在设置首页点击其中一项菜单，进入设置某项子菜单，然后就会有一个Local窗口动画的播放

LocalAnimationAdapter，字面意思就是wms本地窗口动画，该动画在SurfaceAnimationThread（线程名android.anim.lf）线程播放，注意看该类的注释  
（本文剩余源码基于androidS原生源码）

```php
/*frameworks/base/services/core/java/com/android/wm/SurfaceAnimationThread.java */

/**
 * Thread for running {@link SurfaceAnimationRunner} that does not hold the window manager lock.
 */
public final class SurfaceAnimationThread extends ServiceThread {
```

这个才是新动画机制的核心要义，不占用wms主锁，就不会占用wms的资源，这个已经是对wms很大的优化了，android历史版本因为wms锁卡顿的问题太多了

接下来我们通过阅读源码来分析LocalAnimationAdapter的实现流程，先看下整体的Local窗口动画时序图

  

![](https://upload-images.jianshu.io/upload_images/26874665-68873eed3a6f470f.png?imageMogr2/auto-orient/strip|imageView2/2/w/907/format/webp)

Local窗口动画时序图.png

1、动画播放源头类AppTransitionController的方法handleAppTransitionReady  
在一次wms大遍历（performSurfacePlacement）流程结束之后，就会检查app transition是否已经准备好，opening 的app准备好需要满足app的starting窗口是否已经displayed或者app的window是否已经alldrawn，只要满足其中一个条件，就说明app的窗口动画流程可以开始了了。

```cpp
/*frameworks/base/services/core/java/com/android/wm/AppTransitionController.java */

    void handleAppTransitionReady() {
        mTempTransitionReasons.clear();
        //检查app transition是否已经准备好
        if (!transitionGoodToGo(mDisplayContent.mOpeningApps, mTempTransitionReasons)
                || !transitionGoodToGo(mDisplayContent.mChangingContainers,
                        mTempTransitionReasons)) {
            return;
        }
        Trace.traceBegin(Trace.TRACE_TAG_WINDOW_MANAGER, "AppTransitionReady");

        ProtoLog.v(WM_DEBUG_APP_TRANSITIONS, "**** GOOD TO GO");
```

首先会获取当前需要opening和closing的app window列表（ActivityRecord类型）

```dart
/*frameworks/base/services/core/java/com/android/wm/AppTransitionController.java */

        final ActivityRecord topOpeningApp =
                getTopApp(mDisplayContent.mOpeningApps, false /* ignoreHidden */);
        final ActivityRecord topClosingApp =
                getTopApp(mDisplayContent.mClosingApps, false /* ignoreHidden */);
```

然后在applyAnimations方法里面对window列表进行遍历WindowContainer的动画applyAnimation方法的调用

```java
/*frameworks/base/services/core/java/com/android/wm/AppTransitionController.java */

     private void applyAnimations(ArraySet<WindowContainer> wcs, ArraySet<ActivityRecord> apps,
            @TransitionOldType int transit, boolean visible, LayoutParams animLp,
            boolean voiceInteraction) {
        final int wcsCount = wcs.size();
        for (int i = 0; i < wcsCount; i++) {
            final WindowContainer wc = wcs.valueAt(i);
            final ArrayList<ActivityRecord> transitioningDescendants = new ArrayList<>();
            for (int j = 0; j < apps.size(); ++j) {
                final ActivityRecord app = apps.valueAt(j);
                if (app.isDescendantOf(wc)) {
                    transitioningDescendants.add(app);
                }
            }
            wc.applyAnimation(animLp, transit, visible, voiceInteraction, transitioningDescendants);
        }
    }
```

2、在WindowContainer，会先收集getAnimationAdpater当前window的动画适配器

```java
/*frameworks/base/services/core/java/com/android/wm/WindowContainer.java */

    protected void applyAnimationUnchecked(WindowManager.LayoutParams lp, boolean enter,
            @TransitionOldType int transit, boolean isVoiceInteraction,
            @Nullable ArrayList<WindowContainer> sources) {
 
        final Pair<AnimationAdapter, AnimationAdapter> adapters = getAnimationAdapter(lp,
                transit, enter, isVoiceInteraction);
```

如果是普通的窗口动画，比如app内部activity的切换，当前的场景是设置主菜单跳转子菜单，根据当前场景获取到具体的transit，transit=TRANSIT_OLD_ACTIVITY_OPEN，然后再结合enter为true或者false，可以最终可以找到设置主菜单的动画xml资源是activity_open_exit.xml，设置子菜单的动画xml资源是activity_open_enter.xml，在获取到具体xml资源名字后，通过AnimationUtils.loadAnimation方法把xml资源转成Animation对象。  
之后就会创建一个WindowAnimationSpec对象，并把Animation对象作为构造方法的第一个参数传给了WindowAnimationSpec

```dart
/*frameworks/base/services/core/java/com/android/wm/WindowContainer.java */

                final Animation a = loadAnimation(lp, transit, enter, isVoiceInteratction);
                AnimationAdapter adapter = new LocalAnimationAdapter(
                        //创建了一个WindowAnimatonSpec对象作为LocalAnimationAdapter的初始化参数
                        new WindowAnimationSpec(a, mTmpPoint, mTmpRect,
                                getDisplayContent().mAppTransition.canSkipFirstFrame(),
                                appRootTaskClipMode, true /* isAppAnimation */, windowCornerRadius),
                        getSurfaceAnimationRunner());
```

这里创建LocalAnimationAdapter对象的时候同时创建了一个WindowAnimatonSpec对象作为LocalAnimationAdapter的初始化参数，这个类WindowAnimatonSpec比较重要，是在后续窗口动画播放的时候具体的实现类，后面再分析  
在获取到具体的Adaper对象之后，就开始执行startAnimation方法，这个方法里面主要调用了mSurfaceAnimator对象，来实现startAnimation

```css
/*frameworks/base/services/core/java/com/android/wm/WindowContainer.java */

        mSurfaceAnimator.startAnimation(t, anim, hidden, type, animationFinishedCallback,
                mSurfaceFreezer);
```

tip: WindowContainer这个类是wms的最重要类之一，它是所有window的基类，充分学习理解该类可以对wms的所有window的树状图有一定的理解

3、SurfaceAnimator类，字面上的意思就是window动画实现是交给它来实现surfacecontrol的动画（旧窗口动画是通过WindowSurfaceController控制surfacecontrol），该类的就是窗口动画的中控，它的主要作用是在startAnimation的时候，对要进行动画的surfacecontrol创建一个parent的surfacecontrol类型的mLeash对象，leash的翻译是用皮带系住的意思，相当于把要进行动画的surfacecontrol用皮带系住，通过操控mLeash对象来实现窗口的大小、位置、透明度等动画属性的改变。

```tsx
/*frameworks/base/services/core/java/com/android/wm/SurfaceAnimator.java */

    void startAnimation(Transaction t, AnimationAdapter anim, boolean hidden,
            @AnimationType int type,
            @Nullable OnAnimationFinishedCallback animationFinishedCallback,
            @Nullable SurfaceFreezer freezer) {
        cancelAnimation(t, true /* restarting */, true /* forwardCancel */);
        mAnimation = anim;
        mAnimationType = type;
        mAnimationFinishedCallback = animationFinishedCallback;
        final SurfaceControl surface = mAnimatable.getSurfaceControl();
        mLeash = freezer != null ? freezer.takeLeashForAnimation() : null;
        if (mLeash == null) {
            //重点关注这个mLeash对象，该对象是窗口动画专属surfacecontrol包装对象
            mLeash = createAnimationLeash(mAnimatable, surface, t, type,
                    mAnimatable.getSurfaceWidth(), mAnimatable.getSurfaceHeight(), 0 /* x */,
                    0 /* y */, hidden, mService.mTransactionFactory);
            mAnimatable.onAnimationLeashCreated(t, mLeash);
        }
        mAnimatable.onLeashAnimationStarting(t, mLeash);
        mAnimation.startAnimation(mLeash, t, type, mInnerAnimationFinishedCallback);
    }
```

然后在动画结束之后，mLeash对象会走销毁的流程，同时动画的surfacecontrol进行reparent还原操作。

```java
/*frameworks/base/services/core/java/com/android/wm/SurfaceAnimator.java */

    static boolean removeLeash(Transaction t, Animatable animatable, @NonNull SurfaceControl leash,
            boolean destroy) {
        boolean scheduleAnim = false;
        final SurfaceControl surface = animatable.getSurfaceControl();
        final SurfaceControl parent = animatable.getParentSurfaceControl();
        final boolean reparent = surface != null;
        if (reparent) {
            if (surface.isValid() && parent != null && parent.isValid()) {
                t.reparent(surface, parent);
                scheduleAnim = true;
            }
        }
```

窗口动画中控最终调用了动画的适配类LocalAnimationAdapter的startAnimation方法。

```css
/*frameworks/base/services/core/java/com/android/wm/SurfaceAnimator.java */

        mAnimation.startAnimation(mLeash, t, type, mInnerAnimationFinishedCallback);
```

4、LocalAnimationAdapter是窗口动画的适配类，继承自AnimationAdaper，LocalAnimationAdapter实现的是wms本地窗口动画，所以LocalAnimationAdapter可以理解成本地窗口动画的中转类。在startAnimation方法里面，调用了SurfaceAnimationRunner来最终实现动画的播放。

```tsx
/*frameworks/base/services/core/java/com/android/wm/LocalAnimationAdapter.java */

    @Override
    public void startAnimation(SurfaceControl animationLeash, Transaction t,
            @AnimationType int type, OnAnimationFinishedCallback finishCallback) {
        mAnimator.startAnimation(mSpec, animationLeash, t,
                () -> finishCallback.onAnimationFinished(type, this));
    }
```

5、SurfaceAnimationRunner是本地窗口动画真正的实现类，主要需要关注的方法是startAnimationLocked，首先这个方法已经通过mChoreographer切换到SurfaceAnimationThread线程来执行，然后创建了ValueAnimator属性动画对象，交由ValueAnimator属性动画对象的addUpdateListener方法来实现逐帧控制动画mLeash对象（surfacecontrol类型）的变化，具体的update方法的实现是在WindowAnimatonSpec类的apply方法里面。

```java
/*frameworks/base/services/core/java/com/android/wm/SurfaceAnimationRunner.java */

    private void startAnimationLocked(RunningAnimation a) {
        final ValueAnimator anim = mAnimatorFactory.makeAnimator();

        anim.overrideDurationScale(1.0f);
        anim.setDuration(a.mAnimSpec.getDuration());
        anim.addUpdateListener(animation -> {
            synchronized (mCancelLock) {
                if (!a.mCancelled) {
                    final long duration = anim.getDuration();
                    long currentPlayTime = anim.getCurrentPlayTime();
                    if (currentPlayTime > duration) {
                        currentPlayTime = duration;
                    }
                    applyTransformation(a, mFrameTransaction, currentPlayTime);
                }
            }

            scheduleApplyTransaction();
        });

        ………………
        a.mAnim = anim;
        mRunningAnimations.put(a.mLeash, a);

        //窗口动画最终调用了属性动画播放
        anim.start();
        anim.doAnimationFrame(mChoreographer.getFrameTime());
    }
```

再来看下apply方法的具体实现，通过之前以具体xml资源创建的mAnimation对象，根据当前时间片currentPlayTime获取到当前的tmp.transformation，对leash对象实现了Matrix（大小，位置），Alpha，Crop等transformation变化，再通过Transaction 交给surfaceflinger显示，从而实现了动画当前时间片的显示效果。对比旧动画机制，这个transformation变化是在WindowStateAnimator类里面实现的。为什么要重点关注这个方法呢？因为如果窗口动画出bug了（位置大小不对？透明度异常？），就可以在这个方法里面打印window的相关参数来初步定位原因。

```java
/*frameworks/base/services/core/java/com/android/wm/WindowAnimatonSpec.java */

    @Override
    public void apply(Transaction t, SurfaceControl leash, long currentPlayTime) {
        final TmpValues tmp = mThreadLocalTmps.get();
        tmp.transformation.clear();
        mAnimation.getTransformation(currentPlayTime, tmp.transformation);
        tmp.transformation.getMatrix().postTranslate(mPosition.x, mPosition.y);
        t.setMatrix(leash, tmp.transformation.getMatrix(), tmp.floats);
        t.setAlpha(leash, tmp.transformation.getAlpha());

        boolean cropSet = false;
        if (mRootTaskClipMode == ROOT_TASK_CLIP_NONE) {
            if (tmp.transformation.hasClipRect()) {
                t.setWindowCrop(leash, tmp.transformation.getClipRect());
                cropSet = true;
            }
        } else {
            mTmpRect.set(mRootTaskBounds);
            if (tmp.transformation.hasClipRect()) {
                mTmpRect.intersect(tmp.transformation.getClipRect());
            }
            t.setWindowCrop(leash, mTmpRect);
            cropSet = true;
        }
    }
```

6、ValueAnimator类，从上面的介绍可以得知，窗口动画的最终本质就是一个ValueAnimator属性动画，理解了这一点，就相当于把窗口动画简单化了，最终的实现就类比于我们普通app的属性动画的实现（app属性动画的对象是view，窗口属性动画的对象是window），只不过整个流程比较复杂而已，但是最终的实现原理是一样的，殊途同归，这个才是android窗口动画机制的精髓所在。

```rust
/*frameworks/base/services/core/java/com/android/wm/SurfaceAnimationRunner.java */

ValueAnimator anim = mAnimatorFactory.makeAnimator();
anim.addUpdateListener(animation -> {
            applyTransformation(a, mFrameTransaction, currentPlayTime);
        });
anim.start();
```

# 总结

本文只是讲解了WindowManagerService窗口动画之本地窗口动画的原生实现流程，主要是WindowManagerService的子类比较多，所以我们从动画的源头handleAppTransitionReady一步一步分析了它的整个流程，从整个流程来看，本地窗口动画的逻辑比较清晰，线路也比较单一，比较容易学习和理解，最终我们看到，窗口动画的原理就是一个属性动画，在动画update方法里面操控了窗口surface的属性变化，从而实现了窗口动画的逐帧播放。另外还有一个重点需要关注到，那就是wms的窗口动画不需要占用wms主锁，而且是单独线程，这样的设计也能在一定程度上优化系统卡顿的问题。

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/d41fe965e9e6  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。