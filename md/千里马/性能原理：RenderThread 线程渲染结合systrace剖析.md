


性能优化相关的一些内容一直都是广大学员们一直很期待的部分，特别是和性能优化的一些实战相关，基本上网络所有的技术blog或者书籍都应该属于一个完全稀缺状态。马哥这边给大家收集了一些以前老同事们写的性能优化相关文章依旧是目前业界难以超越的经典（简书的“努比亚技术团队”），但是可惜简书目前对这些文章很多插图都没办法显示，为了让这些经典文章可以更好分享受益，我这边对文章进行相关出现格式和图文调整，进行一些经典片段摘取，这样目的是让大家阅读起来更加简单易懂，解决原来一篇文章信息量太大很多读者反馈：读起来觉得牛逼，实际到自己收获却很小，因为没办法实操，内容太多掌握不了。

## **RenderThread 线程渲染流程**

截止到目前，在ViewRootImpl中完成了对界面的measure、layout和draw等绘制流程后，用户依然还是看不到屏幕上显示的应用界面内容，因为整个Android系统的显示流程除了前面讲到的UI线程的绘制外，界面还需要经过RenderThread线程的渲染处理，渲染完成后，还需要通过Binder调用“上帧”交给surfaceflinger进程中进行合成后送显才能最终显示到屏幕上。

本小节中，我们将接上一节中ViewRootImpl中最后draw的流程继续往下分析开启硬件加速情况下，RenderThread渲染线程的工作流程。由于目前Android 4.X之后系统默认界面是开启硬件加速的，所以本文我们重点分析硬件加速条件下的界面渲染流程，我们先分析一下简化的代码流程：

```
/*frameworks/base/core/java/android/view/ViewRootImpl.java*/
private boolean draw(boolean fullRedrawNeeded) {
    ...
    if (mAttachInfo.mThreadedRenderer != null && mAttachInfo.mThreadedRenderer.isEnabled()) {
        ...
        // 硬件加速条件下的界面渲染流程
        mAttachInfo.mThreadedRenderer.draw(mView, mAttachInfo, this);
    } else {
        ...
    }
}

/*frameworks/base/core/java/android/view/ThreadedRenderer.java*/
void draw(View view, AttachInfo attachInfo, DrawCallbacks callbacks) {
    ...
    // 1.从DecorView根节点出发，递归遍历View控件树，记录每个View节点的绘制操作命令，完成绘制操作命令树的构建
    updateRootDisplayList(view, callbacks);
    ...
    // 2.JNI调用同步Java层构建的绘制命令树到Native层的RenderThread渲染线程，并唤醒渲染线程利用OpenGL执行渲染任务；
    int syncResult = syncAndDrawFrame(choreographer.mFrameInfo);
    ...
}
```

**从上面的代码可以看出**



**硬件加速绘制主要包括两个阶段：**

1、从DecorView根节点出发，递归遍历View控件树，记录每个View节点的drawOp绘制操作命令，完成绘制操作命令树的构建；

2、JNI调用同步Java层构建的绘制命令树到Native层的RenderThread渲染线程，并唤醒渲染线程利用OpenGL执行渲染任务；

#### **1 构建绘制命令树**

```
/*frameworks/base/core/java/android/view/ThreadedRenderer.java*/
private void updateRootDisplayList(View view, DrawCallbacks callbacks) {
        // 原生标记构建View绘制操作命令树过程的systrace tag
        Trace.traceBegin(Trace.TRACE_TAG_VIEW, "Record View#draw()");
        // 递归子View的updateDisplayListIfDirty实现构建DisplayListOp
        updateViewTreeDisplayList(view);
        ...
        if (mRootNodeNeedsUpdate || !mRootNode.hasDisplayList()) {
            // 获取根View的SkiaRecordingCanvas
            RecordingCanvas canvas = mRootNode.beginRecording(mSurfaceWidth, mSurfaceHeight);
            try {
                ...
                // 利用canvas缓存DisplayListOp绘制命令
                canvas.drawRenderNode(view.updateDisplayListIfDirty());
                ...
            } finally {
                // 将所有DisplayListOp绘制命令填充到RootRenderNode中
                mRootNode.endRecording();
            }
        }
        Trace.traceEnd(Trace.TRACE_TAG_VIEW);
}

private void updateViewTreeDisplayList(View view) {
        ...
        // 从DecorView根节点出发，开始递归调用每个View树节点的updateDisplayListIfDirty函数
        view.updateDisplayListIfDirty();
        ...
}

/*frameworks/base/core/java/android/view/View.java*/
public RenderNode updateDisplayListIfDirty() {
     ...
     // 1.利用`View`对象构造时创建的`RenderNode`获取一个`SkiaRecordingCanvas`“画布”；
     final RecordingCanvas canvas = renderNode.beginRecording(width, height);
     try {
         ...
         if ((mPrivateFlags & PFLAG_SKIP_DRAW) == PFLAG_SKIP_DRAW) {
              // 如果仅仅是ViewGroup，并且自身不用绘制，直接递归子View
              dispatchDraw(canvas);
              ...
         } else {
              // 2.利用SkiaRecordingCanvas，在每个子View控件的onDraw绘制函数中调用drawLine、drawRect等绘制操作时，创建对应的DisplayListOp绘制命令，并缓存记录到其内部的SkiaDisplayList持有的DisplayListData中；
              draw(canvas);
         }
     } finally {
         // 3.将包含有`DisplayListOp`绘制命令缓存的`SkiaDisplayList`对象设置填充到`RenderNode`中；
         renderNode.endRecording();
         ...
     }
     ...
}

public void draw(Canvas canvas) {
    ...
    // draw the content(View自己实现的onDraw绘制，由应用开发者自己实现)
    onDraw(canvas);
    ...
    // draw the children
    dispatchDraw(canvas);
    ...
}

/*frameworks/base/graphics/java/android/graphics/RenderNode.java*/
public void endRecording() {
        ...
        // 从SkiaRecordingCanvas中获取SkiaDisplayList对象
        long displayList = canvas.finishRecording();
        // 将SkiaDisplayList对象填充到RenderNode中
        nSetDisplayList(mNativeRenderNode, displayList);
        canvas.recycle();
}
```

从以上代码可以看出，构建绘制命令树的过程是从View控件树的根节点DecorView触发，递归调用每个子View节点的updateDisplayListIfDirty函数，最终完成绘制树的创建，简述流程如下：

1、利用View对象构造时创建的RenderNode获取一个SkiaRecordingCanvas“画布”；

2、利用SkiaRecordingCanvas，在每个子View控件的onDraw绘制函数中调用drawLine、drawRect等绘制操作时，创建对应的DisplayListOp绘制命令，并缓存记录到其内部的SkiaDisplayList持有的DisplayListData中；

3、将包含有DisplayListOp绘制命令缓存的SkiaDisplayList对象设置填充到RenderNode中；

4、最后将根View的缓存DisplayListOp设置到RootRenderNode中，完成构建。

以上整个构建绘制命令树的过程可以用如下流程图表示：![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_jpg/DYicOkJDdA2p1HYhroGJOibFrsbicP8O2kGvxzCzzNr1dT4RD9EBIEEXz4ft2lticI26Cv4G1Htd8vicRsGCkBOFhNQ/640?wx_fmt=other&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1)硬件加速下的整个界面的View树的结构如下图所示：

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_jpg/DYicOkJDdA2p1HYhroGJOibFrsbicP8O2kG3icTrtvA1udLw687rFkiaia3HfQrBDmVibIbTZQq7jQjXibLHTtblesulCw/640?wx_fmt=other&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1)

最后从Systrace上看这个过程如下图所示：

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_jpg/DYicOkJDdA2p1HYhroGJOibFrsbicP8O2kGepwXJATjsTfKaPoX83wst7rJR7u1TGiceyxicVBwzxicTSf2SfQ3z4MQg/640?wx_fmt=other&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1)

#### **2 执行渲染绘制任务**

经过上一小节中的分析，应用在UI线程中从根节点DecorView出发，递归遍历每个子View节点，搜集其drawXXX绘制动作并转换成DisplayListOp命令，将其记录到DisplayListData并填充到RenderNode中，最终完成整个View绘制命令树的构建。从此UI线程的绘制任务就完成了。

下一步UI线程将唤醒RenderThread渲染线程，触发其利用OpenGL执行界面的渲染任务，本小节中我们将重点分析这个流程。我们还是先看看这块代码的简化流程：

```
/*frameworks/base/graphics/java/android/graphics/HardwareRenderer.java*/
public int syncAndDrawFrame(@NonNull FrameInfo frameInfo) {
    // JNI调用native层的相关函数
    return nSyncAndDrawFrame(mNativeProxy, frameInfo.frameInfo, frameInfo.frameInfo.length);
}

/*frameworks/base/libs/hwui/jni/android_graphics_HardwareRenderer.cpp*/
static int android_view_ThreadedRenderer_syncAndDrawFrame(JNIEnv* env, jobject clazz,
        jlong proxyPtr, jlongArray frameInfo, jint frameInfoSize) {
    ...
    RenderProxy* proxy = reinterpret_cast<RenderProxy*>(proxyPtr);
    env->GetLongArrayRegion(frameInfo, 0, frameInfoSize, proxy->frameInfo());
    return proxy->syncAndDrawFrame();
}

/*frameworks/base/libs/hwui/renderthread/RenderProxy.cpp*/
int RenderProxy::syncAndDrawFrame() {
    // 唤醒RenderThread渲染线程，执行DrawFrame绘制任务
    return mDrawFrameTask.drawFrame();
}

/*frameworks/base/libs/hwui/renderthread/DrawFrameTask.cpp*/
int DrawFrameTask::drawFrame() {
    ...
    postAndWait();
    ...
}

void DrawFrameTask::postAndWait() {
    AutoMutex _lock(mLock);
    // 向RenderThread渲染线程的MessageQueue消息队列放入一个待执行任务，以将其唤醒执行run函数
    mRenderThread->queue().post([this]() { run(); });
    // UI线程暂时进入wait等待状态
    mSignal.wait(mLock);
}

void DrawFrameTask::run() {
    // 原生标识一帧渲染绘制任务的systrace tag
    ATRACE_NAME("DrawFrame");
    ...
    {
        TreeInfo info(TreeInfo::MODE_FULL, *mContext);
        //1.将UI线程构建的DisplayListOp绘制命令树同步到RenderThread渲染线程
        canUnblockUiThread = syncFrameState(info);
        ...
    }
    ...
    // 同步完成后则可以唤醒UI线程
    if (canUnblockUiThread) {
        unblockUiThread();
    }
    ...
    if (CC_LIKELY(canDrawThisFrame)) {
        // 2.执行draw渲染绘制动作
        context->draw();
    } else {
        ...
    }
    ...
}

bool DrawFrameTask::syncFrameState(TreeInfo& info) {
    ATRACE_CALL();
    ...
    // 调用CanvasContext的prepareTree函数实现绘制命令树同步的流程
    mContext->prepareTree(info, mFrameInfo, mSyncQueued, mTargetNode);
    ...
}

/*frameworks/base/libs/hwui/renderthread/CanvasContext.cpp*/
void CanvasContext::prepareTree(TreeInfo& info, int64_t* uiFrameInfo, int64_t syncQueued,
                                RenderNode* target) {
     ...
     for (const sp<RenderNode>& node : mRenderNodes) {
        ...
        // 递归调用各个子View对应的RenderNode执行prepareTree动作
        node->prepareTree(info);
        ...
    }
    ...
}

/*frameworks/base/libs/hwui/RenderNode.cpp*/
void RenderNode::prepareTree(TreeInfo& info) {
    ATRACE_CALL();
    ...
    prepareTreeImpl(observer, info, false);
    ...
}

void RenderNode::prepareTreeImpl(TreeObserver& observer, TreeInfo& info, bool functorsNeedLayer) {
    ...
    if (info.mode == TreeInfo::MODE_FULL) {
        // 同步绘制命令树
        pushStagingDisplayListChanges(observer, info);
    }
    if (mDisplayList) {
        // 遍历调用各个子View对应的RenderNode的prepareTreeImpl
        bool isDirty = mDisplayList->prepareListAndChildren(
                observer, info, childFunctorsNeedLayer,
                [](RenderNode* child, TreeObserver& observer, TreeInfo& info,
                   bool functorsNeedLayer) {
                    child->prepareTreeImpl(observer, info, functorsNeedLayer);
                });
        ...
    }
    ...
}

void RenderNode::pushStagingDisplayListChanges(TreeObserver& observer, TreeInfo& info) {
    ...
    syncDisplayList(observer, &info);
    ...
}

void RenderNode::syncDisplayList(TreeObserver& observer, TreeInfo* info) {
    ...
    // 完成赋值同步DisplayList对象
    mDisplayList = mStagingDisplayList;
    mStagingDisplayList = nullptr;
    ...
}

void CanvasContext::draw() {
    ...
    // 1.调用OpenGL库使用GPU，按照构建好的绘制命令完成界面的渲染
    bool drew = mRenderPipeline->draw(frame, windowDirty, dirty, mLightGeometry, &mLayerUpdateQueue,
                                      mContentDrawBounds, mOpaque, mLightInfo, mRenderNodes,
                                      &(profiler()));
    ...
    // 2.将前面已经绘制渲染好的图形缓冲区Binder上帧给SurfaceFlinger合成和显示
    bool didSwap =
            mRenderPipeline->swapBuffers(frame, drew, windowDirty, mCurrentFrameInfo, &requireSwap);
    ...
}
```

从以上代码可以看出：UI线程利用RenderProxy向RenderThread线程发送一个DrawFrameTask任务请求，RenderThread被唤醒，开始渲染，大致流程如下：

syncFrameState中遍历View树上每一个RenderNode，执行prepareTreeImpl函数，实现同步绘制命令树的操作；

 调用OpenGL库API使用GPU硬件，按照构建好的绘制命令完成界面的渲染（具体过程，由于本文篇幅所限，暂不展开分析）；

 将前面已经绘制渲染好的图形缓冲区Binder上帧给SurfaceFlinger合成和显示；

整个过程可以用如下流程图表示：

![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_jpg/DYicOkJDdA2p1HYhroGJOibFrsbicP8O2kGNDuYOCMlDdVzRfYWmIe58OQOFxk4t9qzbvicjhZ7pw5G52JltpHdVDQ/640?wx_fmt=other&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1)从Systrace上这个过程如下图所示：![在这里插入图片描述](https://mmbiz.qpic.cn/sz_mmbiz_jpg/DYicOkJDdA2p1HYhroGJOibFrsbicP8O2kG8cZ8cuicDBzEakbUsDcQvTBYpcJWLRZsnYAeL1QMQSFAEJjBB67l3Kg/640?wx_fmt=other&from=appmsg&watermark=1&tp=webp&wxfrom=5&wx_lazy=1)