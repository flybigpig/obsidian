#### OpenGL

​	OpenGL是一种图形应用程序编程接口(Application Programming Interface，API)。它是一种可以对图形硬件设备特性进行访问的软件库，OpenGL被设计为一个现代化的、硬件无关的接口，因此我们可以在不考虑计算机操作系统或窗口系统的前提下，在多种不同的图形硬件系统上，完全**通过软件的方式实现OpenGL的接口**。

#### OpenGL ES

- OpenGL® ES is a royalty-free, cross-platform API for rendering advanced 2D and 3D graphics on embedded and mobile systems - including consoles, phones, appliances and vehicles. It consists of a well-defined subset of desktop OpenGL suitable for low-power devices, and provides a flexible and powerful interface between software and graphics acceleration hardware.
- 是OpenGL的子集，本质上是编程接口规范。
- OpenGL与OpenGL ES的主要区别，在于OpenGL ES主要针对嵌入式设备使用

#### OpenGLES 3.0

- OpenGLES 3.0 实际上是 OpenGLES 2.0 的扩展版本，向下兼容 OpenGLES 2.0 ，但不兼容 OpenGLES 1.0 。

#### 图形API汇总

- **OpenGL（Open Graphics Library）**：是一个跨编程语言、跨平台的编程图形程序接口，它将计算机的资源抽象称为一个个OpenGL的对象，对这些资源的操作抽象为一个个的OpenGL指令。
- **OpenGL ES（OpenGL for Embedded Systems）**：是OpenGL三维图形API的子集，针对手机、PDA和游戏主机等嵌入式设备而设计的，去除了许多不必要和性能较低的API接口。
- **DirectX**：是由很多API组成的，DirectX并不是一个单纯的图形API。最重要的是DirectX是属于Windows上一个多媒体处理API。并不支持Windows以外的平台，所以不是跨平台框架。按照性质分类，可分为四大部分，显示部分、声音部分、输入部分和网络部分。
- **Metal**：Apple为游戏开发者推出了新的平台技术，该技术能够为3D图像提高10倍的渲染性能。Metal是Apple为解决3D渲染而推出的框架。

#### 着色器与管线

**CPU**: 中央处理器,它集成了运算,缓冲,控制等单元,包括绘图功能.CPU将对象处理为多维图形,纹理(Bitmaps、Drawables等都是一起打包到统一的纹理)。

**GPU**:一个类似于CPU的专门用来处理Graphics的处理器, 作用用来帮助加快格栅化操作,当然,也有相应的缓存数据(例如缓存已经光栅化过的bitmap等)机制。

**OpenGL ES**：是手持嵌入式设备的3DAPI,跨平台的、功能完善的2D和3D图形应用程序接口API,有一套固定渲染管线流程. OpenGL ES详解

**DisplayList** 在Android把XML布局文件转换成GPU能够识别并绘制的对象。这个操作是在DisplayList的帮助下完成的。DisplayList持有所有将要交给GPU绘制到屏幕上的数据信息。

**格栅化** 是 将图片等矢量资源,转化为一格格像素点的像素图,显示到屏幕上。

**垂直同步VSYNC**:让显卡的运算和显示器刷新率一致以稳定输出的画面质量。它告知GPU在载入新帧之前，要等待屏幕绘制完成前一帧。下面的三张图分别是GPU和硬件同步所发生的情况,Refresh Rate:屏幕一秒内刷新屏幕的次数,由硬件决定,例如60Hz.而Frame Rate:GPU一秒绘制操作的帧数,单位是30fps,正常情况过程图如下：
![这里写图片描述](https://img.jbzj.com/file_images/article/201703/201703191136461.png)

**渲染机制分析渲染流程简介**

Android整体的绘制流程如下：

UI对象—->CPU处理为多维图形,纹理 —–通过OpeGL ES接口调用GPU—-> GPU对图进行光栅化(Frame Rate ) —->硬件时钟(Refresh Rate)—-垂直同步—->投射到屏幕

![这里写图片描述](https://img.jbzj.com/file_images/article/201703/201703191136462.png)

Android系统每隔16ms发出VSYNC信号(1000ms/60=16.66ms)，触发对UI进行渲染， 如果每次渲染都成功，这样就能够达到流畅的画面所需要的60fps，为了能够实现60fps，这意味着计算渲染的大多数操作都必须在16ms内完成。

渲染时间线

![这里写图片描述](https://img.jbzj.com/file_images/article/201703/201703191136463.png)

![这里写图片描述](https://img.jbzj.com/file_images/article/201703/201703191136474.png)

正常情况下Android的GPU会在16ms完成页面的绘制，如果一帧画面渲染时间超过16ms的时候,垂直同步机制会让显示器硬件 等待GPU完成栅格化渲染操作,然后再次绘制界面，这样就会看起来画面停顿。

当GPU渲染速度过慢,就会导致如下情况,某些帧显示的画面内容就会与上一帧的画面相同。

![这里写图片描述](https://img.jbzj.com/file_images/article/201703/201703191136475.png)

##### 1. GPU 渲染效率比 CPU 高

这点是  GPU、CPU 自身架构决定的，CPU 计算单元少，逻辑控制单元多，为的是执行维度、类型复杂的各种任务，毕竟 CPU 是中枢嘛~。GPU 计算单元多，逻辑控制单元少，专注于图形绘制、渲染

大家看图就明白了：

![img](https:////upload-images.jianshu.io/upload_images/1785445-03087190cfb540f7.jpeg?imageMogr2/auto-orient/strip|imageView2/2/w/997/format/webp)

ALU 是计算单元，剩下的不用说大家也能猜到，所以但是把绘图、渲染任务从 CPU 切到 GPU 都是性能上的巨大提升



####  OpenGl详解

 

![img](https://img2018.cnblogs.com/blog/703548/201904/703548-20190402212658795-1353723850.png)

​		图形渲染管线包含很多部分，每个部分都将在转换顶点数据到最终像素这一过程中处理各自特定的阶段。我们会概括性地解释一下渲染管线的每个部分，让你对图形渲染管线的工作方式有个大概了解。

首先，我们以数组的形式传递3个3D坐标作为图形渲染管线的输入，用来表示一个三角形，这个数组叫做顶点数据(Vertex Data)；

顶点数据是一系列顶点的集合。一个顶点(Vertex)是一个3D坐标的数据的集合。而顶点数据是用顶点属性(Vertex Attribute)表示的，它可以包含任何我们想用的数据，但是简单起见，我们还是假定每个顶点只由一个3D位置(译注1)和一些颜色值组成的吧。

##### 1，顶点着色器

它把一个单独的顶点作为输入。顶点着色器主要的目的是把3D坐标转为另一种3D坐标（后面会解释），同时顶点着色器允许我们对顶点属性进行一些基本处理。



​	**图元装配，**即将顶点根据primitive(原始的连接关系）还原成网格结构。网格由顶点和索引组成，在之前流水线中是对顶点的处理，在这个阶段是根据索引将顶点连接在一起，组成线、面单元。之后就是对超出屏幕外的三角形进行裁剪。

想象一下：一个三角形其中一个顶点在画面外，另外两个顶点在画面内，这时我们在屏幕上看到的就是一个四边形，然后将四边形切成两个三角形。

此外还有一个操作涉及到三角形的顶点顺序（其实也就是三角形的法向量朝向），根据右手定则来决定三角面片的法向量，如果该法向量朝向视点（法向量与到视点的方向的点积为正），该面是正面。



##### 2，图元装配阶段

将顶点着色器输出的所有顶点作为输入（如果是GL_POINTS，那么就是一个顶点），并所有的点装配成指定图元的形状；本节例子中是一个三角形。

##### 3，几何着色器

图元装配阶段的输出会传递给几何着色器(Geometry Shader)。几何着色器把图元形式的一系列顶点的集合作为输入，它可以通过产生新顶点构造出新的（或是其它的）图元来生成其他形状。例子中，它生成了另一个三角形。

##### 4，光栅化阶段

几何着色器的输出会被传入光栅化阶段(Rasterization Stage)，这里它会把图元映射为最终屏幕上相应的像素，生成供片段着色器(Fragment Shader)使用的片段(Fragment)。在片段着色器运行之前会执行裁切(Clipping)。裁切会丢弃超出你的视图以外的所有像素，用来提升执行效率。

##### 5，片段着色器

片段着色器的主要目的是计算一个像素的最终颜色，这也是所有OpenGL高级效果产生的地方。通常，片段着色器包含3D场景的数据（比如光照、阴影、光的颜色等等），这些数据可以被用来计算最终像素的颜色。

##### 6，测试与混合

在所有对应颜色值确定以后，最终的对象将会被传到最后一个阶段，我们叫做Alpha测试和混合(Blending)阶段。这个阶段检测片段的对应的深度（和模板(Stencil)）值（后面会讲），用它们来判断这个像素是其它物体的前面还是后面，决定是否应该丢弃。这个阶段也会检查alpha值（alpha值定义了一个物体的透明度）并对物体进行混合(Blend)。所以，即使在片段着色器中计算出来了一个像素输出的颜色，在渲染多个三角形的时候最后的像素颜色也可能完全不同。

####  精度限定:

glsl在进行光栅化着色的时候,会产生大量的浮点数运算,这些运算可能是当前设备所不能承受的,所以glsl提供了3种浮点数精度,我们可以根据不同的设备来使用合适的精度.

在变量前面加上 `highp` `mediump` `lowp` 即可完成对该变量的精度声明.

```
lowp float color;
varying mediump vec2 Coord;
lowp ivec2 foo(lowp mat3);
highp mat4 m;
```

我们一般在片元着色器(fragment shader)最开始的地方加上 `precision mediump float;` 便设定了默认的精度.这样所有没有显式表明精度的变量 都会按照设定好的默认精度来处理.

如何确定精度:

变量的精度首先是由精度限定符决定的,如果没有精度限定符,则要寻找其右侧表达式中,已经确定精度的变量,一旦找到,那么整个表达式都将在该精度下运行.如果找到多个, 则选择精度较高的那种,如果一个都找不到,则使用默认或更大的精度类型.

```
uniform highp float h1;
highp float h2 = 2.3 * 4.7; //运算过程和结果都 是高精度
mediump float m;
m = 3.7 * h1 * h2; //运算过程 是高精度
h2 = m * h1; //运算过程 是高精度
m = h2 – h1; //运算过程 是高精度
h2 = m + m; //运算过程和结果都 是中等精度
void f(highp float p); // 形参 p 是高精度
f(3.3); //传入的 3.3是高精度
```

#### 变量使用

### void

表示空，用于无返回值的函数。比如顶点shader中main函数：

```text
void main() {
    gl_Position = vPosition;
}
```

### float、int、bool

分别代表浮点型，整型，布尔型。定义各类型变量如下：

```text
float f = 1.0;
int i =0;
bool b = true;
```

### vec2、vec3、vec4

分别包含2、3、4个float类型的向量。定义变量如下：

```text
vec2 v2 = vec2(1.0, 0.0);
vec3 v3 = vec3(1.0, 0.0, 0.5);
vec4 v4 = vec4(1.0, 0.0, 0.5,0.5);
```

如果只给一个参数，则向量中其他值也会使用此值，比如给vec4一个1.0的值：

```text
vec4 v5 = vec4(1.0);
```

等价于：

```text
vec4 v5 = vec4(1.0,1.0,1.0,1.0);
```

注意提供给向量的参数只能是1个或者对应向量个数，比如vec4类型不能提供2个参数：

```text
vec4 v6 = vec4(1.0,1.0);
```

上面给vec4提供2个参数的写法是错误的。 经常使用的内置变量gl_Position和gl_FragColor就是vec4类型，在片段shader中设置颜色：

```text
gl_FragColor = vec4(1.0,0.0,0.0,1.0);
```

ivec2, ivec3, ivec4和vec2、vec3、vec4用法一样，区别就是ivec的参数是int类型。 bvec2, bvec3, bvec4和vec2、vec3、vec4用法一样，区别就是bvec的参数是bool类型。

##### 顶点着色器

```
attribute vec4 vPosition;
void main() {
        gl_Position = vPosition;
};
```



```
precision mediump float; 
uniform vec4 vColor; 
void main() { 
     gl_FragColor = vColor; 
}                 
```



### 内置变量



着色器语言在GPU的着色器单元执行，javascript语言、C语言在CPU上执行，任何一种语言的语法规则，整体设计都和它执行的硬件有一定的关系，GPU和CPU执行程序的硬件单元既有相同点，也有不同点。这里谈到GPU和CPU不是为了讲解硬件，而是为了提醒大家，学习着色器语言有些语法可以参考javascript、C等执行在CPU上的语言，比如if语句、for语句、浮点数、布尔值，有些语法完全没必要参考javascript、C等执行在CPU上的语言，比如着色器语言中的内置变量`gl_PointSize`、`gl_Position`、`gl_FragColor`等等，声明一些变量使用的关键字`attribute`、`uniform`、`varying`。学习着色器语言的时候，如果你有兴趣可以深入研究GPU，对于WebGL学习来说，把GPU当成一个黑箱就可以，你只需要学会着色器语言的编程规则即可

普通变量，着色器语言和javascript语言一样需要先声明后使用，所谓内置变量就是不用声明可以直接赋值，主要是为了实现特定的功能。

| 内置变量      | 含义                               | 值数据类型 |
| ------------- | ---------------------------------- | ---------- |
| gl_PointSize  | 点渲染模式，方形点区域渲染像素大小 | float      |
| gl_Position   | 顶点位置坐标                       | vec4       |
| gl_FragColor  | 片元颜色值                         | vec4       |
| gl_FragCoord  | 片元坐标，单位像素                 | vec2       |
| gl_PointCoord | 点渲染模式对应点像素坐标           | vec2       |

### `gl_PointSize`

当WebGL执行绘制函数`gl.drawArrays()`绘制模式是点模式`gl.POINTS`的时候，顶点着色器语言`main`函数中才会用到内置变量`gl_PointSize`，使用内置变量`gl_PointSize`主要是用来设置顶点渲染出来的方形点像素大小。

```javascript
void main() {
  //给内置变量gl_PointSize赋值像素大小，注意值是浮点数
  gl_PointSize=20.0;
}
1234
//绘制函数绘制模式：点gl.POINTS
gl.drawArrays(gl.POINTS,0,点数量);
12
```

## `gl_Position`

`gl_Position`内置变量主要和顶点相关，出现的位置是顶点着色器语言的`main`函数中。`gl_Position`内置变量表示最终传入片元着色器片元化要使用的顶点位置坐标。

如果只有一个顶点，直接在给顶点着色器中设置内置变量`gl_Position`赋值就可以，内置变量`gl_Position`的值是四维向量`vec4(x,y,z,1.0)`,前三个参数表示顶点的xyz坐标值，第四个参数是浮点数`1.0`。

```javascript
void main() {
  //顶点位置，位于坐标原点
  gl_Position = vec4(0.0,0.0,0.0,1.0);
}
1234
```

如果你想完全理解内置变量`gl_Position`，必须建立`逐顶点`的概念，如果javascript语言中出现一个变量赋值，你可以理解为仅仅执行一次，但是对于着色器中不能直接这么理解，如果有多个顶点，你可以理解为每个顶点都要执行一遍顶点着色器主函数`main`中的程序。

多个顶点的时候，内置变量`gl_Position`对应的值是`attribute`关键字声明的顶点位置坐标变量`apos`，顶点位置坐标变量`apos`变量对应了javascript代码中多个顶点位置数据。

```html
<!-- 顶点着色器源码 -->
<script id="vertexShader" type="x-shader/x-vertex">
  //attribute声明vec4类型变量apos
  attribute vec4 apos;
  void main() {
    //顶点坐标apos赋值给内置变量gl_Position
    //逐顶点处理数据
    gl_Position = apos;
  }
</script>
12345678910
```

`逐顶点`处理的案例：WebGL的每一个顶点位置坐标都会通过平移矩阵`m4`进行矩阵变换，相当于批量操作所有的顶点数据，进行了平移，只是平移的计算通过矩阵乘法运算完成的而已。所谓的`逐顶点`，在这里体现的就是每一个顶点都会执行`main`函数中的矩阵变换。你可以参照生活的流水线去理解，比如多个同样的设备从我这里经过，我会分别对他们进行同样的操作，比如安装一个零件。

```html
<!-- 顶点着色器源码 -->
<script id="vertexShader" type="x-shader/x-vertex">
  //attribute声明vec4类型变量apos
  attribute vec4 apos;
  void main() {
    //创建平移矩阵(沿x轴平移-0.4)
    //1   0   0  -0.4
    //0   1   0    0
    //0   0   1    0
    //0   0   0    1
    mat4 m4 = mat4(1,0,0,0,  0,1,0,0,  0,0,1,0,  -0.4,0,0,1);
    //平移矩阵m4左乘顶点坐标(vec4类型数据可以理解为线性代数中的nx1矩阵，即列向量)
    // 逐顶点进行矩阵变换
    gl_Position = m4*apos;
  }

</script>
1234567891011121314151617
```

#### gl_Position的顶点数据传递

`attribute`声明的顶点变量数据如何通过javascript的WebGL API批量传递所有顶点数据。

```html
<script>
    //顶点着色器源码
    var vertexShaderSource = document.getElementById( 'vertexShader' ).innerText;
    //片元着色器源码
    var fragShaderSource = document.getElementById( 'fragmentShader' ).innerText;
    //初始化着色器
    var program = initShader(gl,vertexShaderSource,fragShaderSource);
    //获取顶点着色器的位置变量apos，即aposLocation指向apos变量。
    var aposLocation = gl.getAttribLocation(program,'apos');

    //类型数组构造函数Float32Array创建顶点数组
    var data=new Float32Array([0.5,0.5,-0.5,0.5,-0.5,-0.5,0.5,-0.5]);

    //创建缓冲区对象
    var buffer=gl.createBuffer();
    //绑定缓冲区对象,激活buffer
    gl.bindBuffer(gl.ARRAY_BUFFER,buffer);
    //顶点数组data数据传入缓冲区
    gl.bufferData(gl.ARRAY_BUFFER,data,gl.STATIC_DRAW);
    //缓冲区中的数据按照一定的规律传递给位置变量apos
    gl.vertexAttribPointer(aposLocation,2,gl.FLOAT,false,0,0);
    //允许数据传递
    gl.enableVertexAttribArray(aposLocation);
...
</script>
12345678910111213141516171819202122232425
```

### `gl_FragColor`

`gl_FragColor`内置变量主要用来设置片元像素的颜色，出现的位置是片元着色器语言的`main`函数中。

内置变量`gl_Position`的值是四维向量`vec4(r,g,b,a)`,前三个参数表示片元像素颜色值RGB，第四个参数是片元像素透明度A，`1.0`表示不透明,`0.0`表示完全透明。

```javascript
// 片元颜色设置为红色
gl_FragColor = vec4(1.0,0.0,0.0,1.0);
12
```

理解内置变量`gl_Position`需要建立`逐顶点`的概念，对于内置变量`gl_FragColor`而言，需要建立`逐片元`的概念。顶点经过片元着色器片元化以后，得到一个个片元，或者说像素点，然后通过内置变量`gl_FragColor`给每一个片元设置颜色值，所有片元可以使用同一个颜色值，也可能不是同一个颜色值，可以通过特定算法计算或者纹理像素采样。

根据位置设置渐变色

```javascript
  void main() {
    // 片元沿着x方向渐变
    gl_FragColor = vec4(gl_FragCoord.x/500.0*1.0,1.0,0.0,1.0);
  }
1234
```

纹理采样

```javascript
// 接收插值后的纹理坐标
varying vec2 v_TexCoord;
// 纹理图片像素数据
uniform sampler2D u_Sampler;
void main() {
  // 采集纹素，逐片元赋值像素值
  gl_FragColor = texture2D(u_Sampler,v_TexCoord);
}
12345678
```

## 片元坐标`gl_FragCoord`

内置变量`gl_FragCoord`表示WebGL在canvas画布上渲染的所有片元或者说像素的坐标，坐标原点是canvas画布的左上角，x轴水平向右，y竖直向下，`gl_FragCoord`坐标的单位是像素，`gl_FragCoord`的值是`vec2(x,y)`,通过`gl_FragCoord.x`、`gl_FragCoord.y`方式可以分别访问片元坐标的纵横坐标。

![在这里插入图片描述](https://img-blog.csdnimg.cn/201911172044270.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3UwMTQyOTE5OTA=,size_16,color_FFFFFF,t_70)

下面代码是把canvas画布上不同区域片元设置为不同颜色。

```html
<!-- 片元着色器源码 -->
<script id="fragmentShader" type="x-shader/x-fragment">
  void main() {
    // 根据片元的x坐标，来设置片元的像素值
    if(gl_FragCoord.x < 300.0){
      // canvas画布上[0,300)之间片元像素值设置
      gl_FragColor = vec4(1.0,0.0,0.0,1.0);
    }else if (gl_FragCoord.x <= 400.0) {
      // canvas画布上(300,400]之间片元像素值设置
      gl_FragColor = vec4(0.0,1.0,0.0,1.0);
    }else {
      // canvas画布上(400,500]之间片元像素值设置
      gl_FragColor = vec4(0.0,0.0,1.0,1.0);
    }    
    // 所有片元设置为红色
    // gl_FragColor = vec4(1.0,0.0,0.0,1.0);
  }
</script>
123456789101112131415161718
```

片元的颜色随着坐标变化(设置一个渐变色效果)

```html
<!-- 片元着色器源码 -->
<script id="fragmentShader" type="x-shader/x-fragment">
  void main() {
    // 片元沿着x方向渐变
    gl_FragColor = vec4(gl_FragCoord.x/500.0*1.0,1.0,0.0,1.0);
  }
</script>
1234567
```

## 渲染点片元坐标`gl_PointCoord`

如果你想了解内置变量`gl_PointCoord`表示的坐标含义，就需要了解WebGL绘制函数`gl.drawArrays()`的绘制模式参数`gl.POINTS`。

绘制函数`gl.drawArrays()`绘制模式参数设置为点渲染模式`gl.POINTS`，WebGL会把顶点渲染为一个方形区域，在顶点着色器代码中可以通过内置变量`gl_PointSize`设置顶点渲染的方向区域像素大小。

一个顶点渲染为一个方形区域，每个方形区域可以以方向区域的左上角建立一个直角坐标系，然后使用内置变量`gl_PointCoord`描述每个方形区域中像素或者说片元的坐标，比如方形区域的左上角坐标是`(0.0,0.0)`,每个方形区域几何中心坐标是`(0.5,0.5)`，右下角坐标是`(1.0,1.0)`。

注意内置变量`gl_PointCoord`和`gl_FragCoord`表示的像素坐标含义不同,查看下图表示。
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191117204356971.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3UwMTQyOTE5OTA=,size_16,color_FFFFFF,t_70)

```javascript
// 点绘制模式渲染10个顶点
gl.drawArrays(gl.POINTS,0,10);
12
```

顶点着色器中通过内置变量`gl_PointSize`设置点渲染的方形区域像素尺寸。

```javascript
void main() {
  //点渲染的方形区域像素大小
  gl_PointSize = 20.0;
  ...
}
12345
```

### `gl_PointCoord`应用案例

`gl.POINTS`绘制模式点默认渲染效果是方形区域，通过下面片元着色器代码设置可以把默认渲染效果更改为圆形区域。

```html
<!-- 片元着色器源码 -->
<script id="fragmentShader" type="x-shader/x-fragment">
  precision lowp float;// 所有float类型数据的精度是lowp
  void main() {
    // 计算方形区域每个片元距离方形几何中心的距离
    // gl.POINTS模式点渲染的方形区域,方形中心是0.5,0.5,左上角是坐标原点,右下角是1.0,1.0，
    float r = distance(gl_PointCoord, vec2(0.5, 0.5));
    //根据距离设置片元
    if(r < 0.5){
      // 方形区域片元距离几何中心半径小于0.5，像素颜色设置红色
      gl_FragColor = vec4(1.0,0.0,0.0,1.0);
    }else {
      // 方形区域距离几何中心半径不小于0.5的片元剪裁舍弃掉：
      discard;
    }
  }

</script>
123456789101112131415161718
```

通过`gl_PointCoord`返回的是片元纵横坐标`vec2(x,y)`,自然通过xy分量`gl_PointCoord.x`、`gl_PointCoord.y`方式可以分别访问片元坐标的横坐标、纵坐标