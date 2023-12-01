**AAC基本概况：**

l **AAC（Advance Audio Coding）：**

即高级音频编码，出现在1997年，基于MPEG-2的音频编码技术，当时被称为MPEG-2 AAC,因此把其作为MPEG-2(MP2)标准的延伸。是由Fraunhofer IIS、杜比实验室、AT&T、Sony等公司共同开发，目的是取代MP3格式，随着MPEG-4(MP4)标准在2000年的成型，则为AAC也叫M4A。

 

l **和AC3编码关系:**

和AC3关系不大，AC3早于AAC，是由AAC的发起单位杜比实验室和日本先锋合作研制的新编码方式。AAC能输出AC-3的任何码率，胜过AC-3，压缩率更高，但技术上更加复杂。

 

l **AAC背景和发展：**

1997年制定了不兼容MPEG-1的音频标准MPEG-2 NBC即MPEG-2 AAC;

1999年AAC又增加了LTP和PNS工具，形成了MPEG-4 AAC V1;

2002年在MPEG-4 AAC v1增加了SBR和错误鲁棒性工具，形成了 HE-AAC;

2004年MPEG-4在HE-AAC引入了PS模块，提升降码率性能，形成了EAAC+;

对于1999年、2002年、2004年增加了SBR和PS等编码技术的统称为MPEG-4 AAC；

 

**备注：**上面这些SBR PS等缩写就是音频的编码算法代名词，网上比较多，感兴趣的可以进一步自行搜索。1. SBR技术即Spectral Band Replication(频段复制)音乐的主要频谱集中在低频段，高频段幅度很小，但很重要，决定了音质。如果对整个频段编码，若是为了保护高频就会造成低频段编码过细以致文件巨大；若是保存了低频的主要成分而失去高频成分就会丧失音质。SBR把频谱切割开来，低频单独编码保存主要成分，高频单独放大编码保存音质，“统筹兼顾”了，在减少文件大小的情况下还保存了音质，完美的化解这一矛盾。

\2. PS指“parametric stereo”（参数立体声）。原来的立体声文件文件大小是一个声道的两倍。但是两个声道的声音存在某种相似性，根据香农信息熵编码定理，相关性应该被去掉才能减小文件大小。所以PS技术存储了一个声道的全部信息，然后，花很少的字节用参数描述另一个声道和它不同的地方。

 

l **AAC编码技术参数：**

采样率范围：8KHz-96KHz 范围比较广，就是一秒在模拟信号上进行多少次采样；

码率：8kbps-576kbps，支持范围比较宽，在压缩比和质量上都能考虑到；

声道：最多支持48个主声道，16个低频声道，声音细节更丰富，音乐场景也用的多；

采样精度：就是一个采样点需要在计算机表示占用的字节数，一般用2字节16bit表示；

 

l **AAC编码的主要规格：**

根据不同的编码技术，AAC的编码分为九种规格，这和H264的编码规格大同小异。

\1. MPEG-2 AAC LC低复杂度规格（Low Complexity）编码方式比较简单，没有增益控制，但是提高了编码效率，在中等码率的编码效率和音质方面，都能找到平衡点。

\2. MPEG-2 AAC Main 主规格

\3. MPEG-2 AAC SSR 可变采样率规格（Scaleable Sample Rate）

\4. MPEG-4 AAC LC 低复杂度规格（Low Complexity）

\5. MPEG-4 AAC Main 主规格--包含了除增益控制之外的全部功能，音质最好

\6. MPEG-4 AAC SSR 可变采样率规格（Scaleable Sample Rate）

\7. MPEG-4 AAC LTP 长时期预测规格（Long Term Prediction）

\8. MPEG-4 AAC LD 低延迟预测规格（Low Delay）

\9. MPEG-4 AAC HE 高效率规格（High Efficency）--这种规格适合用于低码率编码，有Nero-ACC编码器支持，是一种成熟的商用编码器。

 

目前使用最多的就是LC和HE（适合降低码率），**流行的Nero AAC编码程序支持LC、HE、HEv2三种规格的，而且编码后的AAC音频，规格都显示LC。**其中HE就是在AAC(LC)编码技术上增加SBR技术，HEv2就是AAC（LC）上技术上不仅仅增加了SBR技术，同时也增加了PS技术。

所以一般的商业音频编码器只支持部分编码规格，这也是我们选择编码器的重要考虑因素之一，因为不同的编码规格支持的音频采样率，码率都不一样，背后采用的编码技术和算法复杂度也不一样。

l **AAC编码方式特点：**

\1. AAC高压缩比的音频编码方式，比G7xx、MP3、AC3系列的压缩比都高，并且质量和CD差不多，但是和比较新的Opus还是差点，不过Opus目前还未充分普及；

\2. AAC也采用了变换编码算法，采用了更高的滤波器组，这是压缩高的原因；

\3. AAC为了提高压缩比，还采用了噪声重整，反向自适应预测，联合立体声和量化霍夫曼编码算法等新技术；

\4. AAC支持了更多的采样率和比特率，支持了1-48个音轨和多达15个低频音轨，具有多种语言兼容能力；

\5. AAC支持了更宽的声音频率范围，从8KHz-96KHz,远宽于MP3的16KHz-48KHz范围；

\6. AAC特殊的算法可以保有声音频率甚高和甚低频率。声音细节更丰富更清晰更接近原声；

\7. AAC采用了优化算法，导致解码端简单，降低了解码端的处理复杂度；

 

**AAC的封装格式：**

 

n **AAC封装类型：**

 

\1. ADIF:Audio Data Interchange Format音频数据交换格式，这种格式一般应用在将音频通过写文件方式存储在磁盘里，不能进行随机访问，不允许在文件中间开始进行解码。只有拿到整个文件时才能开始进行渲染播放，这种暂时还没用到，不是这篇文章的重点。

\2. ADTS:Audio Data Transport Stream 音频数据传输流。这种格式的特征是用同步字节进行将AAC音频截断，然后可以允许客户端在任何地方进行解码播放，适合网络传输场景。这也是本文介绍的封装格式重点。

ADTS的格式如下：

n **AAC****封装头字段****：**

 

ADIF的格式：

**adif_sequence**

**adif_header + byte_alignment + raw_data_stream**

**adif_header + byte_alignment + raw_data_block......+ raw_data_block**

ADIF Header头信息如下：

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9MTUREaWFWaWF3M0tKV1BsOFEwbGsxSXFNYnF3SUFCcXVhSWNyRld6TlJ6UWRxb0pyYVZzWGliM0dpYlRUVzZ1QUllUjhjT1NFTWZidjdwVEZnTlNhT0lpY093LzY0MA?x-oss-process=image/format,png)

 

ADTS的格式：

**adts_sequence**

**adts_frame + adts_frame + ...... + adts_frame**

**adts_fixed_header + adts_variable_header + error_check + raw_data_block + error_check**

ADTS header 的固定头和可变头信息：

固定头意思就是一旦音频文件形成，所有帧的信息头字段意义都是一样的，但是可变头说的是每个帧这里面字段都有不一样的地方，不要理解为可有可无的意思。

 

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9MTUREaWFWaWF3M0tKV1BsOFEwbGsxSXFNYnF3SUFCcXVhWExCU3ZhTThyd3UwcDVLS0ZHS0RJWkVDSWdWWXRDWDFLWU11YXZ5VkhsQWlicXl1THF1ZEZiUS82NDA?x-oss-process=image/format,png)

 

ADTS帧头各个字段和含义：

| 序号 | 域                               | 长度bits | 说明                                                         | 解释                                                         |
| ---- | -------------------------------- | -------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| 1    | Syncword                         | 12       | all bits must be 1                                           | 总是0xFFF,代表一个ADTS帧的开始，作为分界符，用于同步每帧起始位置。 |
| 2    | ID即MPEG version                 | 1        | 0 for MPEG-4, 1 for MPEG-2                                   | 一般用0，因为都是属于MPEG的规范。                            |
| 3    | Layer                            | 2        | always 0                                                     | 总是00                                                       |
| 4    | Protection Absent                | 1        | set to 1 if there is no CRC and 0 if there is CRC            | 这里代表是否有CRC检验字段，1代表没有，0代表有。              |
| 5    | Profile                          | 2        | the MPEG-4 Audio Object Type minus 1                         | 代表使用哪个级别和规范的AAC，其中01代表Low Complexity(LC),其中profile等于Audio Object Type的值减1，其中所有Audio Object Type值在下面所示。 |
| 6    | Sampling Frequency Index         | 4        | MPEG-4 Sampling Frequency Index (15 is forbidden)            | 采样率下标，由于AAC的采样率范围是8KHz-96KHz，所以具体用那个，这个字段决定。 |
| 7    | Private Bit                      | 1        | set to 0 when encoding, ignore when decoding                 | 一般默认0即可                                                |
| 8    | Channel Configuration            | 3        | MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE) | 通道配置即声道数，一般2表示立体声双声道。具体取值范围参考下表。 |
| 9    | Originality copy                 | 1        | set to 0 when encoding, ignore when decoding                 | 一般默认0即可                                                |
| 10   | Home                             | 1        | set to 0 when encoding, ignore when decoding                 | 一般默认0即可                                                |
| 11   | Copyrighted identification bit   | 1        | set to 0 when encoding, ignore when decoding                 | 一般默认0即可                                                |
| 12   | Copyrighted identification Start | 1        | set to 0 when encoding, ignore when decoding                 | 一般默认0即可                                                |
| 13   | Aac Frame Length                 | 13       | this value must include 7 or 9 bytes of header length: FrameLength = (ProtectionAbsent == 1 ? 7 : 9) + size(AACFrame) | 一个ADTS帧的长度包括ADTS头和AAC原始流。用AAC原始流长度+7或者9。当proection_ansent = 0 则+9proection_ansent = 1 则+7 |
| 14   | ADTS Buffer Fullness             | 11       | buffer fullness                                              | 0x7FF 说明是码率可变的码流。0x000代表是固定码率的码流。      |
| 15   | Number of AAC Frames             | 2        | number of AAC frames (RDBs) in ADTS frame minus 1, for maximum compatibility always use 1 AAC frame per ADTS frame | ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧。所以说number_of_raw_data_blocks_in_frame == 0 表示说ADTS帧中有一个AAC数据块。(一个AAC原始帧包含一段时间内1024个采样及相关数据) |
| 16   | CRC                              | 16       | CRC if protection absent is 0                                | 校验字段，为可选字段。                                       |

 

ADTS各个字段的取值范围：

\1. Profile 取值：

| Object Type ID | Aduio Object Type | 备注   |
| -------------- | ----------------- | ------ |
| 1              | AAC Main          |        |
| 2              | AAC LC            | 最常用 |
| 3              | AAC LTR           |        |
| 4              | SBR               |        |
| 5              | AAC scalable      |        |

 

\2. Sampling Frequency Index采样率取值

| Sampling frequency index | value        | 备注                                                         |
| ------------------------ | ------------ | ------------------------------------------------------------ |
| 0x0 即0000               | 96000        | DVD-Audio、一些 LPCM DVD 音轨、Blu-ray Disc（蓝光盘）音轨、和 HD-DVD （高清晰度 DVD）音轨所用所用采样率 |
| 0x1 即0001               | 88000        |                                                              |
| 0x2 即0010               | 64000        |                                                              |
| 0x03即0011               | 48000        | miniDV、数字电视、DVD、DAT、电影和专业音频所用的数字声音所用采样率 |
| 0x04即0100               | 44100        | 音频 CD, 也常用于 MPEG-1 音频（VCD, SVCD, MP3）所用采样率    |
| 0x05即0101               | 32000        |                                                              |
| 0x06即0110               | 24000        |                                                              |
| 0x07即0111               | 22000        |                                                              |
| 0x08即1000               | 16000        |                                                              |
| 0x09即1001               | 12000        |                                                              |
| 0x0a即1010               | 11025        |                                                              |
| 0x0b即1011               | 8000         | 电话所用采样率, 对于人的说话已经足够                         |
| 0x0c即1100               | 7350         |                                                              |
| 0x0d即1101               | reserver     |                                                              |
| 0x0e即1110               | reserve      |                                                              |
| 0x0f即1111               | escape value |                                                              |

 

\3. Channel Configuration通道数取值

| value | number of channels | Audio syntactic list in order received                       | tips                                             |
| ----- | ------------------ | ------------------------------------------------------------ | ------------------------------------------------ |
| 0     | -                  | -                                                            | 关于这些字段看下面raw_data_block基本码流组件说明 |
| 1     | 1                  | single_channel_element                                       | 1                                                |
| 2     | 2                  | channel_pair_element()                                       | 2                                                |
| 3     | 3                  | single_channel_element()channel_pair_element()               | 1+2                                              |
| 4     | 4                  | single_channel_element()channel_pair_element()single_channel_element() | 1+2+1                                            |
| 5     | 5                  | single_channel_element()channel_pair_element() channel_pair_element() | 1+2+2                                            |
| 6     | 5+1                | single_channel_element()channel_pair_element()channel_pair_element()lfe_channel_element() | 1+2+2+1                                          |
| 7     | 7+1                | single_channel_element()channel_pair_element()channel_pair_element()channel_pair_element() lfe_channel_element() | 1+2+2+2+1                                        |
| 8-15  | -                  | -                                                            | 保留                                             |

 

ADTS的raw_data_block基本码流组件，头部有3位标志位id_syn_ele,指示六种不同类型的元素：

| id_syn_ele  | 数据流                     | 含义                                                         | 注释                             |
| ----------- | -------------------------- | ------------------------------------------------------------ | -------------------------------- |
| ID_SCE(0x0) | single_channel_element()   | 单通道元素基本上只由一个ICS组成。一个原始数据块最可能由16个SCE组成。 | 核心算法区                       |
| ID_CPE(0x1) | channel_pair_element()     | 由两个可能共享边信息的ICS和一些联合立体声编码信息组成。一个原始数据块最多可能由16个SCE组成。 | 核心算法区                       |
| ID_CCE(0x2) | coupling_channel_element() | 藕合通道元素。代表一个块的多通道联合立体声信息或者多语种程序的对话信息。 | 核心算法区                       |
| ID_LFE(0x3) | lfe_channel_element()      | 低频元素。包含了一个加强低采样频率的通道。                   | 核心算法区                       |
| ID_DSE(0x4) | data_stream_element()      | 数据流元素，包含了一些并不属于音频的附加信息。               | 扩展流或者用户数据，非核心算法区 |
| ID_PCE(0x5) | program_config_element()   | 程序配置元素。包含了声道的配置信息。它可能出现在ADIF 头部信息中。 | 扩展流或者用户数据，非核心算法区 |
| ID_FIL(0x6) | fill_element()             | 填充元素。包含了一些扩展信息。如SBR，动态范围控制信息等。    | 扩展流或者用户数据，非核心算法区 |

 

**实例分析：**

**用MediaInfo工具可以查看AAC音频的基本信息**

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9MTUREaWFWaWF3M0tKV1BsOFEwbGsxSXFNYnF3SUFCcXVhUXJhMHhwTWlieWFFZmV2UDJJTzE5N2dBYXppYU5Pa1lZYlBsZkhUNXc1WmVCZUVZMmJHeDRvWWcvNjQw?x-oss-process=image/format,png)

 

**AAC Audio ES Viewer工具可以详细分析每一个字节**

 

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9MTUREaWFWaWF3M0tKV1BsOFEwbGsxSXFNYnF3SUFCcXVhd2liSVpqbEl2dWI4YUFpYW1KSVpFZHhLaWF6VUdTcTFUMUljVlh1VFU3aWNNM05TaEJ2dVYyY2liZXcvNjQw?x-oss-process=image/format,png)

**分析各个字段含义**

待分析数据：

固定头十六进制可变头十六进制

FF  F1 4C   8 0  42  E0  00

固定头二进制可变头二进制

1111 1111 1111 0001 0100 1100  1000 0000 0100 0010 1110 0000 0000 0000

**固定头字段含义**

syncword :

十六进制：0x0FFF (12 bits) 分界符

二进制：1111 1111 1111

 

ID：

十进制0 (1 bit) 0 代表MPEG4的AAC

二进制0 (1 bit) 0

 

layer :

十进制0 (2 bits) 固定填充00，默认

二进制0 0

 

 

protection_absent:

十进制1 (1 bit)，决定了头的长度，目前7字节

二进制：1

 

profile :

十进制：1 [Low Complexity profile (LC)] (2 bits)

二进制：01

 

sampling_frequency_index:

十进制：3 [48000 Hz] (4 bits)

二进制：0011

 

private_bit

十进制: 0 (1 bit)

二级制：0

 

channel_configuration    

十进制: : 2 [2 - LF RF] (3 bits)

二级制：10

 

original/copy

十进制: 0 (1 bit)，默认

二进制：0

 

home:

十进制:0 (1 bit)，默认

二进制：0

 

**可变头信息**

copyright_identification_bit:

十进制:0 (1 bit)，默认

二进制：0

 

copyright_identification_start :

十进制:0 (1 bit)，默认

二进制：0

 

frame_length:

十进制：535 (13 bits) 长度，包括头和实际裸流数据535-7=528

二级制：00 0100 0010 111

 

adts_buffer_fullness:

十进制：0 (11 bits)

二进制：0 0000 0000 00 代表是固定码率0x000,可变码率是0x7FF

 

number_of_raw_data_blocks_in_frame:

十进制：0 (2 bits),代表后面的实际帧数0+1个AAC帧

二级制：00

 

**AAC帧的裸流**

raw_data_block()

 

**核心代码参考：**

我们在开发中经常遇到这块就是AAC封装格式的解析，需要拿到裸流进行播放和提取里面的相应字段，或者将裸流打包为ADTS然后封装到TS、MP4、FLV中进行打包发送传输。下面的代码通过读取一个文件流，获取里面的ADTS信息和音频帧。

\1. 先定义ADTS头的结构体

 

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9MTUREaWFWaWF3M0tKV1BsOFEwbGsxSXFNYnF3SUFCcXVhY3UxQmVxV251aWFpYzNyalBpY1dXdWYxZjJCN2pkMXlaTmN6NUgydlJ0STYyeXBpYTVSQ3V5T0FTdy82NDA?x-oss-process=image/format,png)

\2. 读取文件流的第一个ADTS音频帧的头部数据，并解析里面的长度；

 

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9MTUREaWFWaWF3M0tKV1BsOFEwbGsxSXFNYnF3SUFCcXVhSGpUNFNXa0NnZGtkYjJ1b2ljSDhPNkM1Wmdla2FSOWliSG4xU3NocG1pYXhwRU5ZRGEzWWZIb0hRLzY0MA?x-oss-process=image/format,png)

\3. 再根据长度读取里面的音频裸数据；

 

![img](https://imgconvert.csdnimg.cn/aHR0cHM6Ly9tbWJpei5xcGljLmNuL21tYml6X3BuZy9MTUREaWFWaWF3M0tKV1BsOFEwbGsxSXFNYnF3SUFCcXVhUTNMTHFMNWtUc3Z4Ym84N3g2Z2NCOGw5WGNSbUk5a3JBdmFDaWFHcDI4bXJBQVdQWGJCQkFIUS82NDA?x-oss-process=image/format,png)

\4. 不断循环即可完成头部数据的解析和其裸数据的读取；

 