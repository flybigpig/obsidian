### 目录

- 一 MediaCodec
- 二 极快、极慢模式视频录制
- - 1 创摄像头预览
  - - AbstractFilter
    - ScreenFilter
    - CameraFilter
  - 2 渲染时定义一个录制类MediaRecorder
  - 3 录制类MediaRecorder
  - - MediaRecorder
    - EGLBase
- 三 Demo

**一 MediaCodec**

MediaCodec是Android 4.1.2(API 16)提供的一套编解码API。它的使用非常简单，它存在一个输入缓冲区与一个输出缓冲区，在编码时我们将数据塞入输入缓冲区，然后从输出缓冲区取出编码完成后的数据就可以了。

![在这里插入图片描述](https://i2.wp.com/img-blog.csdnimg.cn/20201209193020452.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2Jhb3BlbmdqaWFu,size_16,color_FFFFFF,t_70)

除了直接操作输入缓冲区之外，还有另一种方式来告知MediaCodec需要编码的数据，那就是:

| 1    | public native final Surface createInputSurface(); |
| ---- | ------------------------------------------------- |
|      |                                                   |

使用此接口创建一个Surface，然后我们在这个Surface中"作画"，MediaCodec就能够自动的编码Surface中的“画作”,我们只需要从输出缓冲区取出编码完成之后的数据即可。







? 此前，我们使用OpenGL进行绘画显示在屏幕上，然而想要复制屏幕图像到cpu内存中却不是一件非常轻松的事情。所以我们可以直接将OpenGL显示到屏幕中的图像,同时绘制到MediaCodec#createInputSurface当中去。

> PBO(Pixel Buffer Object,像素缓冲对象)通过直接的内存访问(Direct Memory Access,DMA)高速的复制屏幕图像像素数据到CPU内存,但这里我们直接使用createInputSurface更简单…
>
> 录制我们在另外一个线程中进行(**录制现场**)，所以录制的EGL环境和显示的EGL环境(GLSurfaceView,**显示线程**)是两个独立的工作环境，他们又能够共享上下文资源：**显示线程**中使用的texture等，需要能够在**录制线程**中操作(通过**录制线程**中使用OpenGL绘制到MediaCodec的Surface)。
>
> 在这个线程中我们需要自己来:
>
> 1、配置录制使用的EGL环境(参照GLSurfaceView是怎么配置的)
>
> 
>
> 
>
> 
>
> 2、完成将显示的图像绘制到MediaCodec的Surface中
>
> 3、编码(H.264)与复用(封装mp4)的工作

**二 极快、极慢模式视频录制**

## 1 创摄像头预览

NDK51_OpenGL：FBO

> 定义一个DouyinView 继承GLSurfaceView， 并setRenderer(douyinRenderer);
> DouyinRenderer负责渲染
> DouyinRenderer中创建画布，设置效果
> CameraFilter写入fbo (帧缓存)，ScreenFilter 负责往屏幕上渲染

####  AbstractFilter

```
public abstract class AbstractFilter {

    protected FloatBuffer mGLVertexBuffer;
    protected FloatBuffer mGLTextureBuffer;

    //顶点着色
    protected int mVertexShaderId;
    //片段着色
    protected int mFragmentShaderId;


    protected int mGLProgramId;
    /**
     * 顶点着色器
     * attribute vec4 position;
     * 赋值给gl_Position(顶点)
     */
    protected int vPosition;
    /**
     * varying vec2 textureCoordinate;
     */
    protected int vCoord;


    /**
     * uniform mat4 vMatrix;
     */
    protected int vMatrix;

    /**
     * 片元着色器
     * Samlpe2D 扩展 samplerExternalOES
     */
    protected int vTexture;


    protected int mOutputWidth;
    protected int mOutputHeight;

    public AbstractFilter(Context context, int vertexShaderId, int fragmentShaderId) {
        this.mVertexShaderId = vertexShaderId;
        this.mFragmentShaderId = fragmentShaderId;
        // 4个点 x，y = 4*2 float 4字节 所以 4*2*4
        mGLVertexBuffer = ByteBuffer.allocateDirect(4 * 2 * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer();
        mGLVertexBuffer.clear();
        float[] VERTEX = {
                -1.0f, -1.0f,
                1.0f, -1.0f,
                -1.0f, 1.0f,
                1.0f, 1.0f
        };
        mGLVertexBuffer.put(VERTEX);


        mGLTextureBuffer = ByteBuffer.allocateDirect(4 * 2 * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer();
        mGLTextureBuffer.clear();
        float[] TEXTURE = {
                0.0f, 1.0f,
                1.0f, 1.0f,
                0.0f, 0.0f,
                1.0f, 0.0f
        };
        mGLTextureBuffer.put(TEXTURE);


        initilize(context);
        initCoordinate();
    }


    protected void initilize(Context context) {
        String vertexSharder = OpenGLUtils.readRawTextFile(context, mVertexShaderId);
        String framentShader = OpenGLUtils.readRawTextFile(context, mFragmentShaderId);
        mGLProgramId = OpenGLUtils.loadProgram(vertexSharder, framentShader);
        // 获得着色器中的 attribute 变量 position 的索引值
        vPosition = GLES20.glGetAttribLocation(mGLProgramId, "vPosition");
        vCoord = GLES20.glGetAttribLocation(mGLProgramId,
                "vCoord");
        vMatrix = GLES20.glGetUniformLocation(mGLProgramId,
                "vMatrix");
        // 获得Uniform变量的索引值
        vTexture = GLES20.glGetUniformLocation(mGLProgramId,
                "vTexture");
    }


    public void onReady(int width, int height) {
        mOutputWidth = width;
        mOutputHeight = height;
    }


    public void release() {
        GLES20.glDeleteProgram(mGLProgramId);
    }


    public int onDrawFrame(int textureId) {
        //设置显示窗口
        GLES20.glViewport(0, 0, mOutputWidth, mOutputHeight);

        //使用着色器
        GLES20.glUseProgram(mGLProgramId);

        //传递坐标
        mGLVertexBuffer.position(0);
        GLES20.glVertexAttribPointer(vPosition, 2, GLES20.GL_FLOAT, false, 0, mGLVertexBuffer);
        GLES20.glEnableVertexAttribArray(vPosition);

        mGLTextureBuffer.position(0);
        GLES20.glVertexAttribPointer(vCoord, 2, GLES20.GL_FLOAT, false, 0, mGLTextureBuffer);
        GLES20.glEnableVertexAttribArray(vCoord);


        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        GLES20.glUniform1i(vTexture, 0);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

        return textureId;
    }

    //修改坐标
    protected void initCoordinate() {

    }

}


```

### ScreenFilter

```
/**
 * 负责往屏幕上渲染
 */
public class ScreenFilter extends AbstractFilter{

    public ScreenFilter(Context context) {
        super(context,R.raw.base_vertex, R.raw.base_frag);
    }

}


```



### CameraFilter

```
public class CameraFilter  extends AbstractFilter{

    private int[] mFrameBuffers;
    private int[] mFrameBufferTextures;
    private float[] matrix;

    public CameraFilter(Context context) {
        super(context, R.raw.camera_vertex2, R.raw.camera_frag2);
    }

    @Override
    protected void initCoordinate() {
        mGLTextureBuffer.clear();
        //摄像头是颠倒的
//        float[] TEXTURE = {
//                0.0f, 0.0f,
                1.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 1.0f
//        };
        //调整好了镜像
//        float[] TEXTURE = {
//                1.0f, 0.0f,
//                0.0f, 0.0f,
//                1.0f, 1.0f,
//                0.0f, 1.0f,
//        };
        //修复旋转 逆时针旋转90度
        float[] TEXTURE = {
                0.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 0.0f,
                1.0f, 1.0f,
        };
        mGLTextureBuffer.put(TEXTURE);
    }

    @Override
    public void release() {
        super.release();
        destroyFrameBuffers();
    }

    public void destroyFrameBuffers() {
        //删除fbo的纹理
        if (mFrameBufferTextures != null) {
            GLES20.glDeleteTextures(1, mFrameBufferTextures, 0);
            mFrameBufferTextures = null;
        }
        //删除fbo
        if (mFrameBuffers != null) {
            GLES20.glDeleteFramebuffers(1, mFrameBuffers, 0);
            mFrameBuffers = null;
        }
    }

    @Override
    public void onReady(int width, int height) {
        super.onReady(width, height);
        if (mFrameBuffers != null) {
            destroyFrameBuffers();
        }

        //fbo的创建 (缓存)
        //1、创建fbo （离屏屏幕）
        mFrameBuffers = new int[1];
        // 1、创建几个fbo 2、保存fbo id的数据 3、从这个数组的第几个开始保存
        GLES20.glGenFramebuffers(mFrameBuffers.length,mFrameBuffers,0);

        //2、创建属于fbo的纹理
        mFrameBufferTextures = new int[1]; //用来记录纹理id
        //创建纹理
        OpenGLUtils.glGenTextures(mFrameBufferTextures);

        //让fbo与 纹理发生关系
        //创建一个 2d的图像
        // 目标 2d纹理+等级 + 格式 +宽、高+ 格式 + 数据类型(byte) + 像素数据
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D,mFrameBufferTextures[0]);
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D,0,GLES20.GL_RGBA,mOutputWidth,mOutputHeight,
                0,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE, null);
        // 让fbo与纹理绑定起来 ， 后续的操作就是在操作fbo与这个纹理上了
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,mFrameBuffers[0]);
        GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER,GLES20.GL_COLOR_ATTACHMENT0,
                GLES20.GL_TEXTURE_2D, mFrameBufferTextures[0], 0);
        //解绑
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D,0);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,0);

    }

    @Override
    public int onDrawFrame(int textureId) {
        //设置显示窗口
        GLES20.glViewport(0, 0, mOutputWidth, mOutputHeight);

        //不调用的话就是默认的操作glsurfaceview中的纹理了。显示到屏幕上了
        //这里我们还只是把它画到fbo中(缓存)
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,mFrameBuffers[0]);

        //使用着色器
        GLES20.glUseProgram(mGLProgramId);

        //传递坐标
        mGLVertexBuffer.position(0);
        GLES20.glVertexAttribPointer(vPosition, 2, GLES20.GL_FLOAT, false, 0, mGLVertexBuffer);
        GLES20.glEnableVertexAttribArray(vPosition);

        mGLTextureBuffer.position(0);
        GLES20.glVertexAttribPointer(vCoord, 2, GLES20.GL_FLOAT, false, 0, mGLTextureBuffer);
        GLES20.glEnableVertexAttribArray(vCoord);

        //变换矩阵
        GLES20.glUniformMatrix4fv(vMatrix,1,false,matrix,0);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        //因为这一层是摄像头后的第一层，所以需要使用扩展的  GL_TEXTURE_EXTERNAL_OES
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureId);
        GLES20.glUniform1i(vTexture, 0);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,0);
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER,0);
        //返回fbo的纹理id
        return mFrameBufferTextures[0];
    }

    public void setMatrix(float[] matrix) {
        this.matrix = matrix;
    }
}


```

## 2 渲染时定义一个录制类MediaRecorder

```
public class DouyinRenderer implements GLSurfaceView.Renderer, SurfaceTexture.OnFrameAvailableListener {

    private ScreenFilter mScreenFilter;
    private DouyinView mView;
    private CameraHelper mCameraHelper;
    private SurfaceTexture mSurfaceTexture;
    private float[] mtx = new float[16];
    private int[] mTextures;
    private CameraFilter mCameraFilter;
    private MediaRecorder mMediaRecorder;

    public DouyinRenderer(DouyinView douyinView) {
        mView = douyinView;
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        //初始化的操作
        mCameraHelper = new CameraHelper(Camera.CameraInfo.CAMERA_FACING_BACK);

        //准备好摄像头绘制的画布
        //通过opengl创建一个纹理id
        mTextures = new int[1];
        // 这里可以不配置 （当然 配置了也可以）
        GLES20.glGenTextures(mTextures.length, mTextures, 0);
        mSurfaceTexture = new SurfaceTexture(mTextures[0]);
        mSurfaceTexture.setOnFrameAvailableListener(this);
        //注意：必须在gl线程操作opengl
        mCameraFilter = new CameraFilter(mView.getContext());
        mScreenFilter = new ScreenFilter(mView.getContext());

        //渲染线程的EGL上下文
        EGLContext eglContext = EGL14.eglGetCurrentContext();
        mMediaRecorder = new MediaRecorder(mView.getContext(), "/sdcard/a.mp4", CameraHelper.HEIGHT, CameraHelper.WIDTH, eglContext);
    }

    /**
     * 画布发生了改变
     *
     * @param gl
     * @param width
     * @param height
     */
    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        //开启预览
        mCameraHelper.startPreview(mSurfaceTexture);
        mCameraFilter.onReady(width, height);
        mScreenFilter.onReady(width, height);
    }

    /**
     * 开始画画吧
     *
     * @param gl
     */
    @Override
    public void onDrawFrame(GL10 gl) {
        // 配置屏幕
        //清理屏幕 :告诉opengl 需要把屏幕清理成什么颜色
        GLES20.glClearColor(0, 0, 0, 0);
        //执行上一个：glClearColor配置的屏幕颜色
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

        // 把摄像头的数据先输出来
        // 更新纹理，然后我们才能够使用opengl从SurfaceTexure当中获得数据 进行渲染
        mSurfaceTexture.updateTexImage();
        //surfaceTexture 比较特殊，在opengl当中 使用的是特殊的采样器 samplerExternalOES （不是sampler2D）
        //获得变换矩阵
        mSurfaceTexture.getTransformMatrix(mtx);
        mCameraFilter.setMatrix(mtx);
        //责任链
        int id = mCameraFilter.onDrawFrame(mTextures[0]);
        //加效果滤镜
        // id  = 效果1.onDrawFrame(id);
        // id = 效果2.onDrawFrame(id);
        //....
        //加完之后再显示到屏幕中去
        mScreenFilter.onDrawFrame(id);
        //进行录制
        mMediaRecorder.encodeFrame(id, mSurfaceTexture.getTimestamp());
    }

    public void onSurfaceDestroyed() {
        mCameraHelper.stopPreview();
    }

    public void startRecord(float speed) {
        try {
            mMediaRecorder.start(speed);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public void stopRecord() {
        mMediaRecorder.stop();
    }
    /**
     * surfaceTexture 有一个有效的新数据的时候回调
     *
     * @param surfaceTexture
     */
    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        mView.requestRender();
    }
}


```

## 3 录制类MediaRecorder

### MediaRecorder

```
/**
 * 录制类
 */
public class MediaRecorder {

    private final Context mContext;
    private final String mPath;
    private final int mWidth;
    private final int mHeight;
    private final EGLContext mEglContext;
    private MediaCodec mMediaCodec;
    private Surface mInputSurface;
    private MediaMuxer mMediaMuxer;
    private Handler mHandler;
    private EGLBase mEglBase;
    private boolean isStart;
    private int index;
    private float mSpeed;

    /**
     * @param context 上下文
     * @param path    保存视频的地址
     * @param width   视频宽
     * @param height  视频高
     *                还可以让人家传递帧率 fps、码率等参数
     */
    public MediaRecorder(Context context, String path, int width, int height, EGLContext eglContext) {
        mContext = context.getApplicationContext();
        mPath = path;
        mWidth = width;
        mHeight = height;
        mEglContext = eglContext;
    }


    /**
     * 开始录制视频
     * @param speed
     */
    public void start(float speed) throws IOException {
        mSpeed = speed;
        /**
         * 配置MediaCodec 编码器
         */
        //视频格式
        // 类型（avc高级编码 h264） 编码出的宽、高
        MediaFormat mediaFormat = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, mWidth, mHeight);
        //参数配置
        // 1500kbs码率
        mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, 1500_000);
        //帧率
        mediaFormat.setInteger(MediaFormat.KEY_FRAME_RATE, 20);
        //关键帧间隔
        mediaFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 20);
        //颜色格式（RGB\YUV）
        //从surface当中回去
        mediaFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        //编码器
        mMediaCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
        //将参数配置给编码器
        mMediaCodec.configure(mediaFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

        //交给虚拟屏幕 通过opengl 将预览的纹理 绘制到这一个虚拟屏幕中
        //这样MediaCodec 就会自动编码 inputSurface 中的图像
        mInputSurface = mMediaCodec.createInputSurface();

        //  H.264
        // 播放：
        //  MP4 -> 解复用 (解封装) -> 解码 -> 绘制
        //封装器 复用器
        // 一个 mp4 的封装器 将h.264 通过它写出到文件就可以了
        mMediaMuxer = new MediaMuxer(mPath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);


        /**
         * 配置EGL环境
         */
        //Handler ： 线程通信
        // Handler: 子线程通知主线程
//        Looper.loop();
        HandlerThread handlerThread = new HandlerThread("VideoCodec");
        handlerThread.start();
        Looper looper = handlerThread.getLooper();
        // 用于其他线程 通知子线程
        mHandler = new Handler(looper);
        //子线程： EGL的绑定线程 ，对我们自己创建的EGL环境的opengl操作都在这个线程当中执行
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                //创建我们的EGL环境 (虚拟设备、EGL上下文等)
                mEglBase = new EGLBase(mContext, mWidth, mHeight, mInputSurface, mEglContext);
                //启动编码器
                mMediaCodec.start();
                isStart = true;
            }
        });

    }


    /**
     * 传递 纹理进来
     * 相当于调用一次就有一个新的图像需要编码
     */
    public void encodeFrame(final int textureId, final long timestamp) {
        if (!isStart) {
            return;
        }
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                //把图像画到虚拟屏幕
                mEglBase.draw(textureId, timestamp);
                //从编码器的输出缓冲区获取编码后的数据就ok了
                getCodec(false);
            }
        });
    }

    /**
     * 获取编码后 的数据
     *
     * @param endOfStream 标记是否结束录制
     */
    private void getCodec(boolean endOfStream) {
        //不录了， 给mediacodec一个标记
        if (endOfStream) {
            mMediaCodec.signalEndOfInputStream();
        }
        //输出缓冲区
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
        // 希望将已经编码完的数据都 获取到 然后写出到mp4文件
        while (true) {
            //等待10 ms
            int status = mMediaCodec.dequeueOutputBuffer(bufferInfo, 10_000);
            //让我们重试  1、需要更多数据  2、可能还没编码为完（需要更多时间）
            if (status == MediaCodec.INFO_TRY_AGAIN_LATER) {
                // 如果是停止 我继续循环
                // 继续循环 就表示不会接收到新的等待编码的图像
                // 相当于保证mediacodec中所有的待编码的数据都编码完成了，不断地重试 取出编码器中的编码好的数据
                // 标记不是停止 ，我们退出 ，下一轮接收到更多数据再来取输出编码后的数据
                if (!endOfStream) {
                    //不写这个 会卡太久了，没有必要 你还是在继续录制的，还能调用这个方法的！
                    break;
                }
                //否则继续
            } else if (status == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                //开始编码 就会调用一次
                MediaFormat outputFormat = mMediaCodec.getOutputFormat();
                //配置封装器
                // 增加一路指定格式的媒体流 视频
                index = mMediaMuxer.addTrack(outputFormat);
                mMediaMuxer.start();
            } else if (status == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                //忽略
            } else {
                //成功 取出一个有效的输出
                ByteBuffer outputBuffer = mMediaCodec.getOutputBuffer(status);
                //如果获取的ByteBuffer 是配置信息 ,不需要写出到mp4
                if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                    bufferInfo.size = 0;
                }

                if (bufferInfo.size != 0) {
                    bufferInfo.presentationTimeUs = (long) (bufferInfo.presentationTimeUs / mSpeed);
                    //写到mp4
                    //根据偏移定位
                    outputBuffer.position(bufferInfo.offset);
                    //ByteBuffer 可读写总长度
                    outputBuffer.limit(bufferInfo.offset + bufferInfo.size);
                    //写出
                    mMediaMuxer.writeSampleData(index, outputBuffer, bufferInfo);
                }
                //输出缓冲区 我们就使用完了，可以回收了，让mediacodec继续使用
                mMediaCodec.releaseOutputBuffer(status, false);
                //结束
                if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                    break;
                }
            }
        }
    }


    public void stop() {
        isStart = false;
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                getCodec(true);
                mMediaCodec.stop();
                mMediaCodec.release();
                mMediaCodec = null;
                mMediaMuxer.stop();
                mMediaMuxer.release();
                mMediaMuxer = null;
                mEglBase.release();
                mEglBase = null;
                mInputSurface = null;
                mHandler.getLooper().quitSafely();
                mHandler = null;
            }
        });
    }
}


```

### EGLBase

```
/**
 * EGL配置 与 录制的opengl操作  工具类
 */
public class EGLBase {

    private ScreenFilter mScreenFilter;
    private EGLSurface mEglSurface;
    private EGLDisplay mEglDisplay;
    private EGLConfig mEglConfig;
    private EGLContext mEglContext;

    /**
     * @param context
     * @param width
     * @param height
     * @param surface    MediaCodec创建的surface 我们需要将其贴到我们的虚拟屏幕上去
     * @param eglContext GLThread的EGL上下文
     */
    public EGLBase(Context context, int width, int height, Surface surface, EGLContext eglContext) {
        //配置EGL环境
        createEGL(eglContext);
        //把Surface贴到  mEglDisplay ，发生关系
        int[] attrib_list = {
                EGL14.EGL_NONE
        };
        // 绘制线程中的图像 就是往这个mEglSurface 上面去画
        mEglSurface = EGL14.eglCreateWindowSurface(mEglDisplay, mEglConfig, surface, attrib_list, 0);
        // 绑定当前线程的显示设备及上下文， 之后操作opengl，就是在这个虚拟显示上操作
        if (!EGL14.eglMakeCurrent(mEglDisplay,mEglSurface,mEglSurface,mEglContext)) {
            throw  new RuntimeException("eglMakeCurrent 失败！");
        }
        //像虚拟屏幕画
        mScreenFilter = new ScreenFilter(context);
        mScreenFilter.onReady(width,height);
    }

    private void createEGL(EGLContext eglContext) {
        //创建 虚拟显示器
        mEglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        if (mEglDisplay == EGL14.EGL_NO_DISPLAY) {
            throw new RuntimeException("eglGetDisplay failed");
        }
        //初始化显示器
        int[] version = new int[2];
        // 12.1020203
        //major：主版本 记录在 version[0]
        //minor : 子版本 记录在 version[1]
        if (!EGL14.eglInitialize(mEglDisplay, version, 0, version, 1)) {
            throw new RuntimeException("eglInitialize failed");
        }
        // egl 根据我们配置的属性 选择一个配置
        int[] attrib_list = {
                EGL14.EGL_RED_SIZE, 8, // 缓冲区中 红分量 位数
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT, //egl版本 2
                EGL14.EGL_NONE
        };

        EGLConfig[] configs = new EGLConfig[1];
        int[] num_config = new int[1];
        // attrib_list：属性列表+属性列表的第几个开始
        // configs：获取的配置 (输出参数)
        //num_config: 长度和 configs 一样就行了
        if (!EGL14.eglChooseConfig(mEglDisplay, attrib_list, 0,
                configs, 0, configs.length, num_config, 0)) {
            throw new IllegalArgumentException("eglChooseConfig#2 failed");
        }
        mEglConfig = configs[0];
        int[] ctx_attrib_list = {
                EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, //egl版本 2
                EGL14.EGL_NONE
        };
        //创建EGL上下文
        // 3 share_context: 共享上下文 传绘制线程(GLThread)中的EGL上下文 达到共享资源的目的 发生关系
        mEglContext = EGL14.eglCreateContext(mEglDisplay, mEglConfig, eglContext, ctx_attrib_list
                , 0);
        // 创建失败
        if (mEglContext == EGL14.EGL_NO_CONTEXT) {
            throw new RuntimeException("EGL Context Error.");
        }
    }


    /**
     *
     * @param textureId 纹理id 代表一个图片
     * @param timestamp 时间戳
     */
    public void draw(int textureId,long timestamp){
        // 绑定当前线程的显示设备及上下文， 之后操作opengl，就是在这个虚拟显示上操作
        if (!EGL14.eglMakeCurrent(mEglDisplay,mEglSurface,mEglSurface,mEglContext)) {
            throw  new RuntimeException("eglMakeCurrent 失败！");
        }
        //画画 画到虚拟屏幕上
        mScreenFilter.onDrawFrame(textureId);
        //刷新eglsurface的时间戳
        EGLExt.eglPresentationTimeANDROID(mEglDisplay,mEglSurface,timestamp);

        //交换数据
        //EGL的工作模式是双缓存模式， 内部有两个frame buffer (fb)
        //当EGL将一个fb  显示屏幕上，另一个就在后台等待opengl进行交换
        EGL14.eglSwapBuffers(mEglDisplay,mEglSurface);
    }

    /**
     * 回收
     */
    public void release(){
        EGL14.eglDestroySurface(mEglDisplay, mEglSurface);
        EGL14.eglMakeCurrent(mEglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE,
                EGL14.EGL_NO_CONTEXT);
        EGL14.eglDestroyContext(mEglDisplay, mEglContext);
        EGL14.eglReleaseThread();
        EGL14.eglTerminate(mEglDisplay);
    }
}


```

