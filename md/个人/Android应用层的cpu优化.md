---
tags:
  - 性能优化
---



我们作为打工人，最喜欢，也最希望的，其实就是钱多事少离家近，，

其实 对于 cpu 也是一样，它也喜欢 “钱多事少离家近”。

怎么理解？

其实，

钱多：就是cpu 能够拥有更多的资源（这里就是功率，电）

事少：就是 算法精简，计算步奏少，重复低，复用高，唤醒率低。

离家近：就是cpu尽量直接去做他需要做的事情，不需要其他去中转。

当然，我们作为应用层，钱多和离家近这两项，是没法干预了。

我们能做的，就是尽量让 cpu “**事少**”

下面，我们就分门别类，慢慢看，怎么让cpu事少

**1 算法和数据处理相关优化**

选择**合适的算法和数据结构**可以显著减少 CPU 的使用。

**1.1 减少循环嵌套**

多层循环嵌套会使时间复杂度呈指数级增长，增加 CPU 的计算负担。可以通过优化算法来减少循环嵌套。例如，将双重循环转换为单循环：

// 优化前

for (int i = 0; i < array1.length; i++) {

    for (int j = 0; j < array2.length; j++) {

        if (array1[i] == array2[j]) {

            // 处理逻辑

        }

    }

}

// 优化后，使用 HashMap 减少嵌套

HashMap<Integer, Boolean> map = new HashMap<>();

for (int num : array2) {

    map.put(num, true);

}

for (int num : array1) {

    if (map.containsKey(num)) {

        // 处理逻辑

    }

}

这里，我们简单说一下，

假设 array1 长度是100，array2 长度也是100.

那么优化前 就需要 执行 100*100 即 10000次。

而优化后，就需要执行 100+100 即 200次。（当然，实际并不是这么多次，hashmap 查找也是需要次数的，只是会比for循环按下标查找快很多，这里就不展开）

**1.2 合理使用缓存**

缓存可以避免重复计算和数据加载，减少 CPU 的负担。可以使用内存缓存（如 LruCache）或磁盘缓存（如 DiskLruCache）。

示例1：使用 LruCache 进行内存缓存

import android.graphics.Bitmap;

import android.util.LruCache;

public class ImageCache {

    private LruCache<String, Bitmap> imageCache;

    public ImageCache() {

        // 获取可用内存的 1/8 作为缓存大小

        int maxMemory = (int) (Runtime.getRuntime().maxMemory() / 1024);

        int cacheSize = maxMemory / 8;

        imageCache = new LruCache<String, Bitmap>(cacheSize) {

            @Override

            protected int sizeOf(String key, Bitmap bitmap) {

                return bitmap.getByteCount() / 1024;

            }

        };

    }

    public void addBitmapToCache(String key, Bitmap bitmap) {

        if (getBitmapFromCache(key) == null) {

            imageCache.put(key, bitmap);

        }

    }

    public Bitmap getBitmapFromCache(String key) {

        return imageCache.get(key);

    }

}

当然，这是最简化的方式，其实像 glide框架等，也都有缓存的机制。

这样，可以避免，重复加载相同图带来的cpu消耗。

实例二：

对于一些重复计算的结果，可以进行缓存，避免重复计算。例如，在计算斐波那契数列时，可以使用记忆化搜索的方法：

import java.util.HashMap;

import java.util.Map;

public class Fibonacci {

    private Map<Integer, Integer> cache = new HashMap<>();

    public int fib(int n) {

        if (n == 0 || n == 1) {

            return n;

        }

        if (cache.containsKey(n)) {

            return cache.get(n);

        }

        int result = fib(n - 1) + fib(n - 2);

        cache.put(n, result);

        return result;

    }

}

**1.3 避免使用枚举类型**

枚举类型在 Java 中虽然方便，但会占用更多的内存和 CPU 资源。

因为每个枚举常量都是一个对象，并且在类加载时会初始化所有枚举常量。

如果可以，使用 int 常量或 String 常量来替代枚举。

// 枚举类型

public enum Color {

    RED, GREEN, BLUE

}

// 替代方案，使用 int 常量

public class ColorConstants {

    public static final int RED = 1;

    public static final int GREEN = 2;

    public static final int BLUE = 3;

}

**1.4 优化字符串拼接**

在 Java 中，使用 + 进行字符串拼接时，会创建多个 String 对象，

这会增加垃圾回收的压力和 CPU 开销。

推荐使用 **StringBuilder** 或 **StringBuffer** 进行字符串拼接，

其中 StringBuilder 是非线程安全的，性能更高；

StringBuffer 是线程安全的。（本质就是加锁了）

根据 线程要求，选择合适的方案

// 不推荐

String result = "";

for (int i = 0; i < 10; i++) {

    result = result + i;

}

// 推荐

StringBuilder sb = new StringBuilder();

for (int i = 0; i < 10; i++) {

    sb.append(i);

}

String result = sb.toString();

**2 线程相关优化**

**2.1 避免在主线程进行耗时操作**

主线程（UI 线程）负责处理用户界面的绘制和交互，如果在主线程执行耗时任务，会导致界面卡顿，

甚至出现 ANR（Application Not Responding）错误。

因此，要将耗时操作放到子线程中执行。

import android.os.AsyncTask;

import android.os.Bundle;

import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    private TextView textView;

    @Override

    protected void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        textView = findViewById(R.id.textView);

        new MyAsyncTask().execute();

    }

    private class MyAsyncTask extends AsyncTask<Void, Void, String> {

        @Override

        protected String doInBackground(Void... voids) {
            // 模拟耗时操作
            try {

                Thread.sleep(3000);

            } catch (InterruptedException e) {

                e.printStackTrace();

            }

            return "耗时操作完成";

        }

        @Override

        protected void onPostExecute(String result) {

            textView.setText(result);

        }

    }

}

这里 也可以用线程池，new一个线程，然后用handler的方式实现。

**2.2 线程池的合理使用**

**频繁创建和销毁线程**会消耗大量的 CPU 资源，使用线程池可以**复用线程**，减少线程创建和销毁的开销。

Android 中可以使用 Executors 类来创建不同类型的线程池，例如：

import java.util.concurrent.ExecutorService;

import java.util.concurrent.Executors;

// 创建一个固定大小的线程池，包含 3 个线程

ExecutorService executorService = Executors.newFixedThreadPool(3);

// 提交任务到线程池

executorService.submit(new Runnable() {

    @Override

    public void run() {

        // 执行耗时任务

    }

});

// 关闭线程池

executorService.shutdown();

这也是一个简单例子，虽然Android给我们提供了线程池的更灵活的用法

使用 **ThreadPoolExecutor** 自定义线程池。

但，我们日常工作中，通常使用 四种Android给我们**预设的线程池**就可以完成所需的工作。

**1 FixedThreadPool（固定大小线程池）**

 特点：线程数量固定，**适用于控制线程最大并发数**。当线程池中的线程达到核心线程数时，新任务将在队列中等待。

 使用方法 ：通过 Executors.newFixedThreadPool(int nThreads) 创建。

 示例

ExecutorService fixedThreadPool = Executors.newFixedThreadPool(3);

fixedThreadPool.execute(new Runnable() {

    @Override

    public void run() {

        System.out.println("执行任务啦");

    }

});

fixedThreadPool.shutdown();

**2 CachedThreadPool（可缓存线程池）**

 特点：线程数量不固定，可动态增加线程数量。如果线程池的当前线程数超过了处理任务所需的线程数，多余的空闲线程会在 60 秒后被终止。

**适用于执行大量短时任务**，任务执行时间较短，且任务量不确定。

 使用方法 ：通过 Executors.newCachedThreadPool() 创建。

 示例

ExecutorService cachedThreadPool = Executors.newCachedThreadPool();

cachedThreadPool.execute(new Runnable() {

    @Override

    public void run() {

        System.out.println("执行任务啦");

    }

});

cachedThreadPool.shutdown();

**3 ScheduledThreadPool（定时线程池）**

 特点 ：类似于 FixedThreadPool，但增加了定时执行任务的功能，可以在指定时间执行任务。

**适用于需要定期执行任务的场景**，例如定时任务、周期性数据同步等。

 使用方法 ：通过 Executors.newScheduledThreadPool(int corePoolSize) 创建。

 示例

ScheduledExecutorService scheduledThreadPool = Executors.newScheduledThreadPool(3);

scheduledThreadPool.schedule(new Runnable() {

    @Override

    public void run() {

        System.out.println("执行任务啦");

    }

}, 3, TimeUnit.SECONDS);

scheduledThreadPool.shutdown();

**4 SingleThreadExecutor（单线程化线程池）**

 特点：只有一个核心线程，确保所有任务按照指定顺序执行。**适用于需要顺序执行任务的场景**，例如任务之间有依赖关系的情况。

 使用方法 ：通过 Executors.newSingleThreadExecutor() 创建。

 示例

ExecutorService singleThreadExecutor = Executors.newSingleThreadExecutor();

singleThreadExecutor.execute(new Runnable() {

    @Override

    public void run() {

        System.out.println("执行任务啦");

    }

});

singleThreadExecutor.shutdown();

**2.3 避免线程阻塞**

在子线程中要避免长时间的**阻塞**操作，

例如在网络请求时设置合理的超时时间，

防止线程一直处于等待状态。

可以使用 OkHttp 库进行网络请求，并设置超时时间：

import okhttp3.OkHttpClient;

import okhttp3.Request;

import okhttp3.Response;

OkHttpClient client = new OkHttpClient.Builder()

       .connectTimeout(10, java.util.concurrent.TimeUnit.SECONDS)

       .readTimeout(30, java.util.concurrent.TimeUnit.SECONDS)

       .build();

Request request = new Request.Builder()

       .url("https://example.com")

       .build();

try (Response response = client.newCall(request).execute()) {

    // 处理响应

} catch (Exception e) {

    e.printStackTrace();

}

当然，不止是 网络请求，，任何可能会阻塞的操作，都应该设置合理的超时机制。

**2.4 减少不必要的同步操作**

同步操作（如 **synchronized** 关键字）会带来额外的 CPU 开销，

因为它需要进行锁的获取和释放操作。

在多线程编程中，要确保只在必要的地方使用同步。

例如，**当多个线程访问共享资源时，尽量缩小同步块的范围**，避免对整个方法进行同步。

// 不推荐，同步整个方法，范围过大

public synchronized void sharedMethod() {

    // 业务逻辑

}

// 推荐，只同步必要的代码块

public void sharedMethod() {

    // 非共享资源操作

    synchronized (this) {

        // 访问共享资源的代码

    }

    // 非共享资源操作

}

**3 Android 界面绘制相关优化**

**3.1 减少不必要的绘制**

在 Android 中，视图的绘制是一个比较消耗 CPU 资源的操作。可以通过以下方法减少不必要的绘制：

1 设置 **setWillNotDraw**：如果自定义视图不需要绘制任何内容，可以调用 **setWillNotDraw(true)** 来避免不必要的绘制。

public class CustomView extends View {

    private boolean mDraw;

    public CustomView(Context context) {

        super(context);

    }

    public void setDraw(boolean draw) {

        mDraw = draw;

        setWillNotDraw(!draw);

        invalidate(); // 引发重绘

    }

    @Override

    protected void onDraw(Canvas canvas) {

        super.onDraw(canvas);

        // 自定义绘制逻辑

    }

}

2 使用 **ViewStub：ViewStub** 是一个轻量级的视图，只有在需要显示时才会进行绘制，可以用于延迟加载视图。

<ViewStub

    android:id="@+id/viewStub"

    android:layout_width="match_parent"

    android:layout_height="wrap_content"

    android:inflatedId="@+id/inflatedView"

    android:layout="@layout/layout_to_inflate" />

import android.os.Bundle;

import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    @Override

    protected void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        ViewStub viewStub = findViewById(R.id.viewStub);

        // 在需要时加载视图

        View inflatedView = viewStub.inflate();

    }

}

**3.2 优化布局层次**

复杂的**布局层次**会**增加视图测量、布局和绘制的时间**，从而消耗更多的 CPU 资源。可以通过以下方式简化布局：

 使用 **ConstraintLayout**：它是一种灵活的布局管理器，能够以扁平化的结构实现复杂的布局，减少嵌套层级。例如：

<androidx.constraintlayout.widget.ConstraintLayout

    xmlns:android="http://schemas.android.com/apk/res/android"

    xmlns:app="http://schemas.android.com/apk/res-auto"

    android:layout_width="match_parent"

    android:layout_height="match_parent">

    <TextView

        android:id="@+id/textView"

        android:layout_width="wrap_content"

        android:layout_height="wrap_content"

        android:text="Hello World!"

        app:layout_constraintStart_toStartOf="parent"

        app:layout_constraintTop_toTopOf="parent" />

</androidx.constraintlayout.widget.ConstraintLayout>

使用哪种布局方式都可以，不一定要**ConstraintLayout。只要能减少布局层次。**

 **合并布局标签**：使用 <merge> 标签可以在不增加额外视图层级的情况下合并布局，常用于自定义视图或 <include> 标签中。

<?xml version="1.0" encoding="utf-8"?>

<merge xmlns:android="http://schemas.android.com/apk/res/android">

    <TextView

        android:id="@+id/textView"

        android:layout_width="wrap_content"

        android:layout_height="wrap_content"

        android:text="Hello, Merge!"

        android:padding="16dp"/>

    <Button

        android:id="@+id/button"

        android:layout_width="wrap_content"

        android:layout_height="wrap_content"

        android:text="Click Me"

        android:layout_marginTop="8dp"/>

</merge>

<?xml version="1.0" encoding="utf-8"?>

<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"

    android:layout_width="match_parent"

    android:layout_height="match_parent"

    android:orientation="vertical">

    <include layout="@layout/custom_layout"/>

</LinearLayout>

使用merge需要注意：

1 **根节点限制** ：<merge> 标签只能作为布局文件的根节点使用，不能嵌套在其他 ViewGroup 中 。

2 **inflate 方法的使用** ：当使用 LayoutInflater.inflate 方法解析包含 <merge> 标签的布局文件时，必须指定一个父 ViewGroup，并且设置 attachToRoot 为 true 。

3 **属性无效** ：因为 <merge> 标签并不是 View，所以对 <merge> 标签设置的所有属性都是无效的 。

4 **配合 <include> 使用** ：<merge> 标签通常与 <include> 标签配合使用，以实现布局的重用和优化 。

5 **ViewStub 限制** ：ViewStub 标签中的布局不能使用 <merge> 标签 。

**3.3 避免过度绘制**

**过度绘制指的是在同一区域重复绘制多次**，这会浪费 CPU 资源。

可以通过开发者选项中的 “**显示过度绘制区域**” 功能来**检测应用中的过度绘制情况**，并进行优化。优化方法包括：

**设置背景透明**：如果某个视图不需要背景，将其背景设置为**透明**。

**避免重叠视图**：**尽量避免视图之间的重叠**，减少不必要的绘制。

**3.4 优化图像资源**

大尺寸、高分辨率的图像会占用大量内存，并且在解码和绘制时消耗 CPU 资源。可以采取以下措施优化图像资源：

 **压缩图像**：使用图像编辑工具对图片进行压缩，减小文件大小。

 **调整图像尺寸**：根据实际显示需求，调整图像的尺寸，避免加载过大的图像。

 **使用 WebP 格式**：WebP 是一种现代的图像格式，具有更好的压缩率和质量，可以在不损失太多画质的情况下减小文件大小。

**3.5 Android 动画优化**

复杂的动画效果会消耗大量的 CPU 资源。可以通过以下方法优化动画：

**使用硬件加速**：在 AndroidManifest.xml 中为应用或特定的 Activity 开启硬件加速。

<application

    android:hardwareAccelerated="true"

    ... >

    ...

</application>

**减少动画帧率**：过高的帧率会增加 CPU 负担，可以适当降低动画的帧率。

大多数人眼能够感知到的流畅动画帧速率在 60fps 左右。因此，在实现动画效果时，将帧速率设置为 60fps 即可，避免不必要地提高帧速率。

**3.6 资源懒加载**

**视图懒加载**：对于一些包含多个页面或片段的应用，采用懒加载机制。

例如，在 ViewPager 中，只有当用户滑动到某个页面时才加载该页面的资源和数据，而不是一次性加载所有页面，减少启动时的 CPU 开销。

**图片懒加载**：在列表或网格布局中显示图片时，使用图片懒加载库（如 Glide 或 Picasso）。

这些库会在图片进入可见区域时才开始加载，避免一次性加载大量图片消耗过多 CPU 资源。

**4 系统交互与唤醒优化**

**4.1 减少系统唤醒次数**

**批量处理任务**：将**多个小任务合并成一个大任务进行处理**，减少系统唤醒的次数。

例如，应用需要定时上传数据时，将多次小的数据上传操作合并成一次批量上传，降低系统唤醒的频率和 CPU 消耗。

**使用 JobScheduler 和 WorkManager**：这两个 Android 系统提供的**任务调度工具**可以根据系统状态和电量情况，智能地安排任务执行时间，避免在不合适的时间唤醒 CPU 执行任务。

JobScheduler jobScheduler = (JobScheduler) getSystemService(Context.JOB_SCHEDULER_SERVICE);

JobInfo.Builder builder = new JobInfo.Builder(0, new ComponentName(this, MyJobService.class));这个服务，就是我们干事儿的地方

builder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY);

builder.setRequiresCharging(true);

JobInfo jobInfo = builder.build();

jobScheduler.schedule(jobInfo);

JobScheduler 针对 Android 5.0 及以上版本的后台任务：

适用于需要在 Android 5.0 及以上版本设备上执行的后台任务，特别是对电量优化和系统调度支持要求较高的场景。

后台批处理任务：适合需要批量处理的后台任务，如数据同步、日志上传等。

// 创建 Constraints.Builder 对象，并设置约束条件

Constraints.Builder constraintsBuilder = new Constraints.Builder()

    .setRequiredNetworkType(NetworkType.UNMETERED)

    .setRequiresCharging(true)

    .setRequiresDeviceIdle(true)

    .setRequiresStorageNotLow(true);

// 创建 OneTimeWorkRequest.Builder 对象，并设置我们的 Worker 和约束条件

OneTimeWorkRequest.Builder workRequestBuilder = new OneTimeWorkRequest.Builder(MyWorker) 这个MyWorker 就是我们干事儿的地方

    .setConstraints(constraintsBuilder.build());

// 使用 WorkManager 调度任务

WorkManager.getInstance(context).enqueue(workRequestBuilder.build());

适用于：

兼容性要求较高的应用：适用于需要支持多种 Android 版本的应用，特别是对任务持久化和灵活约束条件有较高要求的场景。

后台任务需要持久化：如同步数据、上传日志等需要在应用退出后仍能执行的任务。

任务的实时性要求不高：如每天定时更新等任务。

![](file:///C:\Users\YT_FLY\AppData\Local\Temp\ksohtml19824\wps1.jpg) 

**4.2 优化广播接收器使用**

**静态广播优化**：静态广播在应用未启动时也能接收广播，可能会导致应用被频繁唤醒，增加 CPU 消耗。

尽量使用动态广播，只有在应用启动且需要接收广播时才进行注册。

**过滤广播信息**：在注册广播接收器时，设置合适的过滤器，

只接收应用需要的广播信息，减少不必要的广播处理，降低 CPU 负担。

**4.3 系统服务使用优化**

 **定位服务优化**

按需请求定位权限：只在应用确实需要使用定位功能时才请求定位权限，避免不必要地获取用户的位置信息，从而减少定位服务对 CPU 的消耗。

合理设置定位精度和更新间隔：根据应用的实际需求，合理设置定位的精度和更新间隔。例如，对于一些不需要实时高精度定位的应用，可以将定位精度设置为较低级别，并适当延长定位更新的间隔时间。

 **蓝牙和 Wi-Fi 服务优化**

及时关闭不必要的连接：在不需要使用蓝牙或 Wi-Fi 连接时，及时关闭连接，避免这些服务持续运行消耗 CPU 资源。

优化蓝牙数据传输：在进行蓝牙数据传输时，合理设置数据传输的速率和缓冲区大小，避免数据传输过程中出现阻塞或数据丢失的情况，提高数据传输的效率。

等等...