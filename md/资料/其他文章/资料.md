```
    67         64         00         15           AC            D9           41             70           C6 

0110  0111  0110 0100  0000 0000   0001 0101   1010  1100    1101  1001   0100  0001     0111  0000    1100  0110








```

 


符合主profile的码流的profile_idc应为77。符合指定level的主profile的解码器应该可以解码所有的profile_idc为77的或constrained_set1_flag值为1且level_idc值小于或等于指定level的码流。

#### 1.1 扩展profile(Extended profile)

符合扩展profile的码流应该遵循以下的约束:


  对于扩展profile指定的level级应该被达到

符合指定level级的扩展profile的解码器可以解码所有的profile_idc值为88的或constrained_set2_flag值为1的，而且level_idc小于等于指定level级的码流。

符合指定level级的扩展profile的解码器可以解码所有的profile_idc值为66的或constrained_set0_flag值为1的，而且level_idc小于等于指定level级的码流。

> 1. **constrained_set0_flag****
>    **当constrained_set0_flag值为1的时候，就说明码流应该遵循基线profile(Baseline profile)的所有约束.constrained_set0_flag值为0时，说明码流不一定要遵循基线profile的所有约束。**
> 2. **constraint_set1_flag****
>    **当constrained_set1_flag值为1的时候，就说明码流应该遵循主profile(Main profile)的所有约束.constrained_set1_flag值为0时，说明码流不一定要遵循主profile的所有约束。**
> 3. **constraint_set2_flag**
>    **当constrained_set2_flag值为1的时候，就说明码流应该遵循扩展profile(Extended profile)的所有约束.constrained_set2_flag值为0时，说明码流不一定要遵循扩展profile的所有约束。**
>    **注意：当constraint_set0_flag,constraint_set1_flag或constraint_set2_flag中不只一个值为1的话，那么码流必须满足所有相应指明的profile约束。**
> 4. **constraint_set3_flag 
>    Reserved. Set to 0.****
> 5. **reserved_zero_4bits** 
>    **Reserved. Set to 0.**

#### 1.2 profile_idc： 编码等级  有以下取值

| **Options:** |                    |
| ------------ | ------------------ |
| *66*         | Baseline（直播）   |
| *77*         | Main（一般场景）   |
| *88*         | Extended           |
| *100*        | High (FRExt)       |
| *110*        | High 10 (FRExt)    |
| *122*        | High 4:2:2 (FRExt) |
| *144*        | High 4:4:4 (FRExt) |



#### 1.3 level_idc ： 最大支持码流范围

**标识当前码流的Level**。编码的Level定义了某种条件下的最大视频分辨率、最大视频帧率等参数，码流所遵从的level由level_idc指定。

当前码流中，level_idc = 0x1e = 30，因此码流的级别为3

| ***\**Options:\**\*** |                                                              |
| --------------------- | ------------------------------------------------------------ |
| *10*                  | 1    (supports only QCIF format and below with 380160 samples/sec) |
| *11*                  | 1.1  (CIF and below. 768000 samples/sec)                     |
| *12*                  | 1.2  (CIF and below. 1536000 samples/sec)                    |
| *13*                  | 1.3  (CIF and below. 3041280 samples/sec)                    |
| *20*                  | 2    (CIF and below. 3041280 samples/sec)                    |
| *21*                  | 2.1  (Supports HHR formats. Enables Interlace support. 5 068 800 samples/sec) |
| *22*                  | 2.2  (Supports SD/4CIF formats. Enables Interlace support. 5184000 samples/sec) |
| *30*                  | 3    (Supports SD/4CIF formats. Enables Interlace support. 10368000 samples/sec) |
| *31*                  | 3.1  (Supports 720p HD format. Enables Interlace support. 27648000 samples/sec) |
| *32*                  | 3.2  (Supports SXGA format. Enables Interlace support. 55296000 samples/sec) |
| *40*                  | 4    (Supports 2Kx1K format. Enables Interlace support. 62914560 samples/sec) |
| *41*                  | 4.1  (Supports 2Kx1K format. Enables Interlace support. 62914560 samples/sec) |
| *42*                  | 4.2  (Supports 2Kx1K format. Frame coding only. 125829120 samples/sec) |
| *50*                  | 5    (Supports 3672x1536 format. Frame coding only. 150994944 samples/sec) |
| *51*                  | 5.1  (Supports 4096x2304 format. Frame coding only. 251658240 samples/sec) |

####  1.5 seq_parameter_set_id

> 表示当前的序列参数集的id。通过该id值，图像参数集pps可以引用其代表的sps中的参数。

seq_parameter_set_id指定了由图像参数集指明的序列参数集。seq_parameter_set_id值应该是从0到31，包括0和31
注意： 当可用的情况下，编码器应该在sps值不同的情况下使用不同的seq_parameter_set_id值，而不是变化某一特定值的

 

#### 1.6  log2_max_frame_num_minus4

这个句法元素主要是为读取另一个句法元素 frame_num 服务的，frame_num 是最重要的句法元素之一，它标识所属图像的解码顺序。可以在句法表看到， fram-num的解码函数是 ue（v），函数中的 v 在这里指定：

> v = log2_max_frame_num_minus4 + 4
> 从另一个角度看，这个句法元素同时也指明了 frame_num 的所能达到的最大值：
> MaxFrameNum = 2( log2_max_frame_num_minus4 + 4 )
>
> 变量 MaxFrameNum 表示 frame_num 的最大值，在后文中可以看到，在解码过程中它也是一个非常重要的变量。
>
> 值得注意的是 frame_num 是循环计数的，即当它到达 MaxFrameNum 后又从 0 重新开始新一轮的计数。 解码器必须要有机制检测这种循环， 不然会引起类似千年虫的问题，在图像的顺序上造成混乱。

#### 1.6 chroma_format_idc  与亮度取样对应的色度取样 

 chroma_format_idc 的值应该在 0到 3的范围内（包括 0和 3）。**当 chroma_format_idc不存在时，应推断其值为 1（4：2：0的色度格式）。** 

色度采样结构

[![img](http://blog.chinaunix.net/attachment/201211/19/12947719_1353314331jb8g.jpg)](http://blog.chinaunix.net/attachment/201211/19/12947719_1353314331jb8g.jpg)

 

#### 1.7  bit_depth_luma_minus8  表示视频位深

如图: YUV420 8bit

![img](https://img-blog.csdn.net/20161117114254754?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQv/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/Center)



> h264 profile: 

​      0  High 只支持8bit

​      1   High10 才支持10bit



#### 1.8  pic_order_cnt_type

​		指明了 poc (picture order count) 的编码方法， poc 标识图像的播放顺序。由于H.264 使用了 B 帧预测，使得图像的解码顺序并不一定等于播放顺序，但它们之间存在一定的映射关系。 poc 可以由 frame-num 通过映射关系计算得来，也可以索性由编码器显式地传送。 H.264 中一共定义了三种 poc 的编码方法，这个句法元素就是用来通知解码器该用哪种方法来计算 poc。 而以下的几个句法元素是分别在各种方法中用到的数据。

在如下的视频序列中本句法元素不应该等于 2:

 一个非参考帧的接入单元后面紧跟着一个非参考图像(指参考帧或参考场)的接入单元
两个分别包含互补非参考场对的接入单元后面紧跟着一个非参考图像的接入单元.
一个非参考场的接入单元后面紧跟着另外一个非参考场,并且这两个场不能构成一个互补场对



#### 1.9  log2_max_pic_order_cnt_lsb_minus4

> 指明了变量 MaxPicOrderCntLsb 的值:
> MaxPicOrderCntLsb = 2( log2_max_pic_order_cnt_lsb_minus4 + 4 )
> 该变量在 pic_order_cnt_type = 0 时使用。

#### 2.0 max_num_ref_frames**

> 指定参考帧队列可能达到的最大长度，解码器依照这个句法元素的值开辟存储区，这个存储区用于存放已解码的参考帧， H.264 规定最多可用 16 个参考帧，本句法元素的值最大为 16。值得注意的是这个长度以帧为单位，如果在场模式下，应该相应地扩展一倍。

> ***\*(8). gaps_in_frame_num_value_allowed_flag\****
>
> 这个句法元素等于 1 时，表示允许句法元素 frame_num 可以不连续。当传输信道堵塞严重时，编码器来不及将编码后的图像全部发出，这时允许丢弃若干帧图像。 在正常情况下每一帧图像都有依次连续的 **frame_num 值**，解码器检查到如果 frame_num 不连续，便能确定有图像被编码器丢弃。这时，解码器必须启动错误掩藏的机制来近似地恢复这些图像，因为这些图像有可能被后续图像用作参考帧。
> 当这个句法元素等于 0 时，表不允许 frame_num 不连续，即编码器在任何情况下都不能丢弃图像。这时， H.264 允许解码器可以不去检查 frame_num 的连续性以减少计算量。这种情况下如果依然发生 frame_num 不连续，表示在传输中发生丢包，解码器会通过其他机制检测到丢包的发生，然后启动错误掩藏的恢复图像。

#### 2.1 pic_width_in_mbs_minus1**

> 本句法元素加 1 后指明图像宽度，以宏块为单位：
>                         PicWidthInMbs = pic_width_in_mbs_minus1 + 1
> 通过这个句法元素解码器可以计算得到亮度分量以像素为单位的图像宽度：
>                         PicWidthInSamplesL = PicWidthInMbs * 16
> 从而也可以得到色度分量以像素为单位的图像宽度：
>                         PicWidthInSamplesC = PicWidthInMbs * 8
> 以上变量 PicWidthInSamplesL、 PicWidthInSamplesC 分别表示图像的亮度、色度分量以像素为单位的宽。
>
> H.264 将图像的大小在序列参数集中定义，意味着可以在通信过程中随着序列参数集动态地改变图像的大小，在后文中可以看到，甚至可以将传送的图像剪裁后输出。
>
> ```cpp
> frame_width = 16 × (pic\_width\_in\_mbs_minus1 + 1);
> ```

#### 2.2 pic_height_in_map_units_minus1

> 本句法元素加 1 后指明图像高度：
>                         PicHeightInMapUnits = pic_height_in_map_units_minus1 + 1
>                         PicSizeInMapUnits = PicWidthInMbs * PicHeightInMapUnits
> 图像的高度的计算要比宽度的计算复杂，因为一个图像可以是帧也可以是场，从这个句法元素可以在帧模式和场模式下分别计算出出亮度、色度的高。值得注意的是，这里以 map_unit 为单位， map_unit的含义由后文叙述。
>
> ```cpp
> PicHeightInMapUnits = pic\_height\_in\_map\_units\_minus1 + 1;
> ```

#### 2.3 frame_mbs_only_flag**

> 标识位，说明宏块的编码方式。当该标识位为0时，宏块可能为帧编码或场编码；该标识位为1时，所有宏块都采用帧编码。根据该标识位取值不同，PicHeightInMapUnits的含义也不同，为0时表示一场数据按宏块计算的高度，为1时表示一帧数据按宏块计算的高度。
>
> 按照宏块计算的图像实际高度FrameHeightInMbs的计算方法为：
>
> ```cpp
> FrameHeightInMbs = ( 2 − frame_mbs_only_flag ) * PicHeightInMapUnits
> ```

#### 2.4 mb_adaptive_frame_field_flag

> 指明本序列是否属于帧场自适应模式。 mb_adaptive_frame_field_flag等于１时表明在本序列中的图像如果不是场模式就是帧场自适应模式，等于０时表示本序列中的图如果不是场模式就是帧模式。。表 列举了一个序列中可能出现的编码模式：
> a. 全部是帧，对应于 frame_mbs_only_flag =1 的情况。
> b. 帧和场共存。 frame_mbs_only_flag =0, mb_adaptive_frame_field_flag =0
> c. 帧场自适应和场共存。 frame_mbs_only_flag =0, mb_adaptive_frame_field_flag =1
> 值得注意的是，帧和帧场自适应不能共存在一个序列中。

#### 2.5 direct_8x8_inference_flag

> 标识位，用于B_Skip、B_Direct模式运动矢量的推导计算。

> (14). frame_cropping_flag
>
> 标识位，说明是否需要对输出的图像帧进行裁剪。

#### 2.6 vui_parameters_present_flag

> 指明 vui 子结构是否出现在码流中， vui 的码流结构在附录中指明，用以表征视频格式等额外信息。