## 1 OpenGL渲染

上一章节我们通过[SurfaceFlinger](https://so.csdn.net/so/search?q=SurfaceFlinger&spm=1001.2101.3001.7020)拿到Surface进行图像绘制，这节课我们通过GLSurfaceView来进行绘制，把摄像头的数据采集后展示渲染在屏幕上，这种方式是在GPU进行处理和绘制。

### 1.1 渲染使用GLSurfaceView

自定义CarView继承GLSurfaceView  
CarView.java

```
package com.example.carscreen;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.util.Log;

import androidx.camera.core.Preview;
import androidx.lifecycle.LifecycleOwner;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class CarView
        extends GLSurfaceView
        implements GLSurfaceView.Renderer, Preview.OnPreviewOutputUpdateListener {

    ScreenFilter screenFilter;

    private SurfaceTexture mCameraTexture;

    /**
     * 在GPU的位置（第几个图层，共32个图层随便传一个都行）
     */
    private int textures = 0;

    public CarView(Context context) {
        super(context);
    }

    public CarView(Context context, AttributeSet attrs) {
        super(context, attrs);
        CameraHelper cameraHelper = new CameraHelper((LifecycleOwner) getContext(), this);
        // 设置稳定版：2
        setEGLContextClientVersion(2);
        setRenderer(this);
    }

    @Override
    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {
        // 图像，图层
        mCameraTexture.attachToGLContext(textures);
        // 数据过来，我们去onFrameAvailable
        mCameraTexture.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
            @Override
            public void onFrameAvailable(SurfaceTexture surfaceTexture) {
                // 调用了requestRender()相当于View的invilidate()
                // onDrawFrame()相当于onDraw（）
                // requestRender()被调用后，onDrawFrame()方法被调用
                requestRender();
            }
        });

        screenFilter = new ScreenFilter(getContext());
    }

    @Override
    public void onSurfaceChanged(GL10 gl10, int i, int i1) {

    }

    /**
     * 这个渲染方法会不断的被回调：
     * 1.手动触发（调用requestRender()方法）
     * 2.被动触发（16ms一次）
     *
     * @param gl10
     */
    @Override
    public void onDrawFrame(GL10 gl10) {
        Log.d("yvan", "---->1");
        // 获取最新camera数据
        mCameraTexture.updateTexImage();
        screenFilter.onDraw(getWidth(), getHeight(), textures);
    }

    /**
     * 摄像头有画面进来，就会被调用。
     * 约20-30帧/s，跟摄像头有关系（设置了输出就会被调用）
     *
     * @param output
     */
    @Override
    public void onUpdated(Preview.PreviewOutput output) {
        // 数据源
        mCameraTexture = output.getSurfaceTexture();
    }
}
```

### 1.2 Camera2展示摄像头采集数据

在项目的build.gradle添加Camera2库，用于展示摄像头采集的数据：

```
dependencies {
...
    implementation "androidx.camera:camera-core:1.0.0-alpha05"
    implementation "androidx.camera:camera-camera2:1.0.0-alpha05"
}
```

下面我们创建一个Camera管理类。CameraX.bindToLifecycle绑定预览，Preview数据在GPU, YUV在CPU。

CameraHelper.java

```
package com.example.carscreen;

import android.util.Size;

import androidx.camera.core.CameraX;
import androidx.camera.core.Preview;
import androidx.camera.core.PreviewConfig;
import androidx.lifecycle.LifecycleOwner;

public class CameraHelper {

    private Preview.OnPreviewOutputUpdateListener listener;

    public CameraHelper(LifecycleOwner lifecycleOwner, Preview.OnPreviewOutputUpdateListener listener) {
        this.listener = listener;
        // 绑定预览
        CameraX.bindToLifecycle(lifecycleOwner, getPreView());
    }

    private Preview getPreView() {
        // 设置配置
        PreviewConfig previewConfig = new PreviewConfig.Builder()
                // 大小
                .setTargetResolution(new Size(640, 480))
                // 后置摄像头
                .setLensFacing(CameraX.LensFacing.BACK)
                .build();
        Preview preview = new Preview(previewConfig);
        // 监听摄像头更新
        preview.setOnPreviewOutputUpdateListener(listener);
        return preview;
    }
}

```

### 1.3 片元程序和顶点程序

下面我们在raw文件夹中创建两个文件：片元和顶点  
camera\_frag.glsl

```
#extension GL_OES_EGL_image_external : require
precision lowp float;
//接受的坐标
varying vec2 aCoord;
//[]  对象 片元
uniform samplerExternalOES vTexture;
// 顶点 形状     这么确定形状
void main() {

    float y= aCoord.y;
    if(y<0.5)
    {
        y+=0.25;
    }else{
        y -= 0.25;
    }
    gl_FragColor= texture2D(vTexture, vec2( y,aCoord.x));

//    vec4 src = texture2D(vTexture, aCoord);
//    gl_FragColor = src;
}

```

camera\_vert.glsl

```
//确定形状vec4   vec

attribute  vec4 vPosition;

//android坐标系  屏幕
attribute vec4 vCoord;

//怎么输出
varying vec2 aCoord;

//vec4  =  [{-1,1},{-1,1},{-1,1},{-1,1},]
void main() {
//    GPU程序内置的系统变量     GPU  矩形
    gl_Position = vPosition;
    aCoord=vCoord.xy;
}

```

### 1.4 加载编译片元程序和顶点程序

然后开启读取片元和顶点文件，用于OpenGL的渲染需要进行散步走  
1.创建片元/顶点程序  
2.加载片元/顶点程序  
3.编译

ScreenFilter.java

```
package com.example.carscreen;

import android.content.Context;
import android.opengl.GLES20;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;

public class ScreenFilter {

    FloatBuffer vertexBuffer;
    FloatBuffer textureBuffer;

    int vTexture;

    //4 *2 * 4  视频播放   opengl     视频   旋转   gpu
    float[] VERTEX = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f, 1.0f,
            1.0f, 1.0f
    };
    float[] TEXTURE = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f
    };

    int program;
    int vPosition;

    int vCoord;
//   String gVertexShader =
//            "attribute vec4 vPosition;\n"+
//            "void main() {\n"+
//            "  gl_Position = vPosition;\n"+
//            "}\n";
  字符串
//String gFragmentShader =
//            "precision mediump float;\n"+
//            "void main() {\n"+
//            "  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"+
//            "}\n";


    // 准备工作
    public ScreenFilter(Context context) {
        // 对象数组作为入参、出参
        int[] status = new int[1];
        
        // 1.顶点程序
        String vertexSharder = readRawTextFile(context, R.raw.camera_vert);
        int vShader = GLES20.glCreateShader(GLES20.GL_VERTEX_SHADER);
        //加载 顶点程序代码
        GLES20.glShaderSource(vShader, vertexSharder);
        //编译（配置）
        GLES20.glCompileShader(vShader);
        //查看配置 是否成功
        GLES20.glGetShaderiv(vShader, GLES20.GL_COMPILE_STATUS, status, 0);
        if (status[0] != GLES20.GL_TRUE) {
            //失败
            throw new IllegalStateException("load vertex shader:" + GLES20.glGetShaderInfoLog
                    (vShader));
        }

        // 2.片元程序
        String fragSharder = readRawTextFile(context, R.raw.camera_frag);
        int fShader = GLES20.glCreateShader(GLES20.GL_FRAGMENT_SHADER);
        //加载着色器代码
        GLES20.glShaderSource(fShader, fragSharder);
        //编译（配置）
        GLES20.glCompileShader(fShader);
        //查看配置 是否成功
        GLES20.glGetShaderiv(fShader, GLES20.GL_COMPILE_STATUS, status, 0);
        if (status[0] != GLES20.GL_TRUE) {
            //失败
            throw new IllegalStateException("load fragment shader:" + GLES20.glGetShaderInfoLog
                    (vShader));
        }
        
        program = GLES20.glCreateProgram();
        // 执行 这个程序 加载顶点程序和片元程序
        GLES20.glAttachShader(program, vShader);
        GLES20.glAttachShader(program, fShader);
        //链接着色器程序
        GLES20.glLinkProgram(program);

        // 在CPU中定位到GPU中，变量的位置
        vPosition = GLES20.glGetAttribLocation(program, "vPosition");
        vCoord = GLES20.glGetAttribLocation(program, "vCoord");
        // 采样点的坐标
        vTexture = GLES20.glGetUniformLocation(program, "vTexture");
        // 开辟传送通道
        vertexBuffer = ByteBuffer.allocateDirect(4 * 4 * 2).
                order(ByteOrder.nativeOrder()).asFloatBuffer();
        vertexBuffer.clear();
        // 把数据放进去   还没有穿过去
        vertexBuffer.put(VERTEX);

        textureBuffer = ByteBuffer.allocateDirect(4 * 4 * 2).order(ByteOrder.nativeOrder())
                .asFloatBuffer();
        textureBuffer.clear();
        textureBuffer.put(TEXTURE);
    }


    // texture  数据源的地方
    public void onDraw(int mWidth, int mHeight, int texture) {
        GLES20.glViewport(0, 0, mWidth, mHeight);
        GLES20.glUseProgram(program);

        // 起始位置 0
        vertexBuffer.position(0);
        textureBuffer.position(0);

        GLES20.glVertexAttribPointer(vPosition, 2, GLES20.GL_FLOAT,
                false, 0, vertexBuffer);
        // 激活变量
        GLES20.glEnableVertexAttribArray(vPosition);
        GLES20.glVertexAttribPointer(vCoord, 2, GLES20.GL_FLOAT,
                false, 0, textureBuffer);
        GLES20.glEnableVertexAttribArray(vCoord);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        // texture图层
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texture);
        // 激活的意思
        GLES20.glUniform1i(vTexture, 0);
        // 通知gpu绘制
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    }

    public String readRawTextFile(Context context, int rawId) {
        InputStream is = context.getResources().openRawResource(rawId);
        BufferedReader br = new BufferedReader(new InputStreamReader(is));
        String line;
        StringBuilder sb = new StringBuilder();
        try {
            while ((line = br.readLine()) != null) {
                sb.append(line);
                sb.append("\n");
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        try {
            br.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
        return sb.toString();
    }

}
```

### 1.5 页面展示

最后我们展示在页面中  
MainActivity.java

```
package com.example.carscreen;

import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        checkPermission();
    }
    public boolean checkPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && checkSelfPermission(
                Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    Manifest.permission.CAMERA
            }, 1);
        }
        return false;
    }
}
```

activity\_main.xml

```
<?xml version="1.0" encoding="utf-8"?>
<androidx.constraintlayout.widget.ConstraintLayout xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:app="http://schemas.android.com/apk/res-auto"
    xmlns:tools="http://schemas.android.com/tools"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    tools:context=".MainActivity">

    <com.example.carscreen.CarView
        android:layout_width="match_parent"
        android:layout_height="match_parent" />

</androidx.constraintlayout.widget.ConstraintLayout>
```

AndroidManifest.xml

```
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools">
    <uses-permission android:name="android.permission.CAMERA"/>
    <application
        android:allowBackup="true"
        android:dataExtractionRules="@xml/data_extraction_rules"
        android:fullBackupContent="@xml/backup_rules"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:roundIcon="@mipmap/ic_launcher_round"
        android:supportsRtl="true"
        android:theme="@style/Theme.carscreen"
        tools:targetApi="31">
        <activity
            android:name=".MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />

                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>

            <meta-data
                android:name="android.app.lib_name"
                android:value="" />
        </activity>
    </application>

</manifest>
```

## 2 CPU和GPU区别

上面我们提到CPU和GPU，那么CPU和GPU分别是什么？我们来大概了解一下：

> 1.  GPU：计算量大，但没什么技术含量，而且要重复很多很多次。就像你有个工作需要算几亿次一百以内加减乘除一样，最好的办法就是雇上几十个小学生一起算，一人算一部分，反正这些计算也没什么技术含量，纯粹体力活而已。
> 2.  CPU：像老教授，积分微分都会算，就是工资高，一个老教授资顶二十个小学生，你要是富士康你雇哪个？GPU就是这样，用很多简单的计算单元去完成大量的计算任务，纯粹的人海战术。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/828a5a807a76c5f3712e4e05934088cb.png)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b0e30f35a6bfa7a2ca6aeb22aaabefa9.png)  
**对比CPU和GPU上面的图片，我们可以知道GPU中ALU(运算能力)比较多，可计算简单的算术，运算快，适合密码学、挖矿、图形学等需要并行计算，无依赖性、互相独立的场合；CPU计算复杂的算术，运算慢，适合武器装备、信息化等需要复杂逻辑控制的场合；**

## 3 总结

本章使用OpenGL实现了摄像头[数据采集](https://so.csdn.net/so/search?q=%E6%95%B0%E6%8D%AE%E9%87%87%E9%9B%86&spm=1001.2101.3001.7020)和渲染在屏幕上，也大概了解了一下CPU和GPU相关的知识，目前只是实现了OpenGL渲染部分，车载智能坐舱分屏显示可通过该方案加入分屏显示来实现。[项目地址](https://github.com/zhuyingfa/CarScreen)