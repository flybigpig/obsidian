### 一、名词

CTU: 编码树单元
CU: 编码单元
PU: 以CU为根，对CU进行划分,一个预测单元PU包含一个亮度预测块PB和两个色度预测块PB.
TU: 以CU为根，变换单元TU是在CU的基础上划分的，跟PU没有关系，采用四叉树划分方式，具体划分有率失真代价决定，下图给出了某个CU划分成TU的结构。

### 二、基础结构

HEVC Encoder整体框架：

 

![HEVC Encoder](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194530425-568087697.png)



HEVC Encoder

 

CU是用作帧间和帧内编码的基础模块，它的特点是方块，它的大小从8×8到最小64×64，LCU是64x64，可以使用递归分割的四分树的方法来得到，大的CU适用于图像中比较平滑部分，而小的部分则适用于边缘和纹理较丰富的区域。CU采用四叉树的分割方式，具体的分割过程通过两个变量来标记：分割深度(Depth)和分割标记符(Split_flag)。

在设置CTU大小为64X64的情况下，一个亮度CB最大为64X64即一个CTB直接作为一个CB，最小为8X8，则色度CB最大为32X32，最小为4X4。每个CU包含着与之相关联的预测单元(PU)和我变换单元(TU).

 

![CU](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194532568-716316031.png)



CU

 

Z扫描顺序：

 

![Z扫描](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194534475-1458773912.png)



Z扫描

 

PU是预测的最基本的单元，是从CU中分割出来的，HEVC中对于skip模式、帧内模式和帧间模式。
帧内预测有2种划分模式，只有在CU尺寸为8x8时，才能用PART_NxN。
帧间有8种划分模式，PU可以使方形也可以使矩形，**但是其分割不是递归的**，与CU的分割还是有区别的。尺寸最大为64×64到最小4×4。

 

![PU分割模式](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194536318-1613783346.png)



PU分割模式

 

TU也是采用四叉树划分，以CU为根，TU可以大于PU，但是却不可以大于CU的大小。
在帧内编码过程中，TU 的尺寸严格小于 PU 的尺寸；
在帧间编码过程中，TU 的尺寸不一定小于PU 的尺寸，但一定小于其对应 CU 的尺寸。

 

![CB, TB](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194537941-2034207531.png)



CB, TB

 

Slice可以包含一个独立的Slice Segment（SS）和多个非独立的SS，一个Slice中的SS可以互相依赖，但不能依赖其它Slice。图中，虚线是SS分隔线，实线是Slice分隔线。

 

![Slice](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194539557-918660725.png)



Slice

 

Tile是一个矩形块，Slice是一个条带。
Tile、Slice需要满足以下两个条件之一：

1. 任一Slice中的所有CTU属于同一个Tile：

 

![条件一](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194540815-874596673.png)



条件一

 

1. 任一Tile中的所有CTU属于同一个Slice：

 

![条件二](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194542352-2075632002.png)



条件二

 

### 三、帧内预测

帧内预测模式共35个（h264有9个），包括Planar，DC，33个方向模式：

| 模式编号 | 模式名称         |
| :------- | :--------------- |
| 0        | Planar模式       |
| 1        | DC模式           |
| 2~34     | 33种角度预测模式 |

 

![帧内预测模式](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194543916-118147081.png)



帧内预测模式

 

除了Intra_Angular预测外，HEVC还和H.264/MPEG-4 AVC一样，支持Intra_Planar, Intra_DC预测模式；
. Intra_DC 使用参考像素的均值进行预测；
. Intra_Planar 使用四个角的参考像素得到的两个线性预测的均值;

划分模式：帧内只能使用PART_2Nx2N、PART_NxN两种。

### 四、帧间预测

Skipped模式：无MV差异和残差信息的帧间预测模式

针对运动向量预测，H.265有两个参考表：L0和L1。每一个都拥有16个参照项，但是唯一图片的最大数量是8。H.265运动估计要比H.264更加复杂。它使用列表索引，有两个主要的预测模式：合并和高级运动向量（Merge and Advanced MV.）。

#### 1. 运动估计准则

最小均方误差（Mean Square Error,MSE）
最小平均绝对误差（Mean Absolute Difference,MAD）
最大匹配像素数（Matching-Pixel Count,MPC）
绝对误差和(Sum Of Absolute Difference,SAD)
最小变换域绝对误差和(Sum Of Absolute Transformed Difference,SATD)

一般用SAD或者SATD。SAD不含乘除法，且便于硬件实现，因而使用最广泛。实际中，在SAD基础上还进行了别的运算来保证失真率。

#### 2. 搜索算法

- dia 菱形
- hex (default) 六边形
- umh 可变半径六边形搜索(非对称十字六边形网络搜索)
- star 星型
- full 全搜索

全搜索： 所有可能的位置都计算两个块的匹配误差，相当于原块在搜索窗口内一个像素一个像素点的移动匹配
菱形搜索： 在x265中实际是十字搜索，仅对菱形对角线十字上的块进行搜索
HM的则是全搜索和TZSearch以及对TZSearch的优化的搜索。

#### 3. MV预测

HEVC在预测方面提出了两种新的技术–Merge && AMVP (Advanced Motion Vector Prediction)都使用了空域和时域MV预测的思想，通过建立候选MV列表，选取性能最优的一个作为当前PU的预测MV，二者的区别：

- Merge可以看成一种编码模式，在该模式下，当前PU的MV直接由空域或时域上临近的PU预测得到，**不存在MVD**；而AMVP可以看成一种MV预测技术，编码器只需要对实际MV与预测MV的差值进行编码，**因此是存在MVD的**。
- 二者候选MV列表长度不同，构建候选MV列表的方式也有所区别

**Merge**
当前块的运动信息可以通过相邻块的PUs运动信息推导出来，只需要传输合并索引，合并标记，不需要传输运动信息。

空间合并候选：从5个不同位置候选中选择4个合并候选

 

![空间合并候选](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194544550-243445947.png)



空间合并候选

 

图中便是5个PU，但是标准规定最多四个，则列表按照A1–>B1–>B0–>A0–>(B2)的顺序建立，B2为替补，即当其他有一个或者多个不存在时，需要使用B2的运动信息。

时间合并候选：从2个候选中选择1个合并候选
从C3、H中选择一个：

 

![时间合并候选](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194545018-153565912.png)



时间合并候选

 

**AMVP**
构造一个时空PUs的运动矢量候选列表，当前PU遍历候选列表，通过SAD选择最优预测运动矢量。

空间运动矢量候选：从5个位置中左侧、上侧分别选1个共2个候选

而AMVP的选择顺序,左侧为A0–>A1–>scaled A0–>scaledA1，其中scaled A0表示将A0的MV进行比例伸缩。
上方为B0–>B1–B2–>(scaled B0–>scaled B1–>scaled B2)。

然而，x265并不在乎标准，我们要的就是速度，所以在x265的代码中，只能看到它使用AMVP且对应的变量是

| 图中的代号 | x265中代码变量中包含 |
| :--------- | :------------------- |
| B2         | ABOVE_LEFT           |
| B1         | ABOVE                |
| B0         | ABOVE_RIGHT          |
| A1         | LEFT                 |
| A0         | BELLOW_LEFT          |

且对左侧和上侧分别if-else，选出两个。

时间运动矢量候选：从2个不同位置候选中选择1个候选

C0（右下） represents the bottom right neighbor and C1（中心） represents the center block.

 

![时间运动矢量候选](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194545345-455135850.png)



时间运动矢量候选

 

**Skip vs Merge:**

 

![Skip vs Merge](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194549068-996296477.png)



Skip vs Merge

 

**分数像素内插：**
用于产生非整数采样位置像素值的预测样本。

### 五、量化变换

### 六、其它

**熵编码**
目前HEVC规定只使用CABAC算术编码。

**去块效应滤波器**
消除反量化和反变换后由于预测误差产生的块效应，即块边缘处的像素值跳变。

 

![块适应产生](https://images2018.cnblogs.com/blog/463269/201806/463269-20180605194554746-440374495.png)



块适应产生

 

**自适应样点补偿**
通过对重建图像进行分类，对每一类图像像素值进行加减1，从而达到减少失真，提高压缩率，减少码流的作用。

目前自适应样点补偿分为带状补偿，边缘补偿：

1. 带状补偿，按像素值强度划分为不同的等级，一共32个等级，按像素值排序，位于中间的16个等级进行补偿，将补偿信息写进码流，其余16个等级不进行补偿，减少码流。
2. 边缘补偿，选择不同的模板，确定当前像素类型，如局部最大，局部最小，或者图像边缘。

**Wavefront Parallel Processing (WPP)**
WPP的并行技术是以一行LCU块为单位进行的，但是不完全截断LCU行之间的关系，如下图，Thread1的第二个块的CABAC状态保存下来，用于Thread2的起始CABAC状态，依次类推进行并行编码或解码，因此行与行之间存在很大的依赖关系。通常该方法的压缩性高于tiles。