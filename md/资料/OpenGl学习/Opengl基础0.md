# OpenGL.ES在Android上的简单实践：23-水印录制（FBO离屏录制，解决透明冲突）

 

### 1、水印签名罢工了？

不知道大家有没注意到，之前我们使用MediaCodec录制的视频，水印签名那部分区域还是黑黑的啊(笑哭.jpg)。道理还是之前说过的，原生的Surface默认格式是RGB565，不支持透明通道。我也在 [20-水印录制](https://blog.csdn.net/a360940265a/article/details/80403715) 提供了解决方案。 第一种就是在shader层使用内置函数Mix进行混合（不推荐）；第二种解决方案就是利用系统API，SurfaceHolder.setFormat(PixelFormat.RGBA_8888)（推荐）; 但是我们这次的Surface是引用MediaCodec.createInputSurface。这下就没能setFormat了？（起码我是不知道Surface->SurfaceHolder，有知道方法的兄弟请指引一下）  难不成我们录制视频的水印就这样黑黑不处理？不要怂，我还有法宝！

 

### 2、离屏渲染->FrameBufferObject

今天我为大家介绍离屏渲染的概念。百度下来很多都是在说iOS的，其实这个概念不止是iOS独有，这个是所有渲染系统都应该具备的。在OpenGL中，GPU屏幕渲染有以下两种方式：

1.On-Screen Rendering

意为当前屏幕渲染，指的是GPU的渲染操作是在当前用于显示的屏幕缓冲区中进行。（例如我们显示/录制的Surface）

2.Off-Screen Rendering
意为离屏渲染，指的是GPU在当前屏幕缓冲区以外新开辟一个缓冲区进行渲染操作。（区别于Surface的另外一种渲染区）

你可能会说，我懂啊，录制视频就是离屏渲染啊！不不不，概念可不一样哦，请参照下图并说明。

![img](https://img-blog.csdn.net/20180611172931294)

我们用于预览/录制的Surface都是由Android系统提供给OpenGL渲染的**帧缓冲区**，OpenGL的所有渲染结果都是直接到达到帧缓冲区，这是on-screen的渲染方式；

除此方式以外，OpenGL还扩展提供了一种方式来创建额外的**帧缓冲区对象（FBO）**。使用帧缓冲区对象，OpenGL可以将原先绘制到窗口提供的帧缓冲区重定向到FBO之中。和窗口提供的帧缓冲区类似，FBO提供了一系列的缓冲区，包括颜色缓冲区、深度缓冲区和模板缓冲区。这些逻辑的缓冲区在FBO中被称为 framebuffer-attachable说明它们是可以绑定到FBO的对象数组上。

FBO中有两类绑定的对象：纹理图像（texture images）和渲染图像（renderbuffer images）。如果纹理对象绑定到FBO，那么OpenGL就会执行`渲染到纹理（render to texture）`的操作，如果渲染对象绑定到FBO，那么OpenGL会执行`离屏渲染(offscreen rendering)`

FBO可以理解为包含了许多挂接点的一个对象，**它自身并不存储图像相关的数据**，他提供了一种可以快速切换外部纹理对象和渲染对象挂接点的方式，在FBO中必然包含一个深度缓冲区挂接点和一个模板缓冲区挂接点，同时还包含许多颜色缓冲区挂节点（具体多少个受OpenGL实现的影响，可以通过GL_MAX_COLOR_ATTACHMENTS使用glGet查询），**FBO的这些挂接点用来挂接纹理对象和渲染对象，这两类对象中才真正存储了需要被显示的数据。**FBO提供了一种快速有效的方法挂接或者解绑这些外部的对象，对于纹理对象使用 `glFramebufferTexture2D`，对于渲染对象使用`glFramebufferRenderbuffer` 。具体描述参考下图： 

![FBO](https://img-blog.csdn.net/20161108145719480)

上面是知识概念，而且已经画出了重点。白话文解释：FBO是一个挂接器，类似画家画画用的托架；其中FBO只能挂接两种对象，纹理图像 和 渲染图像，这个理解就是画家准备创作作品前，在托架上放的是油画纸还是水墨纸（纹理），或者根本不是放画纸，放的是木板雕刻（渲染模板）。最后我们等画家创作出他的艺术品后，直接搬到到展示区，呈现給大家。（鼓掌散花）

 

### 3、创建FBO

既然我们已经认识了什么是FBO，接下来我们就认识认识关于FBO的OpenGL.API

和其他VAO，VBO，IBO一样，通过调用Gen****，创建FBO，代码如下：

```java
  final int frameBuffers[] = new int[1];
        GLES20.glGenFramebuffers(1, frameBuffers, 0);
        if (frameBuffers[0] == 0) {
            int i = GLES20.glGetError();
            throw new RuntimeException("Could not create a new frame buffer object, glErrorString : "+ GLES20.glGetString(i));
        }
        int frameBufferId = frameBuffers[0];
```

接着，我们开始使用FBO的时候，需要通过绑定纹理对象来锁定挂接渲染区。

```java
    int textureId = createFBOTexture(width, height, format); //创建指定format的纹理对象
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, frameBufferId); //绑定fbo进行操作
        GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, //说明FBO挂接操作
                                    GLES20.GL_COLOR_ATTACHMENT0, //指定挂接区是单元0
                                    GLES20.GL_TEXTURE_2D, //说明挂接的是纹理对象
                                    textureId, //具体挂接的纹理对象
                                     0);
```

接着我们就可以渲染了（draw），当渲染结束后，我们就需要解绑FBO，告诉OpenGL我们已经不需要在FBO上创作了。

```java
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
```

最后，展览结束关门了，我们需要收拾东西。

```java
        GLES20.glDeleteFramebuffers(1, new int[]{frameBufferId}, 0);
        GLES20.glDeleteTextures(1, new int[]{textureId}, 0);
```

以上，so easy。妈妈再也不用担心我们学不会啦。

 

### 4、使用FBO

可能有同学会吐槽，这么简单的API还要你教怎么使用哦。别紧张嘛，会用 和 用得native是两码事嘛。所以接下来我给自己的FBO封装，方便往后代码的编写调用。

```java

public class FrameBuffer {
    private int mWidth;
    private int mHeight;
    private int frameBufferId;
    private int textureId;
    public FrameBuffer() {
        mWidth=0;
        mHeight=0;
        frameBufferId=0;
        textureId=0;
    }
    public int getTextureId() {
        return textureId;
    }
    public boolean isInstantiation() {
        return mWidth!=0||mHeight!=0;
    }
 
    public boolean setup(int width, int height){
        this.mWidth = width;
        this.mHeight = height;
 
        final int frameBuffers[] = new int[1];
        GLES20.glGenFramebuffers(1, frameBuffers, 0);
        if (frameBuffers[0] == 0) {
            int i = GLES20.glGetError();
            throw new RuntimeException("Could not create a new frame buffer object, glErrorString : "+ GLES20.glGetString(i));
        }
        frameBufferId = frameBuffers[0];
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
        return  true;
    }
 
    public boolean begin(){
        if(textureId==0 ){
            textureId = createFBOTexture(mWidth, mHeight, GLES20.GL_RGBA);
        }
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, frameBufferId);
        GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0,
                                    GLES20.GL_TEXTURE_2D, textureId, 0);
        return true;
    }
    private int createFBOTexture(int width, int height, int format) {
        final int[] textureIds = new int[1];
        GLES20.glGenTextures(1, textureIds, 0);
        if(textureIds[0] == 0){
            int i = GLES20.glGetError();
            throw new RuntimeException("Could not create a new texture buffer object, glErrorString : "+ GLES20.glGetString(i));
        }
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureIds[0]);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, format, width, height,
                0, format, GLES20.GL_UNSIGNED_BYTE, null);
        return textureIds[0];
    }
 
    public void end(){
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
    }
 
    public void release(){
        mWidth = 0;
        mHeight = 0;
        GLES20.glDeleteFramebuffers(1, new int[]{frameBufferId}, 0);
        GLES20.glDeleteTextures(1, new int[]{textureId}, 0);
        frameBufferId = 0;
        textureId = 0;
    }
}
```

然后我们回到测试页面ContinuousRecordActivity，开始使用我们的FBO修正录像签名的透明问题。概述如下图：

![img](https://img-blog.csdn.net/20180612101157385)

不知道大家有没看懂，**这个就是利用离屏渲染 和 实时渲染的区别所在。**离屏渲染充当一些幕后的准备操作，当我们向fbo挂载一个纹理对象的时候，在fbo所渲染的任何指令，都是输出到这个挂载的纹理对象当中。而且这个纹理的格式、大小、类型都是由我们自己定义，非常方便灵活。 

利用这个方法，我们可以声明一个支持透明格式（RGBA四通道）的纹理对象，随后把这个纹理各自传到实时预览 和 录制器当中渲染，以到达效果。事不宜迟，show code！

```java
  private FrameBuffer fbo;
    private FBOFrameRect mFBOFrameRect;
 
    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {
        ... ... ...
        mFrameRect.setShaderProgram(new FrameRectSProgram());
        mWaterSign.setShaderProgram(new WaterSignSProgram());
        mFBOFrameRect.setShaderProgram(new WaterSignSProgram());
        ... ... ...
        fbo = new FrameBuffer();
        fbo.setup(VIDEO_HEIGHT, VIDEO_WIDTH);
    }
```

我们新增一个FBOFrameRect，这个类和FrameRect区别在于内部引用的ShaderProgram，因为之前我的摄像头帧纹理是samplerExternalOES的类型；现在我们使用离屏渲染的方式，使用普通的纹理方式（纹理y坐标上下颠倒），所以我们直接引用水印签名的WaterSignSProgram就可以了；之后我们创建FBO，注意屏幕是竖屏的方式，和摄像头默认横屏是宽高互换。

```java
 private void drawFrame() {
        mDisplaySurface.makeCurrent();
 
        fbo.begin();
        GLES20.glClear( GLES20.GL_DEPTH_BUFFER_BIT | GLES20.GL_COLOR_BUFFER_BIT);
        GLES20.glEnable(GLES20.GL_BLEND);
        GLES20.glBlendFunc(GLES20.GL_ONE, GLES20.GL_ONE_MINUS_SRC_ALPHA);
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        mCameraTexture.updateTexImage();
        mCameraTexture.getTransformMatrix(mTmpMatrix);
        int viewWidth = sv.getWidth();
        int viewHeight = sv.getHeight();
        GLES20.glViewport(0, 0, viewWidth, viewHeight);
        mFrameRect.drawFrame(mTextureId, mTmpMatrix);
        GLES20.glViewport(0, 0, 288, 144);
        mWaterSign.drawFrame(mSignTexId);
        fbo.end();
 
        GLES20.glViewport(0, 0, viewWidth, viewHeight);
        mFBOFrameRect.drawFrame(fbo.getTextureId());
        
        mDisplaySurface.swapBuffers();
 
        // 水印录制 状态设置
        if(mRequestRecord) {
            if(!recording) {
                mRecordEncoder.startRecording(new CameraRecordEncoder.EncoderConfig(
                        outputFile, VIDEO_HEIGHT, VIDEO_WIDTH, 1000000,
                        EGL14.eglGetCurrentContext(), ContinuousRecordActivity.this));
                mRecordEncoder.setTextureId(fbo.getTextureId());
                recording = mRecordEncoder.isRecording();
            }
            mRecordEncoder.frameAvailable(mCameraTexture);
        } else {
            if(recording) {
                mRecordEncoder.stopRecording();
                recording = false;
            }
        }
    }
```

之后我们再看看drawFrame方法，我们把原来之前直接渲染的指令（mFrameRect.drawFrame 和 mWaterSign.drawFrame）都包裹在fbo（begin~end）的操作当中，这样渲染指令的结果就流入到fbo挂载的纹理对象。

正常的渲染流程结束后，我们就可以取出fbo.texture一整张的纹理对象，渲染到显示区域（mFBOFrameRect.drawFrame）。接着我们还要进行录制，把fbo.texture设置到编码录制器，编码录制器的渲染操作修改思路也是一样的。

```java
 private void handleFrameAvailable(float[] transform, long timestampNanos) {
        //先推动一次编码器工作，把编码后的数据写入Muxer
        mRecordEncoder.drainEncoder(false);
 
        mRecorderInputSurface.makeCurrent();
        GLES20.glClear( GLES20.GL_DEPTH_BUFFER_BIT | GLES20.GL_COLOR_BUFFER_BIT);
        GLES20.glEnable(GLES20.GL_BLEND);
        GLES20.glBlendFunc(GLES20.GL_ONE, GLES20.GL_ONE_MINUS_SRC_ALPHA);
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        //GLES20.glViewport(0, 0, mConfig.mWidth, mConfig.mHeight );
        //mFrameRect.drawFrame(mFrameTextureId, transform);
        //GLES20.glViewport(0, 0, 288, 144);
        //mWaterSign.drawFrame(mSignTexId);
        GLES20.glViewport(0, 0, mConfig.mWidth, mConfig.mHeight );
        mFBOFrameRect.drawFrame(mFrameTextureId);
 
        // mRecorderInputSurface是 获取编码器的输入Surface 创建的EGLSurface，
        // 以上的draw直接渲染到mRecorderInputSurface，喂养数据到编码器当中，非常方便。
        mRecorderInputSurface.setPresentationTime(timestampNanos);
        mRecorderInputSurface.swapBuffers();
    }
```

我们可以看到，修改后的编码录制器不在需要独自的重新绘制FrameRect/WaterSign，我们只需要绘制显示FBO.texture就大功告成了，so easy。 

而且因为FBO.texture是我们自己独立创建的，格式GLES20.GL_RGBA支持四通道的颜色区，水印签名不再变成黑色的了！

 

### 5、延伸思考。

这章我们学习了FBO，解决了MediaCodec.inputSurface不支持透明通道的问题。 其实FBO用途还有很多，比如微信视频通话中的显示对方的画中画效果，我们就可以利用FBO离屏渲染显示出来了；更有一些视频图像应用双击放大，右下角还保留显示着原图；再譬如一些短视频应用的 镜像 九宫格（不同色调） 等等效果。 相信同学们一定能自己扩展延伸。