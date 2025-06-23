## 前言

从 Android 7.0 开始，Google 推出了一个名为“多窗口模式”的新功能，允许在设备屏幕上同时显示多个应用，多窗口模式允许多个应用同时共享同一屏幕，多窗口模式（Multi Window Supports）目前支持以下三种配置：

分屏模式：让系统可以左右或上下并排显示应用。

画中画模式：在应用中用小窗口叠加显示其他应用

自由窗口模式：在可移动且可调整大小的单独窗口中显示各个应用。

本系列文章我们主要来分析一下自由窗口模式相关的知识点。

## 通过最近任务进入自由窗口模式

打开Activity，然后进入最近任务触发自由窗口，可以成功进入自由窗口模式，具体可以参考下图。![最近任务进入自由窗口](https://i-blog.csdnimg.cn/direct/47f0aa2f345e402b860f6b8df45ff59a.png)  
默认情况下在最近任务列表中点击应用图标，弹出的菜单列表中是没有自由窗口这个选项的，可以通过以下两种方式开启。

1、进入开发者模式页面进行开关开启  
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/db4c52e1bb074b17b3b60d7aa0c09104.png)  
2、通过adb指令开启

```java
adb shell settings put global force_resizable_activities 1 //强制应用支持自由缩放
adb shell settings put global enable_freeform_support 1 //开启自由窗口功能
adb shell am restart  或者 adb reboot
```

其实开发者模式页面的两个开关，本质也是修改force\_resizable\_activities和enable\_freeform\_support这两个数据库字段。

## adb指令进入自由窗口模式

```java
adb shell am start-activity --windowingMode 5 <package/activity>
```

## 全屏/自由窗口层级树信息对比

我使用的安卓模拟器屏幕尺寸为1080x2400，通过adb shell dumpsys activity containers指令可以将Activity的层级树信息导出来，这里我们分别导出全屏模式和自由窗口模式下页面层级树的信息，看看二者有什么不同。

### 全屏模式层级树信息

全屏模式下层级树的DefaultTaskDisplayArea节点信息如下所示。  
![全屏模式](https://i-blog.csdnimg.cn/direct/aa3326c600a3460594e9849f43ed22d0.png)

```java
       #1 DefaultTaskDisplayArea type=undefined mode=fullscreen override-mode=fullscreen requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #3 Task=18 type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 ActivityRecord{a200ce1 u0 com.example.myapplication/.MainActivity t18} type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
          #0 ac18fc1 com.example.myapplication/com.example.myapplication.MainActivity type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #2 Task=1 type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 Task=10 type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
          #0 ActivityRecord{918f596 u0 com.android.launcher3/.uioverrides.QuickstepLauncher t10} type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
           #0 c296ee2 com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #1 Task=8 type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 ActivityRecord{6fa0b29 u0 com.android.dialer/.main.impl.MainActivity t8} type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
          #0 c7c5ed4 com.android.dialer/com.android.dialer.main.impl.MainActivity type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 Task=2 type=undefined mode=fullscreen override-mode=fullscreen requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #1 Task=4 type=undefined mode=multi-window override-mode=multi-window requested-bounds=[0,2400][1080,3600] bounds=[0,2400][1080,3600]
         #0 Task=3 type=undefined mode=multi-window override-mode=multi-window requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
```

在全屏模式下MainActivity的相关信息如下：

+   根Task的mode=fullscreen(全屏模式)，bounds=\[0,0\]\[1080,2400\]
+   父节点ActivityRecord的mode=fullscreen(全屏模式)，bounds=\[0,0\]\[1080,2400\]
+   MainActivity的mode=fullscreen(全屏模式)，bounds=\[0,0\]\[1080,2400\]

### 自由窗口模式层级树信息

自由窗口模式下层级树的DefaultTaskDisplayArea节点信息如下所示。  
![自由窗口](https://i-blog.csdnimg.cn/direct/f65f18e0890842e787b988ef571b8665.png)

```java
       #1 DefaultTaskDisplayArea type=undefined mode=fullscreen override-mode=fullscreen requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #3 Task=18 type=standard mode=freeform override-mode=freeform requested-bounds=[50,113][590,1313] bounds=[50,113][627,1313]
         #0 ActivityRecord{a200ce1 u0 com.example.myapplication/.MainActivity t18} type=standard mode=freeform override-mode=undefined requested-bounds=[0,0][0,0] bounds=[50,113][627,1313]
          #0 62f1d77 com.example.myapplication/com.example.myapplication.MainActivity type=standard mode=freeform override-mode=undefined requested-bounds=[0,0][0,0] bounds=[50,113][627,1313]
        #2 Task=1 type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 Task=10 type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
          #0 ActivityRecord{918f596 u0 com.android.launcher3/.uioverrides.QuickstepLauncher t10} type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
           #0 c296ee2 com.android.launcher3/com.android.launcher3.uioverrides.QuickstepLauncher type=home mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #1 Task=8 type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #0 ActivityRecord{6fa0b29 u0 com.android.dialer/.main.impl.MainActivity t8} type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
          #0 c7c5ed4 com.android.dialer/com.android.dialer.main.impl.MainActivity type=standard mode=fullscreen override-mode=undefined requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
        #0 Task=2 type=undefined mode=fullscreen override-mode=fullscreen requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
         #1 Task=4 type=undefined mode=multi-window override-mode=multi-window requested-bounds=[0,2400][1080,3600] bounds=[0,2400][1080,3600]
         #0 Task=3 type=undefined mode=multi-window override-mode=multi-window requested-bounds=[0,0][0,0] bounds=[0,0][1080,2400]
```

在自由窗口模式下MainActivity的相关信息如下：

+   根Task的mode=freeform(自由模式)，bounds=\[50,113\]\[627,1313\]
+   父节点ActivityRecord的mode=freeform(自由模式)，bounds=\[50,113\]\[627,1313\]
+   MainActivity的mode=freeform(全屏模式)，bounds=\[50,113\]\[627,1313\]

## 自由窗口模式SurfaceFlinger信息可见图层信息

```java
Display 4619827259835644672 (active) HWC layers:
---------------------------------------------------------------------------------------------------------------------------------------------------------------
 Layer name
           Z |  Window Type |  Comp Type |  Transform |   Disp Frame (LTRB) |          Source Crop (LTRB) |     Frame Rate (Explicit) (Seamlessness) [Focused]
---------------------------------------------------------------------------------------------------------------------------------------------------------------
 Wallpaper BBQ wrapper#63
  rel      0 |            0 |     DEVICE |          0 |    0    0 1080 2400 |   21.0   47.0  440.0  977.0 |                                              [ ]
---------------------------------------------------------------------------------------------------------------------------------------------------------------
 com.android.launcher3/com.android.la[...]3.uioverrides.QuickstepLauncher#1179
  rel      0 |            1 |     DEVICE |          0 |    0    0 1080 2400 |    0.0    0.0 1080.0 2400.0 |                                              [ ]
---------------------------------------------------------------------------------------------------------------------------------------------------------------
 com.example.myapplication/com.example.myapplication.MainActivity#1192
  rel      0 |            1 |     DEVICE |          0 |   50  113  627 1313 |    0.0    0.0  577.0 1200.0 |                                              [*]
---------------------------------------------------------------------------------------------------------------------------------------------------------------
 Caption of Task=42#1199
  rel      0 |            0 |     DEVICE |          0 |   50  113  627  223 |    0.0    0.0  577.0  110.0 |                                              [ ]
---------------------------------------------------------------------------------------------------------------------------------------------------------------
 StatusBar#80
  rel      0 |         2000 |     DEVICE |          0 |    0    0 1080   63 |    0.0    0.0 1080.0   63.0 |                                              [ ]
---------------------------------------------------------------------------------------------------------------------------------------------------------------
 NavigationBar0#76
  rel      0 |         2019 |     DEVICE |          0 |    0 2274 1080 2400 |    0.0    0.0 1080.0  126.0 |                                              [ ]
---------------------------------------------------------------------------------------------------------------------------------------------------------------
```

+   Caption of Task=42#1199 自由窗口顶部的菜单栏  
    ![自由窗口菜单栏](https://i-blog.csdnimg.cn/direct/1ae2a94b952a453bb40b4f8dcf42f07f.png)

## 总结

经过对比可以发现

+   当应用Activity处于全屏模式的时候，Activity，父节点ActivityRecord，根Task全部为fullscreen(全屏模式)，根Task直接挂载在DefaultTaskDisplayArea节点上。
+   而当应用Activity处于自由窗口模式的时候，Activity，父节点ActivityRecord，根Task全部为freeform(自由模式)，该Task直接挂载在DefaultTaskDisplayArea叶子节点上。
+   当处于自由模式的时候，系统会为自由窗口添加一个名为Caption of Task的顶部菜单栏视图。

理解了这些，后面我们再结合Android系统任务栈的特性，才能够更好的理解分屏模式框架的实现思路。