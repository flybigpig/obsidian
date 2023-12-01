理是一个2D图片(甚至也有1D和3D的纹理), 它可以用来添加物体的细节；你可以想象纹理是一张绘有砖块图案的纸片，无缝折叠贴合到你的3D房子上，这样你的房子看起来就像有着砖块的外表。因为我们可以在一张图片上插入非常多的细节，这样就可以让物体非常精细而不用指定额外的顶点。

> 除了图像以外，纹理也可以被用来储存大量的数据，这些数据可以发送到着色器上，但是这不是我们现在的主题。例如后续的法线贴图，金属贴图等。

下图中的三角形贴上了一张砖墙的图片。

![img](https:////upload-images.jianshu.io/upload_images/18054784-b76e648fa072f26c.png?imageMogr2/auto-orient/strip|imageView2/2/w/600/format/webp)

image.png


 为了能够把纹理映射到三角形上，我们需要指定三角形的每个顶点各自对应纹理的哪个部分。这样每个顶点就会关联着一个`纹理坐标`, 用来指明着色器该从纹理图像的哪个部分采样。之后在图形的其他片段进行`片段插值`。



纹理坐标在x和y轴上，范围为0到1之间(2D纹理)。使用纹理坐标获取纹理颜色叫做采样。纹理坐标是典型的笛卡尔坐标，左下角为(0, 0)，(1, 1)使用右上角。



![img](https:////upload-images.jianshu.io/upload_images/18054784-9bf36c5b82e4c5b5.png?imageMogr2/auto-orient/strip|imageView2/2/w/326/format/webp)

image.png



我们为三角形指定了3个纹理坐标点。如上图所示，我们希望三角形的左下角对应纹理的左下角，因此我们把三角形左下角顶点的纹理坐标设置为(0, 0)；三角形的上顶点对应于图片的上中位置所以我们把它的纹理坐标设置为(0.5, 1.0)；同理右下方的顶点设置为(1, 0)。我们只要给顶点着色器传递这三个纹理坐标就行了，接下来它们会被传片段着色器中，它会为每个片段进行纹理坐标的插值。

纹理坐标看起来就像这样：



```c
float texCoords[] = {
    0.0f, 0.0f, // 左下角
    1.0f, 0.0f, // 右下角
    0.5f, 1.0f // 上中
};
```

对纹理采样有多种插值方法，所以我们需要自己告诉OpenGL该怎么对纹理进行采样。

## 纹理环绕方式

------

纹理坐标的范围通常是(0, 0)到(1, 1)，那如果我们把纹理坐标设置在范围之外会发生什么?OpenGL默认的行为是重复这个纹理图形，但OpenGL提供了更多的选择:

| 环绕方式           | 描述                                                         |
| :----------------- | :----------------------------------------------------------- |
| GL_REPEAT          | 对纹理的**默认行为**。重复纹理图像。                         |
| GL_MIRRORED_REPEAT | 和GL_REPEAT一样，但每次重复图片是镜像放置的。                |
| GL_CLAMP_TO_EDGE   | 纹理坐标会被约束在0到1之间，超出的部分会重复纹理坐标的边缘，产生一种边缘被拉伸的效果。 |
| GL_CLAMP_TO_BORDER | 超出的坐标为用户指定的边缘颜色。                             |

当纹理坐标超出默认范围时，每个选项都有不同的视觉效果输出。



![img](https:////upload-images.jianshu.io/upload_images/18054784-2fc239b388d37a91.png?imageMogr2/auto-orient/strip|imageView2/2/w/800/format/webp)

image.png



上面提及的每个选项都可以使用glTexParameter*函数对单独的一个坐标轴设置s、t（如果是使用3D纹理那么还有一个r）它们和x、y、z是等价的）：



```c
glTexParamateri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
glTexParamateri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
```

第一个参数: 代表纹理目标类型，我们使用的是2D纹理，因此纹理目标是GL_TEXTURE_2D
 第二个参数: 我们指定设置的选项与应用的纹理轴。s为水平方向，t为垂直方向。
 第三个参数: 我们传递一个环绕方式，在这个例子中OpenGL会给当前激活的纹理设定纹理环绕方式为GL_MIRRORED_REPEAT。
 如果我们选择GL_CLAMP_TO_BORDER选项，我们还需要指定一个边缘的颜色。这需要使用glTexParameter函数的fv后缀形式，用GL_TEXTURE_BORDER_COLOR作为它的选项，并且传递一个float数组作为边缘的颜色值：



```c
float borderColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
```

## 纹理过滤

------

纹理坐标是不依赖分辨率的，它可以是任意浮点数，所以OpenGL需要知道如何计算一个纹理坐标对应的纹理像素。你可能已经猜到了，OpenGL也有对于纹理过滤的选项。纹理过滤有很多个选项，但是现在我们只讨论最重要的两种: GL_NEAREST和GL_LINEAR。

> 常见的纹理过滤方式如下四种:
>  邻近过滤，线性过滤(双线性)， 三线性过滤以及各向异性过滤。不同的过滤方式的计算开销存在着差异，异性过滤效果最好但其开销最大。

GL_NEAREST(邻近过滤)是OpenGL**默认的**纹理过滤方式。设置为GL_NEAREST后，OpenGL会选择中心点最接近纹理坐标的那个像素。下图中你可以看到四个像素，加号代表纹理坐标。左上角那个纹理像素的中心距离纹理坐标最近，所以它会被选择为样本颜色：

![img](https:////upload-images.jianshu.io/upload_images/18054784-72c0cdd7283235d5.png?imageMogr2/auto-orient/strip|imageView2/2/w/200/format/webp)

image.png



GL_LINEAR(线性过滤)，它会基于纹理坐标附近的纹理像素, 计算出一个插值，近似出这些纹理像素之间得颜色。一个纹理像素的中心距离纹理坐标**越近**，那么这个纹理像素对最终的样本颜色的贡献**越大**。下图中你可以看到返回的颜色是邻近像素的混合色：

![img](https:////upload-images.jianshu.io/upload_images/18054784-39938af910d0d0c0.png?imageMogr2/auto-orient/strip|imageView2/2/w/200/format/webp)

image.png



那么这两种纹理过滤方式会有怎样的视觉效果呢?让我们看看在一个很大的物体上应用一张低分辨率的纹理会发生什么吧。



![img](https:////upload-images.jianshu.io/upload_images/18054784-4e95fc62fef15f4a.png?imageMogr2/auto-orient/strip|imageView2/2/w/517/format/webp)

image.png



GL_NEAREST产生了颗粒状的图案，我们能够清晰看到组成纹理的像素，而GL_LINEAR能够产生更加平滑的图案，很难看出单个的纹理像素。GL_LINEAR可以产生更真实的输出，但有些开发者更喜欢8-bit风格，所以他们会用GL_NEAREST选项。
 当进行放大和缩小操作时可以设置纹理过滤的选项，比如你可以在纹理被缩小时使用邻近过滤，而放大时使用线性过滤。我们需要使用glTexParameter*函数为放大和缩小指定过滤方式。这段代码看起来会和纹理环绕方式的设置很相似：



```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

### 多级渐远纹理

想象一下，假设我们有一个包含着上千个物体的大房间，每个物体上都有纹理。有些物体会距离摄像机很远，但其纹理会拥有与近处物体同样高的分辨率。此时若还是采用高分辨率的纹理进行采用，就会出现失真的现象。例如为一个4*4的像素片段应用512*512的纹理进行采样，线性过滤模式下，每个像素仅会使用到4个纹素，显然这样采样的结果不真实的。可以理解为你想用一个像素点来表达一张贴图里8*8或10 * 10个点的颜色信息。

OpenGL使用了一种叫做多级渐远纹理的概念来解决这个问题，它简单来说就是一系列的纹理图像。后一个纹理图像是前一个分辨率的1/2。其背后的理念很简单: 距离观察者的距离超过一定的阈值后，OpenGL会使用不同的多级渐远纹理，即最适合物体距离的那个。



![img](https:////upload-images.jianshu.io/upload_images/18054784-7ac93e930e1d93fa.png?imageMogr2/auto-orient/strip|imageView2/2/w/300/format/webp)

image.png



手工为每个纹理图像创建一系列的多级渐远纹理很麻烦，OpenGL提供了一个glGenerateMipmaps函数，在创建完一个纹理后调用它，OpenGL就会承担创建多级渐远纹理的工作。

> 在渲染中切换多级渐远纹理级别时，OpenGL在两个不同级别的多级渐远纹理层之间会产生不真实的生硬边界。就像普通的纹理过滤一样，切换多级渐远纹理级别时你也可以在两个不同的多级渐远纹理级别之间使用NEAREST和LINEAR过滤。为了指定不同多级渐远纹理级别之间的过滤方式，你可以使用下面四个选项中的一个代替原有的过滤方式：

| 过滤方式                  | 描述                                                         |
| :------------------------ | :----------------------------------------------------------- |
| GL_NEAREST_MIPMAP_NEAREST | 使用最邻近的多级渐远纹理来匹配像素大小，并使用邻近插值进行纹理采样 |
| GL_LINEAR_MIPMAP_NEAREST  | 使用最邻近的多级渐远纹理级别，并使用线性插值进行采样         |
| GL_NEAREST_MIPMAP_LINEAR  | 在两个最匹配像素大小的多级渐远纹理之间进行线性插值，使用邻近插值进行采样 |
| GL_LINEAR_MIPMAP_LINEAR   | 在两个邻近的多级渐远纹理之间使用线性插值，并使用线性插值进行采样 |

就像纹理过滤一样，我们可以使用glTexParameteri将过滤方式设置为前面四种中提到的方法之一:



```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

## 代码实践

------

### 加载与创建纹理

------

纹理图像可能被存储为各种各样的格式，每种都有自己的数据结构和排列，所以我们如何才能把这些图像加载到应用中呢?我们使用的是一个支持多种流行格式的图像加载库来解决这个问题, `stb_image.h`库。
 具体文件配置见LearnOpenGL。
 生成一个纹理的过程大致如下:



```c
unsigned int texture;
glGenTexture(1, &texture);
glBindTexture(GL_TEXTURE_2D, texture);
// 为当前绑定的纹理对象设置环绕，过滤方式
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// 加载并生成纹理
int width, height, nrChannels;
unsigned char *data = stbi_load("container.jpg", &width, &height, &nrChannels, 0);
if (data)
{
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
   // 为当前绑定的纹理自动生成所有所需的多级渐远纹理。 
    glGenerateMipmap(GL_TEXTURE_2D);
}
else{
   std::cout << "Failed to load texture" << std::endl;
}
// 生成了纹理和相应的多级渐远纹理后，释放图像的内存是一个很好的习惯。
stbi_image_free(data);
```

### 应用纹理

------

我们需要为顶点数据中添加顶点对应的纹理坐标，告知OpenGL如何进行采样。



```c
float vertices[] = {
//     ---- 位置 ----       ---- 颜色 ----     - 纹理坐标 -
     0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // 右上
     0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // 右下
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // 左下
    -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // 左上
};
```

顶点着色器代码



```glsl
#version 330 core
layout  (location = 0) in vec3 aPos;
layout  (location = 1) in vec3 aColor;
layout  (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;
void main()
{
      gl_Position = vec4(aPos, 1.0);
      ourColor = aColor;
      TexCoord = aTexCoord;
}
```

片段着色器接下来把输出变量`TexCoord`作为输入变量。
 如何将纹理对象传递给片段着色器?GLSL有一个供纹理对象使用的内建数据类型，叫做采样器,它以纹理类型作为后缀，如`sampler1D`, `sampler3D`等。在我们的例子中可以简单声明为一个`uniform samper2D`。



```glsl
#version 330 core
out vec4 FragColor;
in vec3 ourColor;
in vec2 TexCoord;
uniform sampler2D ourTexture;
void main()
{
    FragColor = texture(ourTexture, TexCoord);
}
```

最后只剩下在调用`glDrawElements`之前绑定纹理了，它会自动把纹理赋值给片段着色器的采样器:



```undefined
glBindTexture(GL_TEXTURE_2D, texture);
glBindVertexArray(VAO);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

你会看到下面的图像：



![img](https:////upload-images.jianshu.io/upload_images/18054784-e43661f9b3c5b166.png?imageMogr2/auto-orient/strip|imageView2/2/w/600/format/webp)

image.png

## 纹理单元

------

这里你肯定很奇怪**为什么`sampler2D`是个uniform变量，而我们却不用glUniform方法给它赋值**。使用glUniform1i，我们可以给纹理采样器分配一个位置值，这样的话我们能够在一个片段中设置多个纹理。一个纹理的位置值通常被称为纹理单元。一个纹理的默认纹理单元是0，它是默认的激活纹理单元，所以教程前面部分我们没有分配一个位置值。
 纹理单元的主要目的是让我们在着色器中可以使用多于一个的纹理。通常把纹理单元赋值给采样器，我们可以一次绑定多个纹理，只要我们**首先激活对应的纹理单元**。就像glBindTexture一样，我们可以使用glActiveTexture激活纹理单元，传入我们需要使用的纹理单元：



```c
glActiveTexture(GL_TEXTURE0); // 在绑定纹理之前先激活纹理单元
glBindTexture(GL_TEXTURE_2D, texutre);
```

激活纹理之后，接下来的glBindTexture函数调用会**绑定这个纹理当前激活的纹理单元**。纹理单元GL_TEXTURE0默认总是被激活，所以我们在前面的例子里当我们使用`glBindTexture`的时候，无需激活任何纹理单元。

> OpenGL至少保证你有16个纹理单元可用，也就是说你可以激活从GL_TEXUTRE0到GL_TEXTURE15。
>  更新片段着色器代码:



```glsl
#version 330 core
...

uniform sampler2D texture1;
uniform sampler2D texture2;

void main()
{
    // 0.2会返回第一个80%输入的颜色和20%第二个输入颜色。
    FragColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.2);
}
```

当使用多个纹理时(超出一个)， 我们必须改变部分渲染流程，在绑定两个纹理到对应的纹理单元后，再**定义哪个uniform采样器对应哪个纹理单元**:



```c
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, texture1);
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, texture2);

glBindVertexArray(VAO);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
//通过使用glUniform1i设置每个采样器的方式来告诉OpenGL每个着色器采样器属于哪个纹理单元。
// 只需要设置一次即可，所以可以放在渲染循环之前:

ourShader.use();
glUniform1i(glGetUniformLocation(ourShader.ID, "texture1), 0); // 手动设置
while(...) 
{
    [...]
}
```

通过使用glUniform1i设置采样器，我们保证了每个uniform采样器对应着正确的纹理单元。你应该能得到下面的结果：

![img](https:////upload-images.jianshu.io/upload_images/18054784-7ad7d9c747f8ed4d.png?imageMogr2/auto-orient/strip|imageView2/2/w/600/format/webp)

image.png


 你可能注意到了最终的笑脸图案是**上下颠倒**了的!这是因为**图片在存储像素数据时通常以左上角为原点的，而纹理坐标系是以左下角为原点**。



修复这个问题，可以有以下两个常见方法:
 1.将顶点数据中的纹理坐标的y值翻转。(1.0 -y值)
 2.可以编辑顶点着色器来自动翻转y坐标，将Texcoord的值变为TexCoord = vec2(texCoord.x, 1.0f - texCoord.y);。



作者：STL_f36e
链接：https://www.jianshu.com/p/e9fe45553a89
来源：简书
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。