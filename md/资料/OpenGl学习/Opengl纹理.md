 前面的基础上，增加物体的表面贴图，使得物体更加好看。

**纹理概念**

纹理用来表示图像照片或者说一系列的数据，使用纹理可以使物体用用更多的细节。OpenGL ES 2.0 中有两种贴图：二维纹理和立方体纹理。

每个二维纹理都由许多小的纹理元素组成，类似与片元和像素，使用纹理最简单的方式就是直接从一个图像加载数据。在OpenGL中规定纹理图像的左下角由stst坐标(0.0,0.0)指定，右上角由stst坐标(1.0,1.0)指定，不过超过1.0的坐标也是允许的，在该区间之外的纹理在读取时的时候由纹理拉伸模式决定。

OpenGL ES 2.0不必是正方形，但是每个维度都应该是2的幂

在Android中使用的OpenGL ES的纹理坐标系跟官方的纹理坐标系统不一样，在Android中使用官方的纹理坐标系统，得到的结果是相反的，而是左上角是stst坐标(0.0,0.0)点，右下角是stst坐标(1.0,1.0)点。

二维纹理映射的原理

![img](https://img.jbzj.com/file_images/article/201805/201852792133993.jpg?201842792142)

使用纹理就是在纹理图中进行采样，因此需要将选定的纹理坐标穿进顶点着色器，经过插值在片元着色器中从纹理图中的指定位置采样即可，纹理图的数据通过往片元插值器传递纹理单元指定的。

**纹理对象和纹理加载**

创建一个纹理对象，保存渲染所需的纹理数据，例如图像数据、过滤模式、包装模式。创建生成纹理对象的函数

```
public` `static` `native` `void` `glGenTextures(``  ``int` `n, ``// 指定要生成的纹理对象的数量``  ``int``[] textures, ``// 保存纹理对象ID的数组``  ``int` `offset`` ``);
```

纹理对象在应用程序中不再使用时，需要删除。

```
public` `static` `native` `void` `glDeleteTextures(``  ``int` `n, ``// 指定要删除的纹理数量``  ``int``[] textures, ``// 保存待删除的纹理ID的数组``  ``int` `offset`` ``);
```

纹理对象的 ID 必须是 glGenTextures 产生的，一旦生成纹理ID，就必须绑定纹理对象才能继续进行后续的操作。后续的操作将影响绑定的纹理对象。一旦纹理被绑定到一个特定的纹理目标，再删除之前就一直保持着绑定状态。

```
public` `static` `native` `void` `glBindTexture(``  ``int` `target, ``// 绑定纹理对象到目标 GL_TEXTURE_2D 或 GL_TEXTURE_CUBE_MAP``  ``int` `texture ``// 要绑定的纹理对象ID`` ``);
```

激活某个纹理单元

```
public` `static` `native` `void` `glActiveTexture(``  ``int` `texture ``// 要激活的纹理单元`` ``);
```

对这两个函数的理解：显卡中有N个纹理单元（GL_TEXTURE0，GL_TEXTURE1，GL_TEXTURE2…），每个纹理单元中保存着很多纹理目标（targetTexture1D，targetTexture2D，targetTexture3D，targetTextureCube…），OpenGL ES 2.0貌似只支持了targetTexture2D和targetTextureCube。

纹理单元TextureUnit的定义如下

```
struct TextureUnit``{`` ``GLuint targetTexture1D;`` ``GLuint targetTexture2D;`` ``GLuint targetTexture3D;`` ``GLuint targetTextureCube;`` ``...``};
```

glActiveTexture函数就是设置当前活动的纹理单元

```
TextureUnit textureUnits[GL_MAX_TEXTURE_IMAGE_UNITS]``GLuint currentTextureUnit = ``0``;``// ...``void` `glActiveTexture(GLenum textureUnit)``{`` ``currentTextureUnit = textureUnit - GL_TEXTURE0 ;``}
```

glBindTexture函数就是将纹理对象ID赋值给当前活动的纹理单元的对应的目标纹理。

```
void` `glBindTexture(GLenum textureTarget, GLuint textureObject)``{`` ``TextureUnit *texUnit = &textureUnits[currentTextureUnit];`` ``switch``(textureTarget)`` ``{`` ``case` `GL_TEXTURE_1D: texUnit->targetTexture1D = textureObject; ``break``;`` ``case` `GL_TEXTURE_2D: texUnit->targetTexture2D = textureObject; ``break``;`` ``case` `GL_TEXTURE_3D: texUnit->targetTexture3D = textureObject; ``break``;`` ``case` `GL_TEXTURE_CUBEMAP: texUnit->targetTextureCube = textureObject; ``break``;`` ``}``}
```

获取一副图片的纹理数据

```
public` `static` `void` `texImage2D(``int` `target, ``// 常数GL_TEXTURE_2D``        ``int` `level, ``// 表示多级分辨率的纹理图像的级数，若只有一种分辨率，则level设为0。``        ``Bitmap bitmap,``        ``int` `border ``// 边框，一般设为0 ``        ``)
```

其他纹理选项的设置使用glTexParameterf系列函数

```
public` `static` `native` `void` `glTexParameterf(``  ``int` `target, ``  ``int` `pname, ``// 设定的参数，可以是GL_TEXTURE_MAG_FILTER,GL_TEXTURE_MIN_FILTER,GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T``  ``float` `param ``// 参数对应的值`` ``);
```

**应用纹理的例子**

对前面的立方体的每个面应用一张图片作为纹理贴图，效果图（这个纹理图是哪个老师来着？）

![img](https://img.jbzj.com/file_images/article/201805/201852792150435.gif?201842792159)

Rectangle.java

### 载入纹理的步骤：

- #### GLES20.glGenTextures() : 生成纹理资源的句柄

- #### GLES20.glBindTexture(): 绑定句柄

- #### GLUtils.texImage2D() ：将bitmap传递到已经绑定的纹理中

- #### GLES20.glTexParameteri() ：设置纹理属性，过滤方式，拉伸方式等

### 纹理的数据来源

- #### 将Bitmap以纹理的形式载入OpenGL中：

  

  ```dart
  /**
   * @param textureTarget Texture类型。
   *                      1. 相机用 GLES11Ext.GL_TEXTURE_EXTERNAL_OES
   *                      2. 图片用GLES20.GL_TEXTURE_2D
   * @param minFilter     缩小过滤类型 (1.GL_NEAREST ; 2.GL_LINEAR)
   * @param magFilter     放大过滤类型
   * @param wrapS         X方向边缘环绕
   * @param wrapT         Y方向边缘环绕
   * @return 返回创建的 Texture ID
   */
  public static int createTexture(int textureTarget, @Nullable Bitmap bitmap, int minFilter,
                                  int magFilter, int wrapS, int wrapT) {
      int[] textureHandle = new int[1];
  
      GLES20.glGenTextures(1, textureHandle, 0);
      checkGlError("glGenTextures");
      GLES20.glBindTexture(textureTarget, textureHandle[0]);
      checkGlError("glBindTexture " + textureHandle[0]);
      GLES20.glTexParameterf(textureTarget, GLES20.GL_TEXTURE_MIN_FILTER, minFilter);
      GLES20.glTexParameterf(textureTarget, GLES20.GL_TEXTURE_MAG_FILTER, magFilter); //线性插值
      GLES20.glTexParameteri(textureTarget, GLES20.GL_TEXTURE_WRAP_S, wrapS);
      GLES20.glTexParameteri(textureTarget, GLES20.GL_TEXTURE_WRAP_T, wrapT);
  
      if (bitmap != null) {
          GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
      }
  
      checkGlError("glTexParameter");
      return textureHandle[0];
  }
  ```

- #### 将 RGB 以纹理的形式载入OpenGL中：

  

  ```dart
  /***
  * 将 RGB 数据 转化 成纹理 ID
  * @param videoW 纹理宽
  * @param videoH 纹理高
  * @param rgbData 视频桢的 RGB 数据
  * @param textureId 因为视频根据帧率刷新，频繁调用onDrawFrame,
  *                     所以不适合多次创建纹理资源ID，
  *                     所以最好在onCreate创建好，免得OOM
  * @return 绑定好 RGB 数据的纹理 ID
  */
  public static int generateRGBTexture(int videoW, int videoH, byte[] rgbData, int textureId) {
        if (rgbData == null) {
            return -1;
        }
  
        ByteBuffer colorByteBuffer = null;
  
        if (colorByteBuffer == null) {
            colorByteBuffer = ByteBuffer.allocate(videoW * videoH * 4);
        }
  
        //生成纹理ID
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        if (colorByteBuffer != null) {
            colorByteBuffer.clear();
            colorByteBuffer.put(rgbData, 0, videoW * videoH * 4);
        } else {
            return -1;
        }
  
        colorByteBuffer.position(0);
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0,
                  GLES20.GL_RGBA, videoW, videoH, 0,
                  GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, colorByteBuffer);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
  
        return textureId;
  }
  ```



