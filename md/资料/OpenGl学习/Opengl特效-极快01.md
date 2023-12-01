在仿抖音的第一步中封装了ScreenFilter类来实现渲染屏幕的操作，我们都知道在抖音的视频录制过程中，可以添加很多的效果进行显示，比如说磨皮、美颜、大眼以及滤镜等效果，如果把这些效果都放在ScreenFilter中，就需要使用很多的if else来进行判断是否开启效果，显而易见，这样的会显得项目结构不是很美好，我们可以将每种效果都写成一个Filter，并且在ScreenFilter之前的效果，都可以不用显示到屏幕当中去，所以可以使用FBO来实现这个需求，不懂 FBO的可以翻看上一篇的博客FBO的使用

但是这里有一个问题，就是在摄像头画面经过FBO缓冲，我们再从FBO中绘制到屏幕上去，这里的ScreenFilter获取的纹理是来自于FBO中的纹理，也就是OpenGL ES中的，所以不再需要额外扩展的纹理类型了，可以直接使用sampler2D类型，也就意味着ScrennFilter，

  \1. 开启效果：使用sampler2D

  \2. 未开启效果：使用samplerExternalOES

那么就需要ScreenFilter使用if else去判断，很麻烦，所以我们可以不管摄像头是否开启效果都先将摄像头数据写到FBO中，这样的话，ScreenFilter的采样数据始终都可以是sampler2D了。也就是下面这种结构：





![img](https:////upload-images.jianshu.io/upload_images/14188537-be588ea379f833cc.png?imageMogr2/auto-orient/strip|imageView2/2/w/961/format/webp)

需求

长按按钮进行视频的录制，视频有5种速度的录制，极慢、慢、正常、快、以及极快，抬起手指时候停止录制，并将视频保存以MP4格式保存在sdcard中。

（抖音的视频录制在录制完成以后显示的时候都是正常速度，这里我为了看到效果，保存下来的时候是用当前选择的速度进行显示的）。

分析需求

想要录制视频，就需要对视频进行编码，摄像头采集到的视频数据一般为AVC格式的，这里我们需要将AVC格式的数据，编码成h.264的，然后再封装为MP4格式的数据。对于速度的控制，可以在写出到MP4文件格式之前，修改它的时间戳，就可以了。

实现需求

MediaCodec

MediaCodec是Android4.1.2（API 16）提供的一套编解码的API，之前试过使用FFmpeg来进行编码，效果不如这个，这个也比较简单，这次视频录制就使用它来进行编码。MediaCodec使用很简单，它存在一个输入缓冲区和一个输出缓冲区，我们把要编码的数据塞到输入缓冲区，它就可以进行编码了，然后从输出缓冲区取编码后的数据就可以了。

![img](https:////upload-images.jianshu.io/upload_images/14188537-f72cbe096e773e60.png?imageMogr2/auto-orient/strip|imageView2/2/w/1000/format/webp)

还有一种方式可以告知MediaCodec需要编码的数据，

![img](https:////upload-images.jianshu.io/upload_images/14188537-a1c5812683aef731.png?imageMogr2/auto-orient/strip|imageView2/2/w/949/format/webp)

这个接口是用来创建一个Surface的，Surface是用来干啥的呢，就是用来"画画"的，也就是说我们只要在这个Surface上画出我们需要的图像，MediaCodec就会自动帮我们编码这个Surface上面的图像数据，我们可以直接从输出缓冲区中获取到编码后的数据。之前的时候我们是使用OpenGL绘画显示到屏幕上去，我们可以同时将这个画面绘制到MediaCodec#createInputSurface() 中去，这样就可以了。

那怎么样才能绘制到MediaCodec的Surface当中去呢，我们知道录制视频是在一个线程中，显示图像（GLSurfaceView）是在另一个GLThread线程中进行的，所以这两者的EGL环境也不同，但是两者又共享上下文资源，录制现场中画面的绘制需要用到显示线程中的texture等，那么这个线程就需要我们做这些：

  1.配置录制使用的EGL环境（可以参照GLSurfaceView怎么配置的）

  2.将显示的图像绘制到MediaCodec中的Surface中

  \3. 编码（h.264）与复用(mp4)的工作

代码实现

MediaRecorder.java

视频编码类

![img](https:////upload-images.jianshu.io/upload_images/14188537-9ae5b616c78aceec.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

![img](https:////upload-images.jianshu.io/upload_images/14188537-3e2deadbe18f7bab.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

![img](https:////upload-images.jianshu.io/upload_images/14188537-dd8b829af760a9b7.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

![img](https:////upload-images.jianshu.io/upload_images/14188537-d2be7a89413b12fd.png?imageMogr2/auto-orient/strip|imageView2/2/w/1035/format/webp)

这里的status==MediaCodec.INFO_TRY_AGAIN_LATER可以看下图理解



![img](https:////upload-images.jianshu.io/upload_images/14188537-05017d8dfff589f8.png?imageMogr2/auto-orient/strip|imageView2/2/w/1173/format/webp)

![img](https:////upload-images.jianshu.io/upload_images/14188537-38a53f54197498a3.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)



 





 