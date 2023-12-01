FFmpeg中的 sws_scale() 函数主要是用来做视频像素格式和分辨率的转换，其优势在于：可以在同一个函数里实现：1.图像色彩空间转换， 2:分辨率缩放，3:前后图像滤波处理。不足之处在于：效率相对较低，不如libyuv或shader，其关联的函数主要有：

1.sws_getContext():

[![复制代码](https://common.cnblogs.com/images/copycode.gif)](javascript:void(0);)

```
struct SwsContext *sws_getContext(
            int srcW, /* 输入图像的宽度 */
            int srcH, /* 输入图像的宽度 */
            enum AVPixelFormat srcFormat, /* 输入图像的像素格式 */
            int dstW, /* 输出图像的宽度 */
            int dstH, /* 输出图像的高度 */
            enum AVPixelFormat dstFormat, /* 输出图像的像素格式 */
            int flags,/* 选择缩放算法(只有当输入输出图像大小不同时有效),一般选择SWS_FAST_BILINEAR */
            SwsFilter *srcFilter, /* 输入图像的滤波器信息, 若不需要传NULL */
            SwsFilter *dstFilter, /* 输出图像的滤波器信息, 若不需要传NULL */
            const double *param /* 特定缩放算法需要的参数(?)，默认为NULL */
            );
```

[![复制代码](https://common.cnblogs.com/images/copycode.gif)](javascript:void(0);)

与其类似的函数还有： sws_getCachedContext ，区别在于： sws_getContext 可以用于多路码流转换，为每个不同的码流都指定一个不同的转换上下文，而 sws_getCachedContext 只能用于一路码流转换。

2.sws_freeContext

```
// 释放sws_scale
void sws_freeContext(struct SwsContext *swsContext);
```

真正用来做转换的函数则是： sws_scale() ，其函数定义如下：

```
int sws_scale(struct SwsContext *c, const uint8_t *const srcSlice[],
              const int srcStride[], int srcSliceY, int srcSliceH,
              uint8_t *const dst[], const int dstStride[]);
```

下面对其函数参数进行详细说明：

1.参数 SwsContext *c， **转换格式的上下文**。也就是 sws_getContext 函数返回的结果。
2.参数 const uint8_t *const srcSlice[], **输入图像的每个颜色通道的数据指针**。其实就是解码后的AVFrame中的data[]数组。因为不同像素的存储格式不同，所以srcSlice[]维数
也有可能不同。
以YUV420P为例，它是planar格式，它的内存中的排布如下：
YYYYYYYY UUUU VVVV
使用FFmpeg解码后存储在AVFrame的data[]数组中时：
data[0]——-Y分量, Y1, Y2, Y3, Y4, Y5, Y6, Y7, Y8……
data[1]——-U分量, U1, U2, U3, U4……
data[2]——-V分量, V1, V2, V3, V4……
linesize[]数组中保存的是对应通道的数据宽度 ，
linesize[0]——-Y分量的宽度
linesize[1]——-U分量的宽度
linesize[2]——-V分量的宽度

而RGB24，它是packed格式，它在data[]数组中则只有一维，它在存储方式如下：
data[0]: R1, G1, B1, R2, G2, B2, R3, G3, B3, R4, G4, B4……
这里要特别注意，linesize[0]的值并不一定等于图片的宽度，有时候为了对齐各解码器的CPU，实际尺寸会大于图片的宽度，这点在我们编程时（比如OpengGL硬件转换/渲染）要特别注意，否则解码出来的图像会异常。

3.参数const int srcStride[]，**输入图像的每个颜色通道的跨度**。.也就是每个通道的行字节数，对应的是解码后的AVFrame中的linesize[]数组。根据它可以确立下一行的起始位置，不过stride和width不一定相同，这是因为：
a.由于数据帧存储的对齐，有可能会向每行后面增加一些填充字节这样 stride = width + N；
b.packet色彩空间下，每个像素几个通道数据混合在一起，例如RGB24，每个像素3字节连续存放，因此下一行的位置需要跳过3*width字节。

4.参数int srcSliceY, int srcSliceH,定义在输入图像上处理区域，srcSliceY是**起始位置**，srcSliceH是**处理多少行**。如果srcSliceY=0，srcSliceH=height，表示一次性处理完整个图像。这种设置是为了多线程并行，例如可以创建两个线程，第一个线程处理 [0, h/2-1]行，第二个线程处理 [h/2, h-1]行。并行处理加快速度。
5.参数uint8_t *const dst[], const int dstStride[]定义输出图像信息（输出的每个颜色通道数据指针，每个颜色通道行字节数）

代码示例：将解码后的数据转换成1280*720的RGBA8888 格式

\1. 定义转换格式的上下文

[![复制代码](https://common.cnblogs.com/images/copycode.gif)](javascript:void(0);)

```
vctx = sws_getCachedContext(
                        vctx,
                        frame->width, // 源图像的宽度
                        frame->height, //  源图像的高度
                        (AVPixelFormat)frame->format,
                        outWidth,
                        outHeight,
                        AV_PIX_FMT_RGBA,
                        SWS_FAST_BILINEAR,
                        0, 0, 0
                        );
```

[![复制代码](https://common.cnblogs.com/images/copycode.gif)](javascript:void(0);)

\2. 开始转换

[![复制代码](https://common.cnblogs.com/images/copycode.gif)](javascript:void(0);)

```
int outWidth = 1280;
int outHeight = 720;
char *rgb = new char[1920*1080*4];
uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
                    data[0] = (uint8_t *)rgb;
                    int lines[AV_NUM_DATA_POINTERS] = {0};
                    lines[0] = outWidth * 4;
                    int h = sws_scale(vctx,
                              (const uint8_t **)frame->data,
                              frame->linesize,
                              0,
                              frame->height,
                              data,
                              lines
                            ); 
```

[![复制代码](https://common.cnblogs.com/images/copycode.gif)](javascript:void(0);)