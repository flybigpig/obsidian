_努比亚技术团队原创内容，转载请务必注明出处。_

[Android画面显示流程分析(1)](https://www.jianshu.com/p/df46e4b39428)  
[Android画面显示流程分析(2)](https://www.jianshu.com/p/f96ab6646ae3)  
[Android画面显示流程分析(3)](https://www.jianshu.com/p/3c61375cc15b)  
[Android画面显示流程分析(4)](https://www.jianshu.com/p/7a18666a43ce)  
[Android画面显示流程分析(5)](https://www.jianshu.com/p/dcaf1eeddeb1)

## 7. 画面更新流程

在我们前面几个章节的讨论中，我们从最底层的显示硬件，SOC和DDIC的接口， linux和Userspace的图形接口以及APP与SurfaceFlinger,HWC service三者的关系，了解了帧数据流动所经过的关键节点，并重点讨论了帧buffer是如何管理的，以及在流动过程中是如何做到同步的。接下来我们将从应用侧角度来从上到下看一下应用所绘制的画面是如何使用到我们上面所设计的流程中的。

### 7.1. 画布的申请

从前文5.4的讨论可知，应用侧对图层的操作是以Surface为接口的，其定义如下所示，它包含了一些更新画面相关的核心api, 比如dequeueBuffer/queueBuffer/connect/disconnect等等。

Surface.h (frameworks\native\libs\gui\include\gui)

```cpp
class Surface
    : public ANativeObjectBase<ANativeWindow, Surface, RefBase>
{
    ......
protected:    
    virtual int dequeueBuffer(ANativeWindowBuffer** buffer, int* fenceFd);
    virtual int cancelBuffer(ANativeWindowBuffer* buffer, int fenceFd);
    virtual int queueBuffer(ANativeWindowBuffer* buffer, int fenceFd);
    virtual int perform(int operation, va_list args);
    ......
    virtual int connect(int api);
    ......
public:
    virtual int disconnect(int api,
            IGraphicBufferProducer::DisconnectMode mode =
                    IGraphicBufferProducer::DisconnectMode::Api);
}
```

那么应用要想画出它的画面，第一个要解决的问题就是应用侧的Surface对象是如何创建？ 它又是如何与SurfaceFlinger建立联系的？下面我们将从代码逻辑中找寻到它的建立过程。

在Android系统中每个Activity都有一个独立的画布（在应用侧称为Surface,在SurfaceFlinger侧称为Layer）， 无论这个Activity安排了多么复杂的view结构，它们最终都是被画在了所属Activity的这块画布上，当然也有一个例外，SurfaceView是有自已独立的画布的，但此处我们先只讨论Activity画布的建立过程。

首先每个应用都会创建有自已的Activity, 进而Android会为Activity创建一个ViewRootImpl， 并调用到它的performTraversals这个函数(篇幅所限这部分流程请读者自行阅读源码)。

该函数里会调用到relayoutWindow函数：

frameworks/base/core/java/android/view/ViewRootImpl.java

```csharp
private void performTraversals() {
    ......
    relayoutResult = relayoutWindow(params, viewVisibility, insetsPending);
    ......
}
```

relayoutWindow函数里会调用到WindowSession的relayout函数，这个函数是一个跨进程调用，mWindowSession可以看作是WMS在应用侧的代表：

frameworks/base/core/java/android/view/ViewRootImpl.java

```csharp
private int relayoutWindow(WindowManager.LayoutParams params, int viewVisibility,
            boolean insetsPending) throws RemoteException {
     ......
     int relayoutResult = mWindowSession.relayout(mWindow, mSeq, params,
                (int) (mView.getMeasuredWidth() * appScale + 0.5f),
                (int) (mView.getMeasuredHeight() * appScale + 0.5f), viewVisibility,
                insetsPending ? WindowManagerGlobal.RELAYOUT_INSETS_PENDING : 0, frameNumber,
                mTmpFrame, mTmpRect, mTmpRect, mTmpRect, mPendingBackDropFrame,
                mPendingDisplayCutout, mPendingMergedConfiguration, mSurfaceControl, mTempInsets,
                mTempControls, mSurfaceSize, mBlastSurfaceControl);
     ......
}
```

随着代码的执行让我们把视角切换到system_server进程（WMS的relayoutWindow函数）,这里会调用createSurfaceControl去创建一个SurfaceControl：

frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java

```cpp
public int relayoutWindow(Session session, IWindow client, int seq, LayoutParams attrs,
            int requestedWidth, int requestedHeight, int viewVisibility, int flags,
            long frameNumber, Rect outFrame, Rect outContentInsets,
            Rect outVisibleInsets, Rect outStableInsets, Rect outBackdropFrame,
            DisplayCutout.ParcelableWrapper outCutout, MergedConfiguration mergedConfiguration,
            SurfaceControl outSurfaceControl, InsetsState outInsetsState,
            InsetsSourceControl[] outActiveControls, Point outSurfaceSize,
            SurfaceControl outBLASTSurfaceControl) {
      ......
      try {
          result = createSurfaceControl(outSurfaceControl, outBLASTSurfaceControl,
          result, win, winAnimator);
      } catch (Exception e) {
      ......
}
```

SurfaceControl的创建过程，注意这里创建工作是调用winAnimator来完成的，注意下面那句surfaceController.getSurfaceControl会把创建出来的SurfaceControl通过形参outSurfaceControl传出去：

frameworks/base/services/core/java/com/android/server/wm/WindowManagerService.java

```csharp
private int createSurfaceControl(SurfaceControl outSurfaceControl,
            SurfaceControl outBLASTSurfaceControl, int result,
            WindowState win, WindowStateAnimator winAnimator) {
    ......
    WindowSurfaceController surfaceController;
    try {
        Trace.traceBegin(TRACE_TAG_WINDOW_MANAGER, "createSurfaceControl");
        surfaceController = winAnimator.createSurfaceLocked(win.mAttrs.type, win.mOwnerUid);
    } finally {
        Trace.traceEnd(TRACE_TAG_WINDOW_MANAGER);
    }
    if (surfaceController != null) {
        surfaceController.getSurfaceControl(outSurfaceControl);
        ......
    }
    ......
}
```

我们先看下创建过程，创建了一个WindowSurfaceController，进而再创建SurfaceControll:

frameworks/base/services/core/java/com/android/server/wm/WindowStateAnimator.java

```cpp
WindowSurfaceController createSurfaceLocked(int windowType, int ownerUid) {
    ......
    /*WindowSurfaceController mSurfaceController;*/
    mSurfaceController = new WindowSurfaceController(attrs.getTitle().toString(), width,
                    height, format, flags, this, windowType, ownerUid);
    ......
}
```

WindowSurfaceController.java (frameworks\base\services\core\java\com\android\server\wm)

```dart
 WindowSurfaceController(String name, int w, int h, int format,
            int flags, WindowStateAnimator animator, int windowType, int ownerUid) {
     ......
     final SurfaceControl.Builder b = win.makeSurface()
                .setParent(win.getSurfaceControl())
                .setName(name)
                .setBufferSize(w, h)
                .setFormat(format)
                .setFlags(flags)
                .setMetadata(METADATA_WINDOW_TYPE, windowType)
                .setMetadata(METADATA_OWNER_UID, ownerUid)
                .setCallsite("WindowSurfaceController");
     ......
}
```

SurfaceControl.java (frameworks\base\core\java\android\view)

```java
public static final @android.annotation.NonNull Creator<SurfaceControl> CREATOR
            = new Creator<SurfaceControl>() {
        public SurfaceControl createFromParcel(Parcel in) {
            return new SurfaceControl(in);
        }

        public SurfaceControl[] newArray(int size) {
            return new SurfaceControl[size];
        }
    };
```

```csharp
private SurfaceControl(SurfaceSession session, String name, int w, int h, int format, int flags,
            SurfaceControl parent, SparseIntArray metadata, WeakReference<View> localOwnerView,
            String callsite){
    ......
    mNativeObject = nativeCreate(session, name, w, h, format, flags,
                    parent != null ? parent.mNativeObject : 0, metaParcel);
    ......
}
            
```

到这里我们看到会通过JNI去创建C层的对象：

android_view_SurfaceControl.cpp (frameworks\base\core\jni)

```cpp
static jlong nativeCreate(JNIEnv* env, jclass clazz, jobject sessionObj,
        jstring nameStr, jint w, jint h, jint format, jint flags, jlong parentObject,
        jobject metadataParcel) {
    ScopedUtfChars name(env, nameStr);//Surface名字， 在SurfaceFlinger侧就是Layer的名字
    ......
    sp<SurfaceComposerClient> client;
    ......
    status_t err = client->createSurfaceChecked(
            String8(name.c_str()), w, h, format, &surface, flags, parent, std::move(metadata));
    ......
}
```

C层的Surface在创建时去调用SurfaceComposerClient的createSurface去创建, 这个SurfaceComposerClient可以看作是SurfaceFlinger在Client端的代表

android_view_SurfaceControl.cpp (frameworks\base\core\jni）

```cpp
status_t SurfaceComposerClient::createSurfaceChecked(const String8& name, uint32_t w, uint32_t h,
                                                     PixelFormat format,
                                                     sp<SurfaceControl>* outSurface, uint32_t flags,
                                                     SurfaceControl* parent, LayerMetadata metadata,
                                                     uint32_t* outTransformHint) {
      ......
      err = mClient->createSurface(name, w, h, format, flags, parentHandle, std::move(metadata),
                                     &handle, &gbp, &transformHint);
      ......
｝
```

SurfaceComposerClient.cpp (frameworks\native\libs\gui)

```cpp
sp<SurfaceControl> SurfaceComposerClient::createSurface(const String8& name, uint32_t w, uint32_t h,
                                                        PixelFormat format, uint32_t flags,
                                                        SurfaceControl* parent,
                                                        LayerMetadata metadata,
                                                        uint32_t* outTransformHint) {
    sp<SurfaceControl> s;
    createSurfaceChecked(name, w, h, format, &s, flags, parent, std::move(metadata),
                         outTransformHint);
    return s;
}
status_t SurfaceComposerClient::createSurfaceChecked(const String8& name, uint32_t w, uint32_t h,
                                                     PixelFormat format,
                                                     sp<SurfaceControl>* outSurface, uint32_t flags,
                                                     SurfaceControl* parent, LayerMetadata metadata,
                                                     uint32_t* outTransformHint) {
      .......
      err = mClient->createSurface(name, w, h, format, flags, parentHandle, std::move(metadata),
                                     &handle, &gbp, &transformHint);
      .......
}

```

跨进程呼叫SurfaceFlinger：

ISurfaceComposerClient.cpp (frameworks\native\libs\gui)

```cpp
status_t createSurface(const String8& name, uint32_t width, uint32_t height, PixelFormat format,
                           uint32_t flags, const sp<IBinder>& parent, LayerMetadata metadata,
                           sp<IBinder>* handle, sp<IGraphicBufferProducer>* gbp,
                           uint32_t* outTransformHint) override {
     return callRemote<decltype(&ISurfaceComposerClient::createSurface)>(Tag::CREATE_SURFACE,
                               name, width, height,
                               format, flags, parent,
                               std::move(metadata),
                               handle, gbp,
                               outTransformHint);
}
```

然后流程就来到了SurfaceFlinger进程，由于SurfaceFlinger支持很多不同类型的Layer, 这里我们只以BufferQueueLayer为例， 当SurfaceFlinger收到这个远程调用后会new 一个BufferQueueLayer出来。

SurfaceFlinger.cpp (frameworks\native\services\surfaceflinger)

```cpp
status_t SurfaceFlinger::createLayer(const String8& name, const sp<Client>& client, uint32_t w,
                                     uint32_t h, PixelFormat format, uint32_t flags,
                                     LayerMetadata metadata, sp<IBinder>* handle,
                                     sp<IGraphicBufferProducer>* gbp,
                                     const sp<IBinder>& parentHandle, const sp<Layer>& parentLayer,
                                     uint32_t* outTransformHint) {
      ......
      case ISurfaceComposerClient::eFXSurfaceBufferQueue:
            result = createBufferQueueLayer(client, std::move(uniqueName), w, h, flags,
                                            std::move(metadata), format, handle, gbp, &layer);
      ......
}
```

至此完成了从一个应用的Activity创建到SurfaceFlinger内部为它创建了一个Layer来对应。那么应用侧又是如何拿到这个Layer的操作接口呢？

续续看代码：

注意到WMS的createSurfaceControl方法中是通过getSurfaceControl将SurfaceControll传出来的：

frameworks/base/services/core/java/com/android/server/wm/WindowSurfaceController.java

```cpp
void getSurfaceControl(SurfaceControl outSurfaceControl) {
    outSurfaceControl.copyFrom(mSurfaceControl, "WindowSurfaceController.getSurfaceControl");
}
```

这个对象会通过WindowSession跨进程传到应用进程里，进而创建出Surface:

frameworks/base/core/java/android/view/ViewRootImpl.java

```csharp
private int relayoutWindow(WindowManager.LayoutParams params, int viewVisibility,
            boolean insetsPending) throws RemoteException {
     ......
     int relayoutResult = mWindowSession.relayout(mWindow, mSeq, params,
                (int) (mView.getMeasuredWidth() * appScale + 0.5f),
                (int) (mView.getMeasuredHeight() * appScale + 0.5f), viewVisibility,
                insetsPending ? WindowManagerGlobal.RELAYOUT_INSETS_PENDING : 0, frameNumber,
                mTmpFrame, mTmpRect, mTmpRect, mTmpRect, mPendingBackDropFrame,
                mPendingDisplayCutout, mPendingMergedConfiguration, mSurfaceControl, mTempInsets, //这里的mSurfaceControl是从WMS传来的
                mTempControls, mSurfaceSize, mBlastSurfaceControl);
     ......
     if (mSurfaceControl.isValid()) {
            if (!useBLAST()) {
                mSurface.copyFrom(mSurfaceControl);//通过Surface的copyFrom方法从SurfaceControl来创建这个Surface
            }
     ......
}
```

那么拿到Surface对象后怎么就能拿到帧缓冲区的操作接口呢？我们来看Surface的实现：

Surface.java (frameworks\base\core\java\android\view)

```java
public class Surface implements Parcelable {
    ......
    private static native long nativeLockCanvas(long nativeObject, Canvas canvas, Rect dirty)
            throws OutOfResourcesException;
    private static native void nativeUnlockCanvasAndPost(long nativeObject, Canvas canvas);
    ......    
}
```

这里这个java层的Surface实现有两个方法，一个是nativeLockCanvas一个是nativeUnlockCanvasAndPost， 它其实对应了我们在章节5.4中所述的dequeueBuffer和queueBuffer, 我们继续从代码上看下它的实现过程：

android_view_Surface.cpp (frameworks\base\core\jni)

```cpp
static jlong nativeLockCanvas(JNIEnv* env, jclass clazz,
        jlong nativeObject, jobject canvasObj, jobject dirtyRectObj) {
    ......
     status_t err = surface->lock(&buffer, dirtyRectPtr);
    ......
}
```

Surface.cpp (frameworks\native\libs\gui)

```cpp
status_t Surface::lock(
        ANativeWindow_Buffer* outBuffer, ARect* inOutDirtyBounds){
     ......
     status_t err = dequeueBuffer(&out, &fenceFd);
     ......
}
int Surface::dequeueBuffer(android_native_buffer_t** buffer, int* fenceFd) {
    ATRACE_CALL();
    ......
    status_t result = mGraphicBufferProducer->dequeueBuffer(&buf, &fence, reqWidth, reqHeight,
                                                            reqFormat, reqUsage, &mBufferAge,
                                                            enableFrameTimestamps ? &frameTimestamps
                                                                                  : nullptr);
    ......
}
```

到这里我们看到java层Surface的lock方法最终它有去调用到GraphicBufferProducer的dequeueBuffer函数，在第5章中我们有详细了解过dequeueBuffer在获取一个Slot后，如果Slot没有分配GraphicBuffer会在这时给它分配GraphicBuffer, 然后会返回一个带有BUFFER_NEEDS_REALLOCATION标记的flag, 应用侧看到这个flag后会通过requestBuffer和importBuffer接口把GraphicBuffer映射到自已的进程空间。

到此应用拿到了它绘制界面所需的“画布”。

由于上面的代码过程看起来比较繁琐，我们用一张图来概述一下这个流程：

![](https://upload-images.jianshu.io/upload_images/26874665-4bf3b17b6ab030d4.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210919115648328.png

首先在这个过程中涉及到三个进程，APP，system_server, surfaceflinger, 先从应用调用performTraversals中调用relayoutWindow这个函数开始，它跨进程调用到了system_server进程中的WMS模块，这个模块的relayoutWindow又经一系列过程创建一个SurfaceContorl, 在SurfaceControl创建过程中会跨进程调用SurfaceFlinger让它创建一个Layer出来。 之后SurfaceControll对象会跨进程通过参数回传给应用，应用根据SurfaceControl创建出应用侧的Surface对象，而Surface对象通过一些api封装向上层提供拿画布（dequeueBuffer）和提交画布（queueBuffer）的操作接口。这样应用完成了对画布的申请操作。

### 7.2. 帧数据的绘制过程

上一节我们知道了应用是如何拿到“画布”的，接下来我们来看下应用是如何在绘画完一帧后来提交数据的：

上节中应用的主线程在performTraversals函数中获取到了操作帧缓冲区的Surface对象，这个Surface对象会通过RenderProxy传递给RenderThread, 一些关健代码如下：

performTraversals里初始化RenderThread时会把Surface对象传过去：

```csharp
private void performTraversals() {
    ......
    if (mAttachInfo.mThreadedRenderer != null) {
    try {
        hwInitialized = mAttachInfo.mThreadedRenderer.initialize(mSurface);
        ........
    } 
    ......
}
```

在ThreadedRenderer的初始化中调用了setSurface，这个setSurface函数会通过JNI调到native层：

ThreadedRenderer.java (frameworks\base\core\java\android\view)

```java
boolean initialize(Surface surface) throws OutOfResourcesException {
    ......
    setSurface(surface);
    ......
}
```

android_graphics_HardwareRenderer.cpp (frameworks\base\libs\hwui\jni)

```cpp
static void android_view_ThreadedRenderer_setSurface(JNIEnv* env, jobject clazz,
        jlong proxyPtr, jobject jsurface, jboolean discardBuffer) {
    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);
    ......
    proxy->setSurface(window, enableTimeout);
    ......
}
```

最终可以看到setSurface是通过RenderProxy这个对象向RenderThread的消息队列中post了一个消息，在这个消息的处理中会调用Context的setSurface, 这里的mContext是CanvasContext.

RenderProxy.cpp (frameworks\base\libs\hwui\renderthread)

```cpp
void RenderProxy::setSurface(ANativeWindow* window, bool enableTimeout) {
    ANativeWindow_acquire(window);
    mRenderThread.queue().post([this, win = window, enableTimeout]() mutable {
        mContext->setSurface(win, enableTimeout);
        ANativeWindow_release(win);
    });
}
```

CanvasContext的setSurface会进一步调用到Surface对象的connect方法（5.4节）和SurfaceFlinger侧协商同步一些参数。

上述过程如下图所示

![](https://upload-images.jianshu.io/upload_images/26874665-53935d45f619480b.png?imageMogr2/auto-orient/strip|imageView2/2/w/776/format/webp)

image-20210921100926294.png

这其中一些关键过程可以在systrace中看到，如下图所示：

![](https://upload-images.jianshu.io/upload_images/26874665-a260c42937cc870c.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210920180755077.png

接下来应用主线程收到vsync信号后会开始绘图流程，应用主线程会遍历ViewTree对所有的View完成measure, layout, draw的工作，我们知道Android的应用界面是由很多View按树状结构组织起来的，如下图以微信主界面为例，但无论界面多么复杂它都有一个根View叫DecorView, 而纷繁复杂的界面最终都是由一个个View通过树形结构组织起来的。

![](https://upload-images.jianshu.io/upload_images/26874665-18289a2a198aecb7.png?imageMogr2/auto-orient/strip|imageView2/2/w/712/format/webp)

image-20210922173743472.png

限于篇幅我们这里不讨论UI线程的measure和layout部分，这里只来看下draw部分：

首先App每次开始绘画都是收到一个vsync信号才会开始绘图（这里暂不讨论SurfaceView自主上帧的情况），应用是通过Choreographer来感知vsync信号, 在ViewRootImpl里向Choreographer注册一个callback, 每当有vsync信号来时会执行mTraversalRunnable:

ViewRootImpl.java (frameworks\base\core\java\android\view)

```java
 void scheduleTraversals() {
     if (!mTraversalScheduled) {
         ......
         mChoreographer.postCallback(
         Choreographer.CALLBACK_TRAVERSAL, mTraversalRunnable, null);//注册vsync的回调
         ......
     }
 }
 final class TraversalRunnable implements Runnable {
     @Override
     public void run() {
        doTraversal();//每次vsync到时调用该函数
     }
 }
```

而doTraversal()主要是调用performTraversals()这个函数，performTraversals里会调用到draw()函数：

```java
 private boolean draw(boolean fullRedrawNeeded) {
     ......
      if (!dirty.isEmpty() || mIsAnimating || accessibilityFocusDirty) {
          ......
          mAttachInfo.mThreadedRenderer.draw(mView, mAttachInfo, this);//这里传下去的mView就是DecorView
          ......
      }
     ......
 }
```

上面的draw()函数进一步调用了ThreadedRenderer的draw:

ThreadedRenderer.java (frameworks\base\core\java\android\view)

```cpp
void draw(View view, AttachInfo attachInfo, DrawCallbacks callbacks) {
    ......
    updateRootDisplayList(view, callbacks);
    ......
}
private void updateRootDisplayList(View view, DrawCallbacks callbacks) {
    Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Record View#draw()");//这里有个trace，我们可以在systrace中观察到它
    ......
    updateViewTreeDisplayList(view);
    ......
}
private void updateViewTreeDisplayList(View view) {
    ......
    view.updateDisplayListIfDirty();//这里开始调用DecorView的updateDisplayListIfDirty
    ......
}

```

接下来代码调用到DecorView的基类View.java:

View.java (frameworks\base\core\java\android\view)

```java
public RenderNode updateDisplayListIfDirty() {
    ......
     final RecordingCanvas canvas = renderNode.beginRecording(width, height);//这里开始displaylist的record
     ......
     draw(canvas);
     ......
     
}
```

上面的RecordingCanvas就是扮演一个绘图指令的记录员角色，它会将这个View通过draw函数绘制的指令以displaylist形式记录下来，那么上面的renderNode又个什么东西呢？

熟悉Web的同学一定会对DOM Tree和Render Tree不陌生，Android里的View和RenderNode是类似的概念，View代表的是实体在空间结构上的存在，而RenderNode代表它在界面呈现上的存在。这样的设计可以让存在和呈现进行分离，便于实现同一存在不同状态下呈现也不同。

在Android的设计里View会对应一个RenderNode, RenderNode里的一个重要数据结构是DisplayList, 每个DisplayList都会包含一系列DisplayListData. 这些DisplayList也会同样以树形结构组织在一起。

当UI线程完成它的绘制工作后，它工作的产物是一堆DisplayListData, 我们可以将其理解为是一堆绘图指令的集合，每一个DisplayListData都是在描绘这个View长什么样子，所以一个View树也可能理解为它的样子由对应的DisplayListData构成的树来描述：

![](https://upload-images.jianshu.io/upload_images/26874665-8d1c99902ad7386c.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210921110021839.png

我们再来看下DisplayListData是长什么样子，它定义在下面这个文件中：

RecordingCanvas.h (frameworks\base\libs\hwui)

```cpp
class DisplayListData final {
    ......
    void drawPath(const SkPath&, const SkPaint&);
    void drawRect(const SkRect&, const SkPaint&);
    void drawRegion(const SkRegion&, const SkPaint&);
    void drawOval(const SkRect&, const SkPaint&);
    void drawArc(const SkRect&, SkScalar, SkScalar, bool, const SkPaint&);
    ......
    template <typename T, typename... Args>
    void* push(size_t, Args&&...);
    ......
    SkAutoTMalloc<uint8_t> fBytes;
    ......
}
```

它的组成大体可以看成三个部分，第一部分是一堆以draw打头的函数，它们是最基本的绘图指令，比如画一条线， 画一个矩形，画一段圆弧等等，上面我们摘取了其中几个,后面我们将以drawRect为例来看它是如何工作的； 第二部分是一个push模版函数，后面我们会看到它的作用； 第三个是一块存储区fBytes，它会根据需要放大存储区的大小。

我们来看下drawRect的实现：

RecordingCanvas.cpp (frameworks\base\libs\hwui)

```cpp
void DisplayListData::drawRect(const SkRect& rect, const SkPaint& paint) {
    this->push<DrawRect>(0, rect, paint);
}
```

我们发现它只是push了画 一个Rect相关的参数，那么这个DrawRect又是什么呢？

```cpp
struct Op {
    uint32_t type : 8;
    uint32_t skip : 24;
};
```

```cpp
struct DrawRect final : Op {
    static const auto kType = Type::DrawRect;
    DrawRect(const SkRect& rect, const SkPaint& paint) : rect(rect), paint(paint) {}
    SkRect rect;
    SkPaint paint;
    void draw(SkCanvas* c, const SkMatrix&) const { c->drawRect(rect, paint); }
};
```

通过上面代码我们不难发现，DrawRect代表的是一段内存布局，这段内存第一个字节存储了它是哪种类型，后面的部分存储有画这个Rect所需要的参数信息，再来看push方法的实现：

```cpp
template <typename T, typename... Args>
void* DisplayListData::push(size_t pod, Args&&... args) {
    ......
    auto op = (T*)(fBytes.get() + fUsed);
    fUsed += skip;
    new (op) T{std::forward<Args>(args)...};
    op->type = (uint32_t)T::kType;//注意这里将DrawRect的类型编码Type::DrawRect存进了第一个字节
    op->skip = skip;
    return op + 1;
}
```

这里push方法就是在fBytes后面放入这个DrawRect的内存布局，也就是执行DisplayListData::drawRect方法时就是把画这个Rect的方法和参数存入了fBytes这块内存中， 那么最后fBytes这段内存空间就放置了一条条的绘制指令。

通过上面的了解，我们知道了UI线程并没有将应用设计的View转换成像素点数据，而是将每个View的绘图指令存入了内存中，我们通常称这些绘图指令为DisplayList, 下面让我们跳出这些细节，再次回到宏观一些的角度。

当所有的View的displaylist建立完成后，代码会来到：

RenderProxy.cpp (frameworks\base\libs\hwui\renderthread)

```cpp
int RenderProxy::syncAndDrawFrame() {
    return mDrawFrameTask.drawFrame();
}
```

DrawFrameTask.cpp (frameworks\base\libs\hwui\renderthread)

```cpp
void DrawFrameTask::postAndWait() {
    AutoMutex _lock(mLock);
    mRenderThread->queue().post([this]() { run(); });//丢任务到RenderThread线程
    mSignal.wait(mLock);
}
```

这边可以看到UI线程的工作到此结束，它丢了一个叫DrawFrameTask的任务到RenderThread线程中去，之后画面绘制的工作转移到RenderThread中来：

DrawFrameTask.cpp (frameworks\base\libs\hwui\renderthread)

```cpp
void DrawFrameTask::run() {
    .....
    context->draw();
    .....
}
```

CanvasContext.cpp (frameworks\base\libs\hwui\renderthread)

```rust
void CanvasContext::draw() {
    ......
    Frame frame = mRenderPipeline->getFrame();//这句会调用到Surface的dequeueBuffer
    ......
     bool drew = mRenderPipeline->draw(frame, windowDirty, dirty, mLightGeometry, &mLayerUpdateQueue,
                                      mContentDrawBounds, mOpaque, mLightInfo, mRenderNodes,
                                      &(profiler()));
    ......
    waitOnFences();
    ......
    bool didSwap =
            mRenderPipeline->swapBuffers(frame, drew, windowDirty, mCurrentFrameInfo, &requireSwap);//这句会调用到Surface的queueBuffer
    ......
}
```

在这个函数中完成了三个重要的动作，一个是通过getFrame调到了Surface的dequeueBuffer向SurfaceFlinger申请了画布， 第二是通过mRenderPipeline->draw将画面画到申请到的画布上， 第三是通过调mRenderPipeline->swapBuffers把画布提交到SurfaceFlinger去显示。

那么在mRenderPipeline->draw里是如何将displaylist翻译成画布上的像素点颜色的呢？

SkiaOpenGLPipeline.cpp (frameworks\base\libs\hwui\pipeline\skia)

```cpp
bool SkiaOpenGLPipeline::draw(const Frame& frame, const SkRect& screenDirty, const SkRect& dirty,
                              const LightGeometry& lightGeometry,
                              LayerUpdateQueue* layerUpdateQueue, const Rect& contentDrawBounds,
                              bool opaque, const LightInfo& lightInfo,
                              const std::vector<sp<RenderNode>>& renderNodes,
                              FrameInfoVisualizer* profiler) {
      ......
      renderFrame(*layerUpdateQueue, dirty, renderNodes, opaque, contentDrawBounds, surface, SkMatrix::I());
      ......
 }
```

SkiaPipeline.cpp (frameworks\base\libs\hwui\pipeline\skia)

```cpp
void SkiaPipeline::renderFrame(const LayerUpdateQueue& layers, const SkRect& clip,
                               const std::vector<sp<RenderNode>>& nodes, bool opaque,
                               const Rect& contentDrawBounds, sk_sp<SkSurface> surface,
                               const SkMatrix& preTransform) {
    ......
    SkCanvas* canvas = tryCapture(surface.get(), nodes[0].get(), layers);
    ......
    renderFrameImpl(clip, nodes, opaque, contentDrawBounds, canvas, preTransform);
    endCapture(surface.get());
    ......
    ATRACE_NAME("flush commands");
    surface->getCanvas()->flush();
    ......
}

```

在上面的renderFrameImpl中会把在UI线程中记录的displaylist重新“绘制”到skSurface中，然后通过SkCanvas将其转化为gl指令， surface->getCanvas()->flush();这句是将指令发送给GPU执行，这其中是如何“翻译”的细节笔者暂时尚未研究，这里先不做讨论。

总结一下应用通过android的View系统画出第一帧的总的流程，如下图所示：

![](https://upload-images.jianshu.io/upload_images/26874665-3d5f1f874e4f1898.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210920180157716.png

首先是UI线程进行measure, layout然后开始draw, 在draw的过程中会建立displaylist树，将每个view应该怎么画记录下来，然后通过RenderProxy把后续任务下达给RenderThread, RenderThread主要完成三个动作，先通过Surface接口向Surfaceflinger申请buffer, 然后通过SkiaOpenGLPipline的draw方法把displaylist翻译成GPU指令， 指挥GPU把指令变成像素点数据， 最后通过swapBuffer把数据提交给SurfaceFlinger, 完成一帧数据的绘制和提交。

这个过程我们可以在systrace上观察到，如下图所示：

![](https://upload-images.jianshu.io/upload_images/26874665-48da9162bb12884a.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210920182936703.png

![](https://upload-images.jianshu.io/upload_images/26874665-141c44029f728716.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210920181748084.png

### 7.3. 帧数据的提交过程

那么应用提交buffer以后SurfaceFlinger会如何处理呢？又是如何提交到HWC Service去合成的呢？

```undefined
    首先响应应用queueBuffer的是一条binder线程， 处理逻辑会走进：
```

BufferQueueProducer.cpp (frameworks\native\libs\gui)

```cpp
status_t BufferQueueProducer::queueBuffer(int slot,
        const QueueBufferInput &input, QueueBufferOutput *output) {
    ATRACE_CALL();
    ATRACE_BUFFER_INDEX(slot);
    ......
     if (frameAvailableListener != nullptr) {
        frameAvailableListener->onFrameAvailable(item);
     }
    ......
}
```

上面的frameAvailableListener是BufferQueueLayer：

BufferQueueLayer.cpp (frameworks\native\services\surfaceflinger)

```cpp
void BufferQueueLayer::onFrameAvailable(const BufferItem& item) {
    ......
     mFlinger->signalLayerUpdate();//这里申请一下个vsync-sf信号
    ......
}
```

由上面代码可知，只要有layer上帧，那么就会申请下一次的vsync-sf信号， 当vsync-sf信号来时会调用onMessageReceived函数来处理帧数据：

SurfaceFlinger.cpp (frameworks\native\services\surfaceflinger)

```cpp
void SurfaceFlinger::onMessageInvalidate(nsecs_t expectedVSyncTime) {
    ATRACE_CALL();
    ......
        refreshNeeded |= handleMessageInvalidate();
    ......
    signalRefresh();//再次向消息队列发送一个消息，消息到达时会调用onMessageRefresh
    ......
}
bool SurfaceFlinger::handleMessageInvalidate() {
    ATRACE_CALL();
    bool refreshNeeded = handlePageFlip();
    ......
}
```

在handleMessageInvalidate里一个比较重要的函数是handlePageFlip():

```rust
bool SurfaceFlinger::handlePageFlip()
{
    ATRACE_CALL();
    ......
    mDrawingState.traverse([&](Layer* layer) {
        if (layer->hasReadyFrame()) {
            frameQueued = true;
            if (layer->shouldPresentNow(expectedPresentTime)) {
                mLayersWithQueuedFrames.push_back(layer);
            } 
            .......
        } 
        ......
    });
    ......
        for (auto& layer : mLayersWithQueuedFrames) {
            if (layer->latchBuffer(visibleRegions, latchTime, expectedPresentTime)) {
                mLayersPendingRefresh.push_back(layer);
            }
            .......
        }
        ......
}
```

这里可以看出来，handlePageFlip里一个重要的工作是检查所有的Layer是否有新buffer提交，如果有则调用其latchBuffer来处理：

BufferLayer.cpp (frameworks\native\services\surfaceflinger)

```cpp
bool BufferLayer::latchBuffer(bool& recomputeVisibleRegions, nsecs_t latchTime,
                              nsecs_t expectedPresentTime) {
    ATRACE_CALL();
    ......
    status_t err = updateTexImage(recomputeVisibleRegions, latchTime, expectedPresentTime);
    ......
}
```

BufferQueueLayer.cpp (frameworks\native\services\surfaceflinger)

```cpp
status_t BufferQueueLayer::updateTexImage(bool& recomputeVisibleRegions, nsecs_t latchTime,
                                          nsecs_t expectedPresentTime) {
     ......
      status_t updateResult = mConsumer->updateTexImage(&r, expectedPresentTime, &mAutoRefresh,
                                                      &queuedBuffer, maxFrameNumberToAcquire);
     ......
}
```

BufferLayerConsumer.cpp (frameworks\native\services\surfaceflinger)

```cpp
status_t BufferLayerConsumer::updateTexImage(BufferRejecter* rejecter, nsecs_t expectedPresentTime,
                                             bool* autoRefresh, bool* queuedBuffer,
                                             uint64_t maxFrameNumber) {
    ATRACE_CALL();
    ......
    status_t err = acquireBufferLocked(&item, expectedPresentTime, maxFrameNumber);
    ......
}
```

这里调用到了BufferLayerConsumer的基类ConsumerBase里：

```cpp
status_t ConsumerBase::acquireBufferLocked(BufferItem *item,
        nsecs_t presentWhen, uint64_t maxFrameNumber) {
    ......
    status_t err = mConsumer->acquireBuffer(item, presentWhen, maxFrameNumber);
    ......
}
```

到这里onMessageReceived中的主要工作结束，在这个函数的处理中，SurfaceFlinger主要是检查每个Layer是否有新提交的buffer， 如果有则调用latchBuffer将每个BufferQueue中的Slot 通过acquireBuffer拿走。 之后拿走的buffer(Slot对应的状态是ACQUIRED状态)会被交由HWC Service处理，这部分是在onMessageRefresh里处理的：

```cpp
void SurfaceFlinger::onMessageRefresh() {
    ATRACE_CALL();
    ......
    mCompositionEngine->present(refreshArgs);
    ......
｝
```

CompositionEngine.cpp (frameworks\native\services\surfaceflinger\compositionengine\src)

```cpp
void CompositionEngine::present(CompositionRefreshArgs& args) {
    ATRACE_CALL();
    ......
    for (const auto& output : args.outputs) {
        output->present(args);
    }
    ......
}
```

Output.cpp (frameworks\native\services\surfaceflinger\compositionengine\src)

```cpp
void Output::present(const compositionengine::CompositionRefreshArgs& refreshArgs) {
    ATRACE_CALL();
    ......
    updateAndWriteCompositionState(refreshArgs);//告知HWC service有哪些layer要参与合成
    ......
    beginFrame();
    prepareFrame();
    ......
    finishFrame(refreshArgs);
    postFramebuffer();//这里会调用到HWC service的接口去present display合成画面
}
void Output::postFramebuffer() {
    ......
    auto frame = presentAndGetFrameFences();
    ......
}
```

```css
HWComposer.cpp (frameworks\native\services\surfaceflinger\displayhardware)
```

```cpp
status_t HWComposer::presentAndGetReleaseFences(DisplayId displayId) {
    ATRACE_CALL();
    ......
    auto error = hwcDisplay->present(&displayData.lastPresentFence);//送去HWC service合成
    ......
    std::unordered_map<HWC2::Layer*, sp<Fence>> releaseFences;
    error = hwcDisplay->getReleaseFences(&releaseFences);
    RETURN_IF_HWC_ERROR_FOR("getReleaseFences", error, displayId, UNKNOWN_ERROR);

    displayData.releaseFences = std::move(releaseFences);//获取releaseFence, 以便通知到各个Slot, buffer被release后会通过dequeueBuffer给到应用，应用在绘图前会等待releaseFence
    ......
}
```

Hwc2::Composer是HWC service提供给Surfaceflinger侧的一些接口，挑些有代表的如下

```csharp
class Composer final : public Hwc2::Composer {
    ......
    Error createLayer(Display display, Layer* outLayer) override;//通知HWC要新加一个layer
    Error destroyLayer(Display display, Layer layer) override;//通知hwc 有个layer被destory掉了
    ......
    Error presentDisplay(Display display, int* outPresentFence) override;//合成画面
    ......
    Error setActiveConfig(Display display, Config config) override;//设置一些参数，如屏幕刷新率
    ......
    CommandWriter mWriter;
    CommandReader mReader;//mWriter用于SurfaceFlinger向HWC service发送指令， mReader用于从HWC service侧获取信息
}
```

```cpp
Error Composer::presentDisplay(Display display, int* outPresentFence)
{
    ......
    mWriter.presentDisplay();
    ......
}
```

到这里 SurfaceFlinger侧的处理主流程走完了，我们先来总结一下当应用的buffer提交到SurfaceFlinger后SurfaceFlinger所经历的主要流程， 如下图所示，首先binder线程会通过BufferQueue机制把应用上帧的Slot状态改为QUEUED, 然后把这个Slot放入mQueue队列（请回忆5.3节知识）， 然后通过onFrameAvailable回调通知到BufferQueueLayer, 在处理函数里会请求下一次的vsync-sf信号，在vsync-sf信号到来后，SurfaceFlinger主线程要执行两次onMessageReceived, 第一次要检查所有的layer看是否有上帧， 如果有Layer上帧就调用它的latchBuffer把它的buffer acquireBuffer走。并发送一个消息到主消息队列，让UI线程再次走进onMessageReceived, 第二次走进来时，主要执行present方法，在这些方法里会和HWC service沟通，调用它的跨进程接口通知它去做图层的合成。

![](https://upload-images.jianshu.io/upload_images/26874665-060403340cbb1128.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210920180035351.png

之后就进入HWC Service的处理流程，这部分的处理流程和芯片厂商HAL层实现紧密相关，限于某些因素不便于介绍。本章应用更新画面的代码流程就介绍到这里。

### 7.4. 本章小结

本章我们沿着代码逻辑学习了应用是如何申请到画布、使用android的View系统如何绘图、绘图完成后如何提交buffer以及buffer提交以及Surfaceflinger如何处理。但本章所述的逻辑均是指通过android的View系统绘图的过程，也可以称其为hwui绘图流程，从上面代码流程可以知道，hwui的绘图流程是被vsync信号触发的，开始于vsync信号到达UI线程调用performTraversals函数， hwui的画面更新是被vsync信号驱动的。

在android系统中也有提供不依赖vsync信号的自主上帧接口，比如app可以使用SurfaceView这个特殊的View来申请一个独立于Activity之外的画布。接下来我们通过一些helloworld示例来看下这些接口如何使用。

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/7a18666a43ce  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。