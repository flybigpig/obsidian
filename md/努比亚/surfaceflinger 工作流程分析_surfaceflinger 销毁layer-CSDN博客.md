  
SurfaceFlinger继承了[Thread类](https://so.csdn.net/so/search?q=Thread%E7%B1%BB&spm=1001.2101.3001.7020)，自然也继承了Thread类的threadLoop方法，SurfaceFlinger工作线程的主代码都在threadLoop()方法中。工作线程启动后，基类Thread会循环地调用threadLoop方法，SurfaceFlinger的threadLoop()主要是要完成系统中各个Layer(Surface)进行混合（compose），然后不停地把一帧帧混合好的图像数据传送到显示设备中。

### threadLoop的流程

![](http://hi.csdn.net/attachment/201011/2/0_1288679002eeQj.gif)

                 图一   threadLoop流程

### 1. handleConsoleEvents

handleConsoleEvent目前没有深入了解，貌似只是处理显示设备进入休眠状态或者从休眠中唤醒时，改变SufaceFlinger的状态，然后threadLoop的后续流程会根据相应的状态来决定是否继续给显示设备传送显示数据。

### 2. handleTransaction

因为Layer的混合是在线程中进行的，而混合的过程中，应用程序或者系统可能会改变Layer的状态，例如屏幕旋转、增加或删除Layer、某个Layer可见或不可见，为了使这些变动不会破坏当前正在进行的混合动作，SurfaceFlinger维护着两个Layer列表：

- mCurrentState.layersSortedByZ   ---- 当前系统最新的Layer列表
- mDrawingState.layersSortedByZ  ---- 本次混合操作使用的Layer列表

handleTransaction就是根据Layer列表的这些状态的变化，计算是否有可见区域内需要更新，并设置状态变量mVisibleRegionsDirty，然后把mCurrentState赋值给mDrawingState，最后释放已经被丢弃（ditch）的Layer

- ### 上一次混合过程中，可能应用程序释放了一个Layer，可是mDrawingState正在使用，不能马上销毁，所以要等到本次混合前才能做出销毁的动作。
    
- ### 如果Layer的大小有变化并且可见，Layer的handleTransaction将会重新分配缓冲区，并且冻结SurfaceFlinger后续的混合操作，也就是屏幕的内容本次将不会刷新，直到下一个循环的handlePageFlip阶段才解除冻结。
    

### 3. handlePageFlip

该阶段会遍历各个Layer，在每个Layer中，取得并锁住该Layer的frontBuffer，然后利用frontBuffer中的图像数据生成该Layer的2D贴图（Texture），并且计算更新区域，为后续的混合操作做准备。

![](http://hi.csdn.net/attachment/201011/2/0_1288696054hR6X.gif)

                                图二   handlePageFlip处理流程

Layer的lockPageFlip()首先通过[SharedBufferServer类的成员变量lcblk](http://blog.csdn.net/DroidPhone/archive/2010/10/28/5972568.aspx)，调用retireAndLock取得该Layer当前可用的frontBuffer，然后通过reloadTexture方法生成openGL ES的纹理贴图，最后通过unlockPageFlip完成更新区域的Layer坐标到屏幕坐标的变换。

### handleRepaint

 handleRepaint真正实现Layer混合的阶段，下图是handleRepaint的处理流程：

![](http://hi.csdn.net/attachment/201011/2/0_12886989392pYH.gif)

                                                     图三  handleRepaint 的处理流程

handleRepaint首先重置了openGL的观察矩阵，然后遍历mDrawingState.layersSortedByZ 中的Layer列表，调用每个Layer的onDraw方法，在onDraw方法中，会调用drawWithOpenGL()方法，将在handlePageFlip阶段生成的贴图混合到OpenGL的主表面，最后handleRepaint把需要刷新的区域清除。

### unlockClients

unlockClients只是遍历各个Layer并调用各个Layer的finishPageFlip方法。finishPageFlip会进一步调用SharedBufferServer的unlock()方法：([关于SharedBufferSever，请参考本人以下博文的SharedClient 和 SharedBufferStack一节](http://blog.csdn.net/DroidPhone/archive/2010/10/28/5972568.aspx))

**[c-sharp]**  [view plain](http://blog.csdn.net/droidphone/article/details/5982893# "view plain") [copy](http://blog.csdn.net/droidphone/article/details/5982893# "copy")

1. void Layer::finishPageFlip()  
2. {  
3.     status_t err = lcblk->unlock( mFrontBufferIndex );  
4.     LOGE_IF(err!=NO_ERROR,   
5.             "layer %p, buffer=%d wasn't locked!",  
6.             this, mFrontBufferIndex);  
7. }  

 lcblk->unlock( mFrontBufferIndex )会把Layer的frontBuffer解除锁定。

### postFramebuffer

 进入postFramebuffer阶段，OpenGL主表面已经准备好了混合完成的图像数据，postFramebuffer只是简单地调用 hw.flip()，hw.flip()进一步调用了eglSwapBuffers完成主表面的切换，这样屏幕上的图像就会更新为新的数据