本章节通过获取Android系统的Display，使用Display+Presentation或Display+Activity实现分屏展示。最后通过MediaProjection录屏采集主屏幕数据，通过Display+Activity+OpenGL方式实现副屏同主屏的渲染。

## 1 Display的获取

系统提供了三种方式获取要Display显示屏幕：  
**1.MediaRouter获取Display  
2.DisplayManager获取可支持Display\[\]数组  
3.DisplayManager获取所有Display\[\]数组**

### 1.1 MediaRouter获取Display

[MediaRouter官方文档](https://android-dot-google-developers.gonglchuangl.net/guide/topics/media/mediarouter?hl=zh-cn)  
通过MediaRouter服务`getSelectedRoute(MediaRouter.ROUTE_TYPE_LIVE_AUDIO)`方法获取到MediaRouter.RouteInfo对象，再通过MediaRouter.RouteInfo的`getPresentationDisplay()`获取分屏Display。

```
    fun getDisplayByMediaRouter(): Display? {
        val mediaRouter = getSystemService(MEDIA_ROUTER_SERVICE) as MediaRouter
        val localRouteInfo = mediaRouter.getSelectedRoute(MediaRouter.ROUTE_TYPE_LIVE_AUDIO)
        return localRouteInfo?.presentationDisplay
    }
```

### 1.2 DisplayManager获取可支持Display\[\]数组

通过DisplayManager的`getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);`获取所有**分屏Display**，然后获取对应分屏Display。

```
    fun getDisplayByDisplayManager(): Display? {
        val displayManager = getSystemService(DISPLAY_SERVICE) as DisplayManager
        val pDisplays = displayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)
        return if (pDisplays.size > 0) pDisplays[0] else null
    }
```

### 1.3 DisplayManager获取所有Display\[\]数组

通过DisplayManager的`getDisplays()`获取所**有屏幕Display的数组（该数组第一个是主屏Display）**，然后获取对应分屏Display。

```
    fun getDisplayByDisplayManager2(): Display? {
        val displayManager = getSystemService(DISPLAY_SERVICE) as DisplayManager
        val displays = displayManager.displays
        return if (displays.size > 1) displays[1] else null
    }
```

## 2 Display+Presentation实现分屏

上面我们已经通过三种方式获取到`Display对象`，这里我们还需要Presentation来展示在对应分屏上，该方案需要`悬浮窗权限`，因为`Presentation实际上继承自Dialog`，所以展示在分屏上的其实是一个弹窗。下面我来是实现一下：

**传入上面获取到的Display对象，通过Presentation实现分屏展示**

```

    private var presentation: MyPresentation? = null
    
    /**
     * 主屏back键/home键隐藏后，副屏仍可使用。
     * 但是，再次打开主屏，副屏会失联，所以作如下设置
     *
     * @param display
     */
    private fun showPresentation(display: Display?) {
        if (display != null) {
            presentation = null
            presentation = MyPresentation(applicationContext, display)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                presentation!!.window!!.setType(WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY)
            } else {
                presentation!!.window!!.setType(WindowManager.LayoutParams.TYPE_SYSTEM_ALERT)
            }
            presentation!!.show()
        } else {
            Toast.makeText(this@MainActivity, "不支持分屏！", Toast.LENGTH_SHORT).show()
        }
    }

    /**
     * 隐藏Presentation
     */
    private fun dismissPresentation() {
        if (presentation != null) {
            presentation!!.dismiss()
            presentation = null
        }
    }
```

MyPresentation.kt

```
package com.yvan.display

import android.app.AlertDialog
import android.app.Presentation
import android.content.Context
import android.os.Bundle
import android.view.Display
import android.view.View
import android.widget.TextView

/**
 * act和Presentation之间可以设置监听，进行互相通讯
 */
class MyPresentation(context: Context?, display: Display?) : Presentation(context, display) {
    var textView: TextView? = null
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.ly_display)
        textView = findViewById<TextView>(R.id.text)
        textView!!.text = "副屏如果支持触摸，可以点击试试结果"
        findViewById<View>(R.id.click).setOnClickListener {
            AlertDialog.Builder(context)
                    .setTitle("Dialog")
                    .setMessage("Prsentation Click Test")
                    .setCancelable(false)
                    .setPositiveButton("OK") { dialog, which -> textView!!.text = "哎呦，不错哦" }.create().show()
        }
        findViewById<View>(R.id.dismiss).setOnClickListener { dismiss() }
    }
}

```

ly\_display.xml

```
<?xml version="1.0" encoding="utf-8"?>
<RelativeLayout
    android:id="@+id/layout"
    xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent">

    <Button
        android:id="@+id/click"
        android:layout_width="200dp"
        android:layout_height="wrap_content"
android:text="在对应主屏上弹出对话框"/>

    <Button
        android:id="@+id/dismiss"
        android:layout_width="150dp"
        android:layout_height="wrap_content"
        android:layout_alignParentRight="true"
        android:text="使自己消失"/>

    <TextView
        android:id="@+id/text"
        android:layout_width="wrap_content"
        android:layout_height="wrap_content"
        android:text="1111"
        android:layout_centerInParent="true"
        android:textSize="30sp"/>

</RelativeLayout>
```

别忘了要在AndroidManifest.xml添加`浮窗权限`，在使用前动态获取权限

```
    <!--浮窗权限-->
    <uses-permission android:name="android.permission.SYSTEM_ALERT_WINDOW" />
    <uses-permission android:name="android.permission.SYSTEM_OVERLAY_WINDOW" />
```

```
    /**
     * 动态获取浮窗权限
     */
    private fun checkPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            //SYSTEM_ALERT_WINDOW权限申请
            if (!Settings.canDrawOverlays(this)) {
                val intent = Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION)
                intent.data = Uri.parse("package:$packageName") //不加会显示所有可能的app
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                startActivityForResult(intent, 1)
            } else {
                //TODO do something you need
            }
        }
    }
```

## 3 Display+Activity实现分屏

上面的Presentation是弹窗，这里把分屏Display的id通过`options.putInt("android.activity.launchDisplayId", displayId)`传入Activity，实现Activity展示在副屏上。

```
    fun startShared() {
        val options = Bundle() // 实例化Bundle
        val displayManager = getSystemService(DISPLAY_SERVICE) as DisplayManager // 获取显示管理器 并 强转类型为 显示管理器
        val displays = displayManager.displays // 从显示管理器中 获取 当前系统所有的显示屏幕
        val intent = Intent(this, ScreenActivity::class.java) // 准备一个待 激活的Activity 也就是显示第二块屏幕的 目标Activity
        if (displays != null && displays.size > 1) { // 判断屏幕大于1，意味着 有 副屏的存在，才能进入if
            options.putInt("android.activity.launchDisplayId", displays[1].displayId) // 激活Activity前期的设置，设置为第二块屏幕的ID
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK or Intent.FLAG_ACTIVITY_NEW_TASK) // 这种第二块屏幕的显示，标准Flags的设置
            startActivity(intent, options) // 激活 显示第二块屏幕的 目标Activity
        }
    }
```

## 4 OpenGL实现分屏显示

上面副屏展示有Display+Presentation和Display+Activity两种方式，现在要实现将主屏内容渲染到副屏上，这里结合前面两个章节学习的OpenGL渲染到屏幕知识，可以通过以下方案实现：  
**主屏数据采集**：`MediaProjection录屏`  
**副屏渲染**：`Display+Activity+OpenGL`  
这里是[项目地址](https://github.com/zhuyingfa/Display)，别忘了点star喔！