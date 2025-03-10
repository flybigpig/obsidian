### 背景：

今年车展上，网友蚱蜢同学带回来了一些[车载](https://so.csdn.net/so/search?q=%E8%BD%A6%E8%BD%BD&spm=1001.2101.3001.7020)rom相关的一些素材，刚好也发布了[wms/ams专题课程](https://ke.qq.com/course/5992266#term_id=106217431)，有粉丝朋友提供了一个车机的双屏互动的产品交互视频如下：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/777e2bf1175485c16e56987dc2e4edff.gif#pic_center)

上面的就是车机两个屏幕的互动联动情况，转化成设计图如下：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/8b3d305bab756fe097dbfe7cf806419e.png#pic_center)

### 需求说明

这里对在个双屏需求进行要点细分：  
1、通过多手指对屏幕1画面A进行拖动滑动

2、画面A可以跟随手指进行平移，即有跟手功能

3、拖到一定阈值时候松手，画面A会自动动画移动到屏幕2

4、拖动和动画过程画面A依旧是可以刷新的，不是截图

5、在没有达到滑动屏幕2阈值，则需要对画面进行动画返回原来屏幕1的原来位置

上面4个要点就是实现的核心部分，主要难度在以下几个方面：  
1、多指全局动作监听，而不是在某个app的onTouch里面监听，这里其实之前的[input专题课程是有讲解的](https://ke.qq.com/course/package/83580?tuin=7d4eb354)  
所以这里部分基本上学员都可以搞定

2、画面A的平移，两个屏幕都要进行显示，这个难度较大，得考虑好相关的画面怎么都可以显示在两个屏幕

3、画面A移动过程也可以进行刷新，这个相对截图画面A进行显示难度可能又大一些

4、移动后整个画面A就完全在屏幕2了

### 需求实现

看完需求，大概也就知道本质就是通过在屏幕1上面多指触摸画面A，然后画面A可以到达屏幕2

那么先不考虑动画体验相关，如果只实现一个最简单版本的双屏移动，哈哈，其实这个车展上也有的车厂就是这样实现的：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4ccb71a790717f80f38587fc90adff1f.gif#pic_center)

这个就是没有动画版本的双屏互动，功能其实也是实现的，但是体验是不是感觉起来差的太多，而且还带有黑屏。。。  
不过还好的是功能可以用，那么我们也根据[wms](https://so.csdn.net/so/search?q=wms&spm=1001.2101.3001.7020)课程基础后也来实现一下这个双屏互动的功能。

1、背景知识-wms的层级结构树  
移动的画面A，其实一般就是我们Activity，一般在一个task中

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5871b5a850c0f211162e80f595cb280a.png)

task一般是挂载在某个displaycontent下的TaskDisplayArea下面，具体如下图所示  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/a201091344993677a7f8b11c86fa94e7.png#pic_center)

那么系统中有没有现成的方法就可以吧Task从这个display1移动到对应的diplay2下面呢？

答案当然是有的，方法就是RootWindowContainer里面的

```
/**
     * Move root task with all its existing content to specified display.
     *
     * @param rootTaskId Id of root task to move.
     * @param displayId  Id of display to move root task to.
     * @param onTop      Indicates whether container should be place on top or on bottom.
     */
    void moveRootTaskToDisplay(int rootTaskId, int displayId, boolean onTop) {
        final DisplayContent displayContent = getDisplayContentOrCreate(displayId);
        if (displayContent == null) {
            throw new IllegalArgumentException("moveRootTaskToDisplay: Unknown displayId="
                    + displayId);
        }

        moveRootTaskToTaskDisplayArea(rootTaskId, displayContent.getDefaultTaskDisplayArea(),
                onTop);
    }

    /**
     * Move root task with all its existing content to specified task display area.
     *
     * @param rootTaskId      Id of root task to move.
     * @param taskDisplayArea The task display area to move root task to.
     * @param onTop           Indicates whether container should be place on top or on bottom.
     */
    void moveRootTaskToTaskDisplayArea(int rootTaskId, TaskDisplayArea taskDisplayArea,
            boolean onTop) {
        final Task rootTask = getRootTask(rootTaskId);
        if (rootTask == null) {
            throw new IllegalArgumentException("moveRootTaskToTaskDisplayArea: Unknown rootTaskId="
                    + rootTaskId);
        }

        final TaskDisplayArea currentTaskDisplayArea = rootTask.getDisplayArea();
        if (currentTaskDisplayArea == null) {
            throw new IllegalStateException("moveRootTaskToTaskDisplayArea: rootTask=" + rootTask
                    + " is not attached to any task display area.");
        }

        if (taskDisplayArea == null) {
            throw new IllegalArgumentException(
                    "moveRootTaskToTaskDisplayArea: Unknown taskDisplayArea=" + taskDisplayArea);
        }

        if (currentTaskDisplayArea == taskDisplayArea) {
            throw new IllegalArgumentException("Trying to move rootTask=" + rootTask
                    + " to its current taskDisplayArea=" + taskDisplayArea);
        }
        rootTask.reparent(taskDisplayArea, onTop);//这里就是核心，把task重新挂载到了新display的taskDisplayArea

        // Resume focusable root task after reparenting to another display area.
        rootTask.resumeNextFocusAfterReparent();

        // TODO(multi-display): resize rootTasks properly if moved from split-screen.
    }
```

其实实现也很简单，就是吧task容器重新挂载到新的display的TaskDisplayArea

下面来实际调用一下这个方法的实际效果：  
这里其实不需要写代码，可以直接调用am display相关的命令即可以实现这个方法的调用。  
可以用am -h看看相关的命令帮助提示，有一个display参数：

```
  display [COMMAND] [...]: sub-commands for operating on displays.
       move-stack <STACK_ID> <DISPLAY_ID>
           Move <STACK_ID> from its current display to <DISPLAY_ID>.

```

使用方式就是：  
am display move\-stack taskid displayId

那么这里的taskid就是activity所在roottask的id，我们可以通过am stack list看到：

```
255|emulator_x86_64:/ # am stack list
RootTask id=420 bounds=[0,0][1440,2960] displayId=0 userId=0
 configuration={1.0 310mcc260mnc [en_US] ldltr sw411dp w411dp h797dp 560dpi nrml long port finger qwerty/v/v dpad/v winConfig={ mBounds=Rect(0, 0 - 1440, 2960) mAppBounds=Rect(0, 0 - 1440, 2876) mMaxBounds=Rect(0, 0 - 1440, 2960) mDisplayRotation=ROTATION_0 mWindowingMode=fullscreen mDisplayWindowingMode=fullscreen mActivityType=standard mAlwaysOnTop=undefined mRotation=ROTATION_0}-455144485 s.24 fontWeightAdjustment=0}
  taskId=420: com.android.gallery3d/com.android.gallery3d.app.GalleryActivity bounds=[0,0][1440,2960] userId=0 visible=true topActivity=ComponentInfo{com.android.gallery3d/com.android.gallery3d.app.GalleryActivity}

RootTask id=1 bounds=[0,0][1440,2960] displayId=0 userId=0
 configuration={1.0 310mcc260mnc [en_US] ldltr sw411dp w411dp h797dp 560dpi nrml long port finger qwerty/v/v dpad/v winConfig={ mBounds=Rect(0, 0 - 1440, 2960) mAppBounds=Rect(0, 0 - 1440, 2876) mMaxBounds=Rect(0, 0 - 1440, 2960) mDisplayRotation=ROTATION_0 mWindowingMode=fullscreen mDisplayWindowingMode=fullscreen mActivityType=home mAlwaysOnTop=undefined mRotation=ROTATION_0}-454220964 s.24 fontWeightAdjustment=0}
  taskId=419: com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher bounds=[0,0][1440,2960] userId=0 visible=false topActivity=ComponentInfo{com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher}

RootTask id=3 bounds=[0,0][1440,2960] displayId=0 userId=0
 configuration={1.0 310mcc260mnc [en_US] ldltr sw411dp w411dp h797dp 560dpi nrml long port finger qwerty/v/v dpad/v winConfig={ mBounds=Rect(0, 0 - 1440, 2960) mAppBounds=Rect(0, 0 - 1440, 2876) mMaxBounds=Rect(0, 0 - 1440, 2960) mDisplayRotation=ROTATION_0 mWindowingMode=fullscreen mDisplayWindowingMode=fullscreen mActivityType=undefined mAlwaysOnTop=undefined mRotation=ROTATION_0}-456068006 s.24 fontWeightAdjustment=0}
  taskId=3: unknown bounds=[0,0][1440,2960] userId=0 visible=false

RootTask id=4 bounds=[0,0][1440,2960] displayId=0 userId=0
 configuration={1.0 310mcc260mnc [en_US] ldltr sw411dp w411dp h797dp 560dpi nrml long port finger qwerty/v/v dpad/v winConfig={ mBounds=Rect(0, 0 - 1440, 2960) mAppBounds=Rect(0, 0 - 1440, 2876) mMaxBounds=Rect(0, 0 - 1440, 2960) mDisplayRotation=ROTATION_0 mWindowingMode=fullscreen mDisplayWindowingMode=fullscreen mActivityType=undefined mAlwaysOnTop=undefined mRotation=ROTATION_0}-456068006 s.24 fontWeightAdjustment=0}
  taskId=5: unknown bounds=[0,0][1440,2960] userId=0 visible=false
  taskId=6: unknown bounds=[0,0][1440,2960] userId=0 visible=false


```

可以看出com.android.gallery3d/com.android.gallery3d.app.GalleryActivity这Activity的对应RootTask id为：  
RootTask id=420

dipslayId那就是屏幕的id，这个也可以通过如下命令获取：

```
test@test:~/Downloads$ adb shell dumpsys display | grep mDisplayId
adb server version (41) doesn't match this client (39); killing...
* daemon started successfully
    mDisplayId=0
    mDisplayId=2
  mDisplayId=0
  mDisplayId=2

```

一般默认屏幕就是0

所以这里如果要把com.android.gallery3d/com.android.gallery3d.app.GalleryActivity移动到display2命令如下：

```
am display move-stack 420 2
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d7e3ea58ceaa2e67d907de42ce74ba91.gif#pic_center)

是不是明显看出来图库从屏幕1转到了屏幕2，屏幕1也有点闪黑  
好，那我们下一节再进行具体代码实现，进行手势触摸实现  
ps：需要相关代码和资料请加入下面微信公众号哈