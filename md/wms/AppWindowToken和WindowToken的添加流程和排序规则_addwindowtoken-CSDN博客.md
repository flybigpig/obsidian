本篇基于AndroidQ代码分析  
我们知道Android系统有三种类型窗口，应用窗口，系统窗口，子窗口，无论哪种窗口在WMS都会用一个WindowState来描述，

[Android窗口Z轴计算以及WindowState排列规则](https://blog.csdn.net/qq_34211365/article/details/103873512)详细介绍了WindowState的排列规则，

每个窗口都需要一种token以识别身份，应用窗口对应AppWindowToken，系统窗口对应WindowToken，子窗口对应父窗口ViewRootImpl的W对象，

另外三种类型窗口在WMS对应的WindowState会分别添加在不同的地方，
```
应用窗口添加在AppWindowToken的WindowList中，
系统窗口添加在WindowToken的WindowList中，
子窗口添加在父窗口的WindowState的WindowList中
```

AppWindowToken继承WindowToken，WindowToken继承WindowContainer，WindowState也是继承WindowContainer，WindowList是WindowContainer的成员变量，是一个ArrayList

```
// List of children for this window container. List is in z-order as the children appear on
    // screen with the top-most window container at the tail of the list.
    protected final WindowList<E> mChildren = new WindowList<E>();
```

这张图引自罗升阳的博客[Android窗口管理服务WindowManagerService对窗口的组织方式分析](https://blog.csdn.net/Luoshengyang/article/details/8498908)

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/aa64f47aa043aa4f9fd86703620274b8.png)  
我们可以从图中分析窗口的关系；  
一个Activity对应一个ActivityRecord，一个ActivityRecord对应一个，AppWindowToken，一个AppWindowToken对应一组WindowState，一个WindowState可能还有子WindowState，当ActivityRecord顺序定好了，AppWindowToken顺序也好了，WindowState的顺序自然也定好了，  
[Android窗口Z轴计算以及WindowState排列规则](https://blog.csdn.net/qq_34211365/article/details/103873512)已经介绍了WindowState的排序规则，WindowState排好之后再对AppWindowToken和WindowToken排序，这样整个窗口的Z-order就排好了，这篇文章来分析下AppWindowToken和WindowToken的添加排序过程

AppWindowToken是在Activity启动过程创建的

### ActivityStack.startActivityLocked

```
 void startActivityLocked(ActivityRecord r, ActivityRecord focusedTopActivity,
            boolean newTask, boolean keepCurTransition, ActivityOptions options) {
......
if (!startIt) {
                 r.createAppWindowToken();
                 .....
                    return;
              }
......
}
```

### ActivityRecord.createAppWindowToken

这个方法里为Activity创建了AppWindowToken，并通过Task进行添加，这里的Task同样继承自WindowContainer

```
void createAppWindowToken() {
......
if (mAppWindowToken != null) {
            // TODO: Should this throw an exception instead?
            Slog.w(TAG, "Attempted to add existing app token: " + appToken);
        } else {
            final Task container = task.getTask();
            if (container == null) {
                throw new IllegalArgumentException("createAppWindowToken: invalid task =" + task);
            }
            mAppWindowToken = createAppWindow(mAtmService.mWindowManager, appToken,
                    task.voiceSession != null, container.getDisplayContent(),
                    ActivityTaskManagerService.getInputDispatchingTimeoutLocked(this)
                            * 1000000L, fullscreen,
                    (info.flags & FLAG_SHOW_FOR_ALL_USERS) != 0, appInfo.targetSdkVersion,
                    info.screenOrientation, mRotationAnimationHint,
                    mLaunchTaskBehind, isAlwaysFocusable());
            container.addChild(mAppWindowToken, Integer.MAX_VALUE /* add on top */);
        }
......
}
```

```
AppWindowToken createAppWindow(WindowManagerService service, IApplicationToken token,
            boolean voiceInteraction, DisplayContent dc, long inputDispatchingTimeoutNanos,
            boolean fullscreen, boolean showForAllUsers, int targetSdk, int orientation,
            int rotationAnimationHint, boolean launchTaskBehind,
            boolean alwaysFocusable) {
        return new AppWindowToken(service, token, mActivityComponent, voiceInteraction, dc,
                inputDispatchingTimeoutNanos, fullscreen, showForAllUsers, targetSdk, orientation,
                rotationAnimationHint, launchTaskBehind, alwaysFocusable,
                this);
    }
```

new了一个AppWindowToken，这里传递的token是ActivityRecord构造方法中创建的IApplicationToken.Stub对象

```
 AppWindowToken(WindowManagerService service, IApplicationToken token,
            ComponentName activityComponent, boolean voiceInteraction, DisplayContent dc,
            long inputDispatchingTimeoutNanos, boolean fullscreen, boolean showForAllUsers,
            int targetSdk, int orientation, int rotationAnimationHint,
            boolean launchTaskBehind, boolean alwaysFocusable,
            ActivityRecord activityRecord) {
        this(service, token, activityComponent, voiceInteraction, dc, fullscreen);
        //省略赋值代码
       .....
    }
```

AppWindowToken继承WindowToken，这里调用了WindowToken构造方法

```
 WindowToken(WindowManagerService service, IBinder _token, int type, boolean persistOnEmpty,
            DisplayContent dc, boolean ownerCanManageAppTokens, boolean roundedCornerOverlay) {
        super(service);
        token = _token;
        windowType = type;
        mPersistOnEmpty = persistOnEmpty;
        mOwnerCanManageAppTokens = ownerCanManageAppTokens;
        mRoundedCornerOverlay = roundedCornerOverlay;
        onDisplayChanged(dc);
    }
```

### onDisplayChanged

```
void onDisplayChanged(DisplayContent dc) {
        dc.reParentWindowToken(this);
//子类回调方法
        super.onDisplayChanged(dc);
    }
```

### DisplayContent.reParentWindowToken

```
void reParentWindowToken(WindowToken token) {
        final DisplayContent prevDc = token.getDisplayContent();
        if (prevDc == this) {
            return;
        }
        if (prevDc != null) {
            if (prevDc.mTokenMap.remove(token.token) != null && token.asAppWindowToken() == null) {
                // Removed the token from the map, but made sure it's not an app token before
                // removing from parent.
                token.getParent().removeChild(token);
            }
            if (prevDc.mLastFocus == mCurrentFocus) {
                // The window has become the focus of this display, so it should not be notified
                // that it lost focus from the previous display.
                prevDc.mLastFocus = null;
            }
        }
//添加AppWindowToken
        addWindowToken(token.token, token);
    }
```

在看addWindowToken之前先看下DisplayContent构造方法，DisplayContent继承WindowContainers，这里将四个容器添加到了父类的WindowList中

```
    DisplayContent(Display display, WindowManagerService service,
            ActivityDisplay activityDisplay) {
        .....
        //mBelowAppWindowsContainers存储壁纸窗口WindowToken
super.addChild(mBelowAppWindowsContainers, null);
//mTaskStackContainers存储TaskStack而不是AppWindowToken
        super.addChild(mTaskStackContainers, null);
        //mAboveAppWindowsContainers存储系统窗口WindowToken
        super.addChild(mAboveAppWindowsContainers, null);
        //mImeWindowsContainers存储输入法相关窗口WindowToken
        super.addChild(mImeWindowsContainers, null);
        .....
}
```

这四个类都是WindowContainer的间接子类，并且除mTaskStackContainers外的其他三个类存储的都是WindowToken类型，mTaskStackContainers存储的是TaskStack类型，可以猜测TaskStack里面存储的应该就是AppWindowToken

```
    private final class TaskStackContainers extends
 DisplayChildWindowContainer<TaskStack>
    private final TaskStackContainers mTaskStackContainers = new TaskStackContainers(mWmService);
    
    private final class AboveAppWindowContainers extends
     NonAppWindowContainers
    private final AboveAppWindowContainers mAboveAppWindowsContainers =
            new AboveAppWindowContainers("mAboveAppWindowsContainers", mWmService);
            
    private class NonAppWindowContainers extends
     DisplayChildWindowContainer<WindowToken>
    private final NonAppWindowContainers mBelowAppWindowsContainers =
            new NonAppWindowContainers("mBelowAppWindowsContainers", mWmService);
            
    private final NonAppWindowContainers mImeWindowsContainers =
            new NonAppWindowContainers("mImeWindowsContainers", mWmService);
            
    static class DisplayChildWindowContainer<E extends WindowContainer> 
     extends WindowContainer<E>
```

WindowContainer的addChild方法通过传递的自定义比较器对child进行排序，  
DisplayContent中添加的四个类没有传入比较器，则按默认ArrayList添加顺序排序，即按\[mBelowAppWindowsContainers，mTaskStackContainers，mAboveAppWindowsContainers，mImeWindowsContainers\]的顺序，到这里我们可以知道了mImeWindowsContainers里面放的窗口在最上面，mBelowAppWindowsContainers里面放的窗口在最底下

```
 protected void addChild(E child, Comparator<E> comparator) {
 ......
        int positionToAdd = -1;
        if (comparator != null) {
            final int count = mChildren.size();
            for (int i = 0; i < count; i++) {
                if (comparator.compare(child, mChildren.get(i)) < 0) {
                    positionToAdd = i;
                    break;
                }
            }
        }
        //-1代表按顺序添加
        if (positionToAdd == -1) {
            mChildren.add(child);
        } else {
        //插入特定位置
            mChildren.add(positionToAdd, child);
        }
...
    }
```

addWindowToken方法里面可以看到，将系统类型窗口分了三类，壁纸类型，输入法相关类型，其他类型，三种分别添加到mBelowAppWindowsContainers，mImeWindowsContainers和mAboveAppWindowsContainers

```
private void addWindowToken(IBinder binder, WindowToken token) {
......
        //mTokenMap是一个hashMap，只要创建了WindowToken的窗口
        //都会在这个map里面，包括AppWindowToken和WindowToken
        mTokenMap.put(binder, token);
//token.asAppWindowToken为空代表不是应用类型窗口
        if (token.asAppWindowToken() == null) {
            switch (token.windowType) {
                case TYPE_WALLPAPER:
                //壁纸类型添加到mBelowAppWindowsContainers
                    mBelowAppWindowsContainers.addChild(token);
                    break;
                case TYPE_INPUT_METHOD:
                case TYPE_INPUT_METHOD_DIALOG:
                //输入法窗口或者输入法对话窗口添加到mImeWindowsContainers
                    mImeWindowsContainers.addChild(token);
                    break;
                default:
                //其他类型添加到mAboveAppWindowsContainers
                    mAboveAppWindowsContainers.addChild(token);
                    break;
            }
        }
    }
```

最终我们发现这个方法里没有添加AppWindowToken，那AppWindowToken在哪里添加的呢？我们回到ActivityRecord.createAppWindowToken方法

```
void createAppWindowToken() {
......
if (mAppWindowToken != null) {
            // TODO: Should this throw an exception instead?
            Slog.w(TAG, "Attempted to add existing app token: " + appToken);
        } else {
            final Task container = task.getTask();
            if (container == null) {
                throw new IllegalArgumentException("createAppWindowToken: invalid task =" + task);
            }
            mAppWindowToken = createAppWindow(mAtmService.mWindowManager, appToken,
                    task.voiceSession != null, container.getDisplayContent(),
                    ActivityTaskManagerService.getInputDispatchingTimeoutLocked(this)
                            * 1000000L, fullscreen,
                    (info.flags & FLAG_SHOW_FOR_ALL_USERS) != 0, appInfo.targetSdkVersion,
                    info.screenOrientation, mRotationAnimationHint,
                    mLaunchTaskBehind, isAlwaysFocusable());
             //传递一个int值最大数，保证添加的AppWindowToken总是在集合尾部
            container.addChild(mAppWindowToken, Integer.MAX_VALUE /* add on top */);
        }
......
}
```

### Task.addChild

Task继承WindowContainer，getAdjustedAddPosition方法获取具体添加的position

```
@Override
    void addChild(AppWindowToken wtoken, int position) {
          position = getAdjustedAddPosition(position);
          super.addChild(wtoken, position);
          mDeferRemoval = false;
      }
```

### getAdjustedAddPosition

getAdjustedAddPosition这个方法就是获取AppWindowToken集合的size

```
private int getAdjustedAddPosition(int suggestedPosition) {
  //获取当前AppWindowToken集合的大小
          final int size = mChildren.size();
          //suggestedPosition传递的是int最大值，肯定是大于size的
          if (suggestedPosition >= size) {
              return Math.min(size, suggestedPosition);
          }
  
          for (int pos = 0; pos < size && pos < suggestedPosition; ++pos) {
              // TODO: Confirm that this is the behavior we want long term.
              if (mChildren.get(pos).removed) {
                  // suggestedPosition assumes removed tokens are actually gone.
                  ++suggestedPosition;
              }
          }
          return Math.min(size, suggestedPosition);
      }
```

接着调用父类addChild方法将AppWindowToken添加到集合尾部

```
void addChild(E child, int index) {
        .....
       //对index做基本判断
        if ((index < 0 && index != POSITION_BOTTOM)
                || (index > mChildren.size() && index != POSITION_TOP)) {
            throw new IllegalArgumentException("addChild: invalid position=" + index
                    + ", children number=" + mChildren.size());
        }
//如果index是int最大值就直接等于AppWindowToken集合大小
        if (index == POSITION_TOP) {
            index = mChildren.size();
            //如果index是int最小值就等于0
        } else if (index == POSITION_BOTTOM) {
            index = 0;
        }
//添加到集合
        mChildren.add(index, child);
       .....
    }
```

到这里AppWindowToken就添加到了Task的WindowList中，前面我们说到mTaskStackContainers存储的是TaskStack，TaskStack存储的是Task，而Task存储AppWindowToken，这样AppWindowToken就被间接存储到mTaskStackContainers中，最终他们对应关系应该是：  
**mTaskStackContainers存储一组TaskStack，TaskStack存储一组Task，Task存储一组AppWindowToken，AppWindowToken存储一组WindowState，WindowState还可以有子WindowState**

我们这篇文章只分析AppWindowToken添加和排序，mTaskStackContainers的数据填充这里就不去看了，涉及Activity的启动过程，非常复杂

AppWindowToken分析完了接着分析WindowToken的添加和排序，其实之前我们已经提到了WindowToken的添加，系统类型窗口被分了三类，壁纸类型，输入法相关类型，其他类型，三种分别添加到mBelowAppWindowsContainers，mImeWindowsContainers和mAboveAppWindowsContainers，添加过程同样通过WindowToken的构造方法 -> onDisplayChanged -> DisplayContent.reParentWindowToken->addWindowToken

### addWindowToken

```
private void addWindowToken(IBinder binder, WindowToken token) {
        ......
        mTokenMap.put(binder, token);
//系统类型窗口才添加
        if (token.asAppWindowToken() == null) {
            switch (token.windowType) {
                case TYPE_WALLPAPER:
                //壁纸类型
                    mBelowAppWindowsContainers.addChild(token);
                    break;
                case TYPE_INPUT_METHOD:
                case TYPE_INPUT_METHOD_DIALOG:
                //输入法和输入法对话框类型
                    mImeWindowsContainers.addChild(token);
                    break;
                default:
                //其他类型
                    mAboveAppWindowsContainers.addChild(token);
                    break;
            }
        }
    }
```

壁纸和输入法两种特殊类型窗口先不用管，我们看其他类型的系统窗口  
AboveAppWindowContainers继承NonAppWindowContainers，addChild调用的是父类的方法，父类NonAppWindowContainers又调用了父类WindowContainer的方法，WindowContainer的addChild方法之前的文章[Android窗口Z轴计算以及WindowState排列规则](https://blog.csdn.net/qq_34211365/article/details/103873512)已经分析过多次了，主要就是通过传递的自定义比较器对传递的child进行排序

```
 void addChild(WindowToken token) {
           addChild(token, mWindowComparator);
       }
```

我们看一下mWindowComparator的比较规则，getWindowLayerFromTypeLw这个方法会通过不同窗口类型返回不同数值，有三十多种类型，不同类型就代表不同层级

```
 private final Comparator<WindowToken> mWindowComparator = (token1, token2) ->
                mWmService.mPolicy.getWindowLayerFromTypeLw(token1.windowType,
                        token1.mOwnerCanManageAppTokens)
                < mWmService.mPolicy.getWindowLayerFromTypeLw(token2.windowType,
                        token2.mOwnerCanManageAppTokens) ? -1 : 1;
```

```
default int getWindowLayerFromTypeLw(int type, boolean canAddInternalSystemWindow) {
        if (type >= FIRST_APPLICATION_WINDOW && type <= LAST_APPLICATION_WINDOW) {
            return APPLICATION_LAYER;
        }

        switch (type) {
            case TYPE_WALLPAPER:
                // wallpaper is at the bottom, though the window manager may move it.
                return  1;
            case TYPE_PRESENTATION:
            case TYPE_PRIVATE_PRESENTATION:
                return  APPLICATION_LAYER;
            case TYPE_DOCK_DIVIDER:
                return  APPLICATION_LAYER;
            case TYPE_QS_DIALOG:
                return  APPLICATION_LAYER;
            case TYPE_PHONE:
                return  3;
            case TYPE_SEARCH_BAR:
            case TYPE_VOICE_INTERACTION_STARTING:
                return  4;
            case TYPE_VOICE_INTERACTION:
                // voice interaction layer is almost immediately above apps.
                return  5;
            case TYPE_INPUT_CONSUMER:
                return  6;
            case TYPE_SYSTEM_DIALOG:
                return  7;
            case TYPE_TOAST:
                // toasts and the plugged-in battery thing
                return  8;
                
                .....
                
               default:
                Slog.e("WindowManager", "Unknown window type: " + type);
                return APPLICATION_LAYER;
```

所以如果当前添加的窗口的层级小于与之进行比较的窗口层级则返回-1，否则返回1，返回1和返回-1的结果就是向mAboveAppWindowsContainers的WindowList中添加元素的位置不同，-1则positionToAdd会重新赋值，赋的值等于与当前添加窗口进行比较的窗口的位置，即我们添加的窗口会插入positionToAdd的位置，返回1则直接添加到WindowList的尾部即可

```
 protected void addChild(E child, Comparator<E> comparator) {
      .....
        int positionToAdd = -1;
        if (comparator != null) {
            final int count = mChildren.size();
            for (int i = 0; i < count; i++) {
                if (comparator.compare(child, mChildren.get(i)) < 0) {
                    positionToAdd = i;
                    break;
                }
            }
        }
        if (positionToAdd == -1) {
            mChildren.add(child);
        } else {
            mChildren.add(positionToAdd, child);
        }
        ......
        child.setParent(this);
    }
```

可以得出结论mAboveAppWindowsContainers的WindowList一定是按窗口的层级数值由小到大进行排列的

到这里我们已经分析完了AppWindowToken和WindowToken的添加流程和排序规则，我们通过系统dump命令来看一下系统所有窗口已经token存储即排列规则  
创建一个简单的Activity  
在MainActivity界面dump一下

```
Dump time : 2020-01-10 04:19:28.976
ROOT type=undefined mode=fullscreen
  #0 Display 0 name="Built-in screen" type=undefined mode=fullscreen
   #3 mImeWindowsContainers type=undefined mode=fullscreen
    #0 WindowToken{f127d64 android.os.Binder@d21eaf7} type=undefined mode=fullscreen
   #2 mAboveAppWindowsContainers type=undefined mode=fullscreen
    #3 WindowToken{d72862d android.os.BinderProxy@856e544} type=undefined mode=fullscreen
     #0 2730762 NavigationBar type=undefined mode=fullscreen
    #2 WindowToken{c351b88 android.os.BinderProxy@2f95b2b} type=undefined mode=fullscreen
     #0 4305921 StatusBar type=undefined mode=fullscreen
    #1 WindowToken{1800ac7 android.os.BinderProxy@9af9406} type=undefined mode=fullscreen
     #0 3b768f4 AssistPreviewPanel type=undefined mode=fullscreen
    #0 WindowToken{8a36a18 android.os.BinderProxy@2ac89fb} type=undefined mode=fullscreen
     #0 e77ac71 DockedStackDivider type=undefined mode=fullscreen
   #1 com.android.server.wm.DisplayContent$TaskStackContainers@edb3b60 type=undefined mode=fullscreen
    #2 Stack=9 type=standard mode=fullscreen
     #0 Task=13 type=standard mode=fullscreen
      #0 AppWindowToken{472456c token=Token{6532e1f ActivityRecord{d064dbe u0 com.tct.aidl/.MainActivity t13}}} type=standard mode=fullscreen
       #0 b20eb59 com.tct.aidl/com.tct.aidl.MainActivity type=standard mode=fullscreen
    #1 Stack=0 type=home mode=fullscreen
     #1 Task=3 type=home mode=fullscreen
      #0 AppWindowToken{e3fcb14 token=Token{4822167 ActivityRecord{de76126 u0 com.tct.launcher/.Launcher t3}}} type=home mode=fullscreen
       #0 a1b2ccd com.tct.launcher/com.tct.launcher.Launcher type=home mode=fullscreen
     #0 Task=4 type=home mode=fullscreen
      #0 AppWindowToken{764dc81 token=Token{4e61068 ActivityRecord{773978b u0 com.tct.launcher/.Launcher t4}}} type=home mode=fullscreen
    #0 Stack=8 type=recents mode=fullscreen
     #0 Task=12 type=recents mode=fullscreen
      #0 AppWindowToken{3696d7b token=Token{472c10a ActivityRecord{12bfd75 u0 com.android.systemui/.recents.RecentsActivity t12}}} type=recents mode=fullscreen
       #0 ce2c94f com.android.systemui/com.android.systemui.recents.RecentsActivity type=recents mode=fullscreen
   #0 mBelowAppWindowsContainers type=undefined mode=fullscreen
    #0 WallpaperWindowToken{91b8faa token=android.os.Binder@f126295} type=undefined mode=fullscreen
     #0 8ff11c1 com.android.systemui.ImageWallpaper type=undefined mode=fullscreen
 
Window{2730762 u0 NavigationBar}
Window{4305921 u0 StatusBar}
Window{3b768f4 u0 AssistPreviewPanel}
Window{e77ac71 u0 DockedStackDivider}
Window{b20eb59 u0 com.tct.aidl/com.tct.aidl.MainActivity}
Window{a1b2ccd u0 com.tct.launcher/com.tct.launcher.Launcher}
Window{ce2c94f u0 com.android.systemui/com.android.systemui.recents.RecentsActivity}
Window{8ff11c1 u0 com.android.systemui.ImageWallpaper}

```

注意前面的空格，这段dump信息意思是：  
1.Display下有一个mImeWindowsContainers，一个mAboveAppWindowsContainers，一个DisplayContent$TaskStackContainers，一个mBelowAppWindowsContainers，这四个类就是我们前面分析的四大窗口容器

2.mImeWindowsContainers下有一个WindowToken这应该是输入法，但是没有WindowState，说明界面上是没有输入法的

3.mAboveAppWindowsContainers下有多个WindowToken，每个WindowToken下都有一个WindowState，分别是NavigationBar，StatusBar，AssistPreviewPanel，DockedStackDivider

4.DisplayContent$TaskStackContainers下有多个TaskStack，每个TaskStack有一个Task，每个Task下有一个AppWindowToken，每个AppWindowToken下有一个WindowState，分别是Window{b20eb59 u0 com.tct.aidl/com.tct.aidl.MainActivity}，  
Window{a1b2ccd u0 com.tct.launcher/com.tct.launcher.Launcher}，  
Window{ce2c94f u0 com.android.systemui/com.android.systemui.recents.RecentsActivity}

5.mBelowAppWindowsContainers下有一个WallpaperWindowToken，WallpaperWindowToken下有一个WindowState为Window{8ff11c1 u0 com.android.systemui.ImageWallpaper}

dump信息最后一段就是当前窗口的Z-order，NavigationBar最大，ImageWallpaper最小

```
Window{2730762 u0 NavigationBar}
Window{4305921 u0 StatusBar}
Window{3b768f4 u0 AssistPreviewPanel}
Window{e77ac71 u0 DockedStackDivider}
Window{b20eb59 u0 com.tct.aidl/com.tct.aidl.MainActivity}
Window{a1b2ccd u0 com.tct.launcher/com.tct.launcher.Launcher}
Window{ce2c94f u0 com.android.systemui/com.android.systemui.recents.RecentsActivity}
Window{8ff11c1 u0 com.android.systemui.ImageWallpaper}
```

可以看到和我们代码分析一样，Android系统创建的所有WindowToken和AppWindowToken都是直接或间接存储在这四个容器中，应用窗口和系统窗口描述信息WindowState则是存储在AppWindowToken或者WindowToken中，子窗口的WindowState存储在父窗口的WindowState中，子窗口也不会创建自己的WindowToken

[Android窗口Z轴计算以及WindowState排列规则](https://blog.csdn.net/qq_34211365/article/details/103873512)和这篇详细介绍了AppWindowToken，WindowToken，WindowState这三种对象的创建和添加以及排序流程，  
最终会分别添加到四种类型窗口容器，这个四个容器在DisplayContent构造方法中添加到了DisplayContent的WindowList中，传递的比较器为空即按默认ArrayList顺序添加，这四个窗口容器顺序排好了，再把他们里面的WindowToken排好顺序，再把WindowToken里面的WindowState排好顺序，这样一层一层下去，最终系统所有窗口都按照Z轴位置从低到高排列