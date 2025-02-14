## 1 [USB协议](https://so.csdn.net/so/search?q=USB%E5%8D%8F%E8%AE%AE&spm=1001.2101.3001.7020)

### 1.1 USB协议分层

  上一篇已经简单介绍了USB协议的相关知识，其中的描述符较为重要，描述符成功返回，USB通信已经成功了一大半，具体描述符的知识点可以翻阅上一篇来了解。下面我们来看一下USB协议在的分层。  
  USB协议用的地方非常多，比如U盘、[麦克风](https://so.csdn.net/so/search?q=%E9%BA%A6%E5%85%8B%E9%A3%8E&spm=1001.2101.3001.7020)、充电器等等。其中传输、事务层在USB协议中都通用，而**包层则是软件层根据不同的用途做的区别实现，这一层才是我们软件开发要用到的**。如下图所示：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/dbbbb61f76bf9c2cee9c525d7984c5e9.png#pic_center)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/2daa4a63c6b5e40830811bd66f42de6a.png#pic_center)  
接下来我们以U盘为例分析一下。

## 2 U盘协议

  在USB协议中，规定了一类大容量存储设备(mass storage device)，U盘就属于大容量存储设备。大容量存储设备的接口类代码(bInterfaceClass字段)为0x08，接口子类代码(bInterfaceSubClass字段)有好几种，但大部分U盘都使用代码0x06，即[SCSI](https://so.csdn.net/so/search?q=SCSI&spm=1001.2101.3001.7020)透明命令集。协议代码(bInterfaceProtocol字段)有3种：0x00,0x01,0x50，前两种使用中断传输，最后一种仅使用批量传输。  
  U盘通讯协议也有一套规则，详情参考官方文档：https://usb.org/sites/default/files/usbmassbulk\_10.pdf0

### 2.1 类特殊请求

  在USB大容量存储设备的`Bulk Only Transport协议`中，规定了两个类特殊请求：`Bulk-Only Mass Storage Reset`和`Get Max LUN`。**前者是复位到命令状态的请求，后者是获取最大逻辑单元请求。**

### 2.2 Get Max LUN请求

  `Get Max LUN请求的格式`，由`bmRequestType`可知，它是发送到接口的类输入请求，`bRequest`值为0xFE，`wIndex`的值为请求的接口号，本实例中为接口0，传输的数据长度`wLength` 为1字节，设备将在数据过程返回1字节的数据。该字节表示设备由多少个逻辑单元，值为0时表示有一个逻辑单元，为1时表示有两个逻辑单元，以此类推，最大可以取15，Data为数据块。

| BmRequestType | bRequest | wValue | wIndex | wLength | Data |
| --- | --- | --- | --- | --- | --- |
| 10100001b | 11111110b | 0000h | Interface | 0001h | 1byte |

### 2.3 Bulk-Only Mass Storeage Reset请求

  Bulk-Only Mass Storage Reset请求是通知设备接下来的批量传输端点输出数据为命令块封包CBW(Command Block Wrapper)，其结构如表。在这个请求处理中，仅需设置一下状态，说明接下来的数据为CBW，然后返回一个0长度的状态数据包即可。

### 2.4 仅批量传输协议的数据流模型

  前面的工作完成之后，接下来就是`通过批量端点传输数据`。在仅批量传输协议中，规定了数据传输的结构和过程，共分成三个阶段：`命令阶段`、`数据阶段`和`状态阶段`。这有点类似控制传输，但又不完全相同。**`命令阶段`由主机通过批量端点发送一个CBW(命令块封包)的结构，在CBW中定义了要操作的命令以及传输数据的方向和数量。`数据阶段`的传输方向由命令阶段决定，而`状态阶段`则总是由设备返回该命令完成的状态。**  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/897137e0a53891ff76d9b9494ce15e57.png#pic_center)

### 2.5 命令封包CBW的结构

  每个CBW通过`Bulk-Out端点`进行传输，每个CBW的长度是`31字节`。CBW的传输是`小端格式`。**（注：大端小端格式例如：0x1234在内存中的写入顺序：大端模式：正序。先写0x12，再写0x34。小端模式：倒序，先写0x34，再写0x12）**

> 1.  dCBWSignature 该字段为CBW的标志，为字符串USBC（即USB命令的缩写）。用ASCII码来  
>     表示就是0x55,0x53,0x42,0x43。如果用小端模式的4字节整数来表示，其值就是  
>     0x43425355。
> 2.  dCBWTag CBW的标签，由主机分配，设备在完成该命令返回状态时，需要在CSW（命令状  
>     态封包）中的dCSWTag字段中填入命令的dCBWTag。
> 3.  dCBWDataTransferLength 需要在数据阶段传输数据的字节数。小端结构，即低字节在先。
> 4.  bmCBWFlags CBW的标志。最高位（D7）表示数据传输的方向，0表示输出数据（从主机到  
>     设备），1表示输入数据。其他位为0。
> 5.  bCBWLUN 该字段为目标逻辑单元的编号。当有多个逻辑单元时，使用该字段来区分不同的  
>     目标单元。该字段仅使用低4位，高4位为保留值0。
> 6.  bCBWCBLength CBWCB的长度。该字段仅使用5位，有效的取值范围为1~16。不同的命令  
>     （由CBWCB决定），其长度可能是不一样的。如果命令的长度不足16字节，则后面的部分值  
>     为0。
> 7.  CBWCB 需要执行的命令，由选择的子类决定使用那些命令。

### 2.6 命令状态封包CSW的结构

  CSW（Command Status Wrapper)的结构表，用`Bulk-In端点`进行传输，其长度是`13字节`，**用于表示CBW传输的状态。**  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/bcd0b8b3a88bc0b13f3bf231f6800e76.png#pic_center)

> 1.  dCSWSignature 该字段为CSW的标志，为字符串USBS（即USB状态的缩写）。用ASCII码来  
>     表示就是0x55、0x53、0x42、0x53。如果用小端模式的4字节整数表示，其值就是  
>     0x53425355。
> 2.  dCSWTag 该命令状态封包的标签，其值为CBW中的dCBWTag，响应哪个CBW，就设置为哪  
>     个CBW的dCBWTag。
> 3.  dCSWDataResidue 命令完成时的剩余字节数。它表示实际完成传输的字节数与主机在CBW  
>     中设置的长度dCBWDataTransferLength之间的差额。
> 4.  bCSWStatus 命令执行的状态。0x00表示命令执行成功，0x01表示命令执行失败，0x02表示  
>     阶段错误。其他值为保留值。
> 5.  通常，命令都能够成功完成，这时只需要设置前面两个字段为相应的值，后面两个字段都设  
>     置为0即可。

### 2.7 对批量数据的处理

  第一次批量数据，肯定是CBW。定义一个缓冲区，用来接收命令封包CBW。然后进入到数据阶段，在数据阶段中，对CBW进行解码，返回或者接收相应的数据。数据发送或接收完毕后，进入到状态阶段，返回命令执行的情况。然后再次进入命令阶段，等待主机发送CBW命令块封包。

下面我们看下有哪些命令：

### 2.8 查询命令 INQUIRY

  `INQUIRY命令`请求目标设备的一些基本信息。其格式如表。操作代码为`0x12`。其中EVPD字段和页码字段只支持0。分配的缓冲区长度为主机接收该命令返回数据所分配的缓冲区，缓冲区长度通常为`0x24 （即26字节）`。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/53279fe16279e4ec60977e59da671f65.png#pic_center)  
  `INQUIRY命令返回的数据为36字节`。外设类型为0表示直接寻址设备（如磁盘）。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/354ccd4ed3b9761606ccadd6d4b2502c.png#pic_center)  
  `RMB位`表示存储媒介是否可以移除，0为不可移除，1为可移除。  
  `ISO、ECMA、ANSI`等各种版本号规定为0，“响应数据格式”为`0x01`。附加数据长度是后面附加数据的长度，`为31字节`。厂商信息、产品信息、产品版本号可以根据自己的需要设置。

### 2.9 读格式化容量命令 READ FORMAT CAPACITIES

  `READFORMAT CAPACITIES命令`可让主机读取设备各种可能的格式化容量的列表，如果设备中没有存储媒介，则设备返回最大能够支持的格式化容量。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3f5704d5e7aae3880900ef607fa1d45d.png#pic_center)  
  下表是没有存储媒介时返回最大格式容量的数据格式。其中，容量列表长度为8字节（即后面的8字节），描述符代码为3，容量的计算方法为：容量 = 块数 \* 每块字节数  
  通常每块字节数为512字节（即0x200），块数可以设置得大一些，它表示该设备最大支持的格式化容量，实际的容量要由存储媒介来决定。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/84191ecc8318429d7198be5657bc813a.png#pic_center)

### 2.10 读容量命令 READ CAPACITY

  `READ CAPACITY命令`可以让主机读取到当前存储媒介的容量，其格式如表。操作代码为0x25.该命令除了操作代码为0x25之外，其他各字段的值都为0。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/8b91e20b84beeefeee80c424c5f80b7c.png#pic_center)  
READ CAPACITY命令返回数据格式  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/bb7a9b7f13e108712ce9168029bd0606.png#pic_center)

### 2.11 READ（10）命令

  主机通常使用`READ（10）命令`来**读取实际的磁盘数据**，其命令格式如表，操作代码为`0x28`。另外还有一个READ（12）命令（操作代码为0xA8），它的格式跟READ（10）命令差不多，仅传输字段不一样，**READ（12）的字节6-9都为传输长度，而READ（10）命令只有7-8为传输长度**。  
  其中DPO、FUA、RelAdr等字段都为0值。  
  逻辑块地址字段的值为需要读取数据的起始块的地址。在磁盘设备中，读、写通常都是按照块来操作的（所以叫块设备）。一般来说，一个逻辑块就是一个扇区，大小为512字节。当然也有其他大小的逻辑块，例如在光盘文件系统中一个块就是2048字节。  
  传输长度字段的值为需要传输的逻辑块的数量，实际传输的字节数为传输长度值乘以每块大小。  
  设备根据命令中指定的逻辑块地址，从存储媒介中读取数据并通过批量端点返回。当全部数据都返回后，在返回命令执行状态CSW。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/1ff87935e8c202e49e08fff5c37f8465.png#pic_center)

### 2.12 WRITE（10）命令

  主机通常使用`WRITE（10）命令`往设备**写入实际的磁盘数据**，其命令格式如表。操作代码为`0x2A`。另外还有一个WRITE（12）命令（操作代码为0xAA），它的格式跟命令WRITE（10）差不多，仅传输长度字段不一样，它的**WRITE（12）字节6-9都为传输长度，而WRITE（10）命令只有字节7-8为传输长度。**  
  该命令的各参数跟READ（10）命令类似。  
  主机在发送此命令之后，接着就会发出实际要传送的数据，设备在收到全部数据后，返回命令执行的情况CSW。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9270629a75f96ba45d49a0088080cab3.png#pic_center)

### 2.13 REQUEST SENSE命令

  `REQUEST SENSE命令`用来探测上一个命令执行失败的原因，主机可在每个命令之后使用该命令来读取命令执行的情况。其命令代码为0x03，  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c661ba0533eb00a15c01f46ac4f35727.png#pic_center)  
  请求返回的数据格式如表。其中Valid字段指示信息字段是否符合UFI规范，Valid为1时，说明信息字段符合UFI规范。在UFI协议中，信息字段通常用来返回那个逻辑块地址出现了错误。Sense Key、Additional Sense Code(ASC)、Additional Sense Code Qualifier(ASCQ)表示出错的代码，可以在UFI协议的最后找到。  
  本实例中，仅返回一种错误原因：无效的命令操作码，其Sense Key为0x05，ASC为0x20，ASCQ为0。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/ac34ef7cffde5423787144e0df9ba2c6.png#pic_center)

### 2.14 TEST UNIT READY命令

  `TEST UNIT READY命令`用来**测试设备的某个逻辑单元是否准备**好，操作代码为`0x00`，其格式如表。该命令的响应比较简单，如果设备已经准备好，则在状态阶段返回命令执行成功；否则返回命令执行失败。  
  当主机使用REQUEST SENSE命令来探测错误原因时，设置Sense Key为NOT READY。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/0de33d67067ed6301066f35ba7fa371b.png#pic_center)

### 2.15 MODE SENSE命令

  `MODE SENSE命令`允许UFI设备向主机报告媒体或设备的参数。它是MODE SELSET命令的补充。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d289b9dde23bf437a81916078bd86fcb.png#pic_center)

> DBD：禁用的块描述符被设置为0  
> PC：页面控制字段指定要返回的模式参数的类型

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/38bd8824676f06b0bf822f7ba19dddce.png#pic_center)

### 2.16 VERIFY命令

  `Verify命令`要求UFI设备对媒体中的**数据进行校验**  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/bec293dd9a984b2381a179ca0e10240a.png#pic_center)

> **DPO**：该位应该设置为0  
> **ByteChk**：该位应该设置为0；USB-FDU只检查媒体上的CRC数据，不进行数据比较。  
> **RelAdr**：该位应该设置为0；  
> **Logical block address**：逻辑块地址，指定验证操作开始的逻辑块。  
> **Verification length**：校验长度：指定要验证的连续逻辑块的数量。“Verification length”校验长度  
> 为0表示不校验逻辑块。这不会被视为错误，也不会校验任何数据，任何其他值都表示要验证的逻  
> 辑块的数量。

> Result values 返回值  
> 1.如果VERIFY命令成功，UFI设备将检查数据设置为NO sense。  
> 2.如果VERIFY命令因为USB位填充错误或者CRC错误而中止，UFI设备应该将检查数据设置为USB to HOST SYSTEM INTERFACE FAILURE

上面的理论知识了解后，我们大致知道这些U盘读写的命令协议。**那么重点来了，下面我们在Android设备上操作U盘的增删改查操作。**

## 3 U盘在Android的代码实现

### 3.1 AndroidManifest.xml声明

```
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools">
    <!-- 声明使用usb -->
    <uses-feature
        android:name="android.hardware.usb.host"
        android:required="true" />
    <application
        android:allowBackup="true"
        android:dataExtractionRules="@xml/data_extraction_rules"
        android:fullBackupContent="@xml/backup_rules"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:roundIcon="@mipmap/ic_launcher_round"
        android:supportsRtl="true"
        android:theme="@style/Theme.USB"
        tools:targetApi="31">
        <activity
            android:name=".MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
<!--            很多厂家 -->
            <intent-filter>
                <action android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED" />
            </intent-filter>
            <meta-data
                android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED"
                android:resource="@xml/device_filter" />
            <meta-data
                android:name="android.app.lib_name"
                android:value="" />
        </activity>
    </application>

</manifest>
```

device\_filter.xml

```
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <usb-device vendor-id="10473" product-id="649"/>
    <usb-device vendor-id="1336" product-id="0" />
</resources>
```

### 3.2 U盘协议的封装

  我们在上一篇USB协议的Android代码实现的基础上，继续完成通过USB对U盘进行读写操作。下面我们先简单介绍一下代码的结构，  
CommandBlockWrapperNative：固定的命令拼装封装基类，然后通过继承的方式对不同的操作进行对应的指令拼装发生；  
InQueryNative：查询命令拼装类；  
USBHelper：USB的检测工具类；  
UsbConnection：连接设备的管理及数据发送的操作；

下面我们看一下代码：

#### 3.2.1 基类封装

CommandBlockWrapperNative

```
public class CommandBlockWrapperNative {
    
    /**
     * 1  CBW固定标志：USBC，0x43425355
     * 2、标签 相当于请求ID  dCBWTag
     * 3、U盘要返回数据的字节数  dCBWDataTransferLength
     * 4、 bmCBWFlags  最高位 0输出：主机要写出数据到设备 1输入：主机要读取设备数据      80    主机 到 设备  1000 0000    0000 0000  设备到主机
     * 5、目标逻辑单元编号，仅使用低四位
     * 6、cbwcb 长度
     * 7、CBWCB 设备将执行的命令块
     */
    public final static byte IN = (byte) 0x80;
    public final static byte OUT = 0x00;

    public final static int dCBWSignature = 0x43425355;

    int dCBWTag;
    int dCBWDataTransferLength;
    byte bmCBWFlags;
    byte bCBWLUN = 0;

    public CommandBlockWrapperNative(int dCBWTag, int dCBWDataTransferLength,
                                     byte bmCBWFlags) {
        this.dCBWTag = dCBWTag;
        this.dCBWDataTransferLength = dCBWDataTransferLength;
        this.bmCBWFlags = bmCBWFlags;
    }

    public void serialize(ByteBuffer buffer) {
        buffer.order(ByteOrder.LITTLE_ENDIAN)
                .putInt(dCBWSignature)
                // usb
                // 0x 64   100
                .putInt(dCBWTag)
                // 长度 36  0x25
                .putInt(dCBWDataTransferLength)
                // 0x80  读   00  是写
                .put(bmCBWFlags)
                // 写 00000
                .put(bCBWLUN);

    }

    public ByteBuffer dataStage(UsbDeviceConnection deviceConnection,
                                UsbEndpoint endpoint) {
        ByteBuffer buffer = ByteBuffer.allocate(dCBWDataTransferLength);
        buffer.clear();
        deviceConnection.bulkTransfer(endpoint, buffer.array(), 0, dCBWDataTransferLength,
                5000);
        return buffer;
    }

    public void commandStage(UsbDeviceConnection deviceConnection,
                             UsbEndpoint endpoint) {
        ByteBuffer buffer = ByteBuffer.allocate(31);
        buffer.clear();
        serialize(buffer);
        deviceConnection.bulkTransfer(endpoint, buffer.array(), 0,
                31, 5000);
    }

    public ByteBuffer execute(UsbConnection usbConnection) {
        UsbDeviceConnection deviceConnection = usbConnection.getDeviceConnection();
        UsbEndpoint inEndpoint=usbConnection.getInEndpoint();
        UsbEndpoint outEndpoint = usbConnection.getOutEndpoint();

        // 命令阶段
        commandStage(deviceConnection, outEndpoint);

        // 数据阶段
        // 查询1  写入2
        ByteBuffer buffer = dataStage(deviceConnection, inEndpoint);
        // 状态阶段
        // 状态阶段
        stateStage(deviceConnection, inEndpoint);
        return buffer;
    }
    public void stateStage(UsbDeviceConnection deviceConnection,
                           UsbEndpoint endpoint) {

        ByteBuffer buffer = ByteBuffer.allocate(13).order(ByteOrder.LITTLE_ENDIAN);
        buffer.clear();
        deviceConnection.bulkTransfer(endpoint, buffer.array(), 0, 13, 5000);

        // dCBWSignature
        int dCSWSignature = buffer.getInt();
        // dCSWTag
        int dCSWTag = buffer.getInt();
        // dCSWDataResidue 实际数据的字节与主机在CBW中设置的dCBWDataTransferLength差额
        int dCSWDataResidue = buffer.getInt();

        // bCSWStatus 状态 0x00表示成功，0x01 失败，0x02阶段错误
        byte bCSWStatus = buffer.get();
        Log.i("david", "状态: " + String.format("0x%x", dCSWSignature) + " TAG:" + dCSWTag +
                " 差额:" + dCSWDataResidue + " 状态:" + bCSWStatus);
    }
}
```

#### 3.2.2 查询命令拼装类

InQueryNative.java

```
public class InQueryNative  extends  CommandBlockWrapperNative{
    private static final String TAG = "InQueryNative";

    public InQueryNative(int dCBWTag) {
        super(dCBWTag, 0x24, IN);
    }
    @Override
    public void serialize(ByteBuffer buffer) {
        super.serialize(buffer);
        buffer.put((byte) 6)
                //因为都是一个字节，转不转大端都无所谓
                .put((byte) 0x12) //命令码
                .put((byte) 0x00)
                .put((byte) 0x00)
                .put((byte) 0x00)
                .put((byte) 0x24)// 返回数据长度
                .put((byte) 0x00);
    }

    @Override
    public ByteBuffer dataStage(UsbDeviceConnection deviceConnection, UsbEndpoint endpoint) {
        ByteBuffer buffer = super.dataStage(deviceConnection, endpoint);
        // 0-4 位：外设类型 0为磁盘
        buffer.get();
        // ram:  最高位 0为不可移除，1为可移除
        buffer.get();
        // ISO（7-6位） ECMA（5-3位） ANSI（2-0位）版本号，规定为0
        buffer.get();
        // 响应数据格式
        buffer.get();
        //附加数据长度 后面数据长度：31,但是读出来是0x41不知道为啥
        buffer.get();
        //保留
        buffer.get();
        buffer.get();
        buffer.get();
        // 厂商信息
        byte[] manufacturer = new byte[8];
        buffer.get(manufacturer);
        Log.i(TAG, "INQUERY libusb 厂商信息：" + Bytes.toString(manufacturer));
        // 产品信息
        byte[] product = new byte[16];
        buffer.get(product);
        Log.i(TAG, "INQUERY libusb 产品信息：" + Bytes.toString(product));
        // 产品版本信息
        byte[] version = new byte[4];
        buffer.get(version);
        Log.i(TAG, "INQUERY libusb 产品版本信息：" + Bytes.toString(version));
        buffer.clear();
        return buffer;
    }
    
    public static String toString(byte[] buffer) {
        int i = 0;
        for (; i < buffer.length; i++) {
            if ((buffer[i] & 0xff) == 0x20) break;
        }
        return new String(buffer, 0, i);
    }

    public static byte getCheckSum(byte[] data) {
        int sum = 0;
        for (int i = 0; i < 11; i++) {
            sum = ((sum & 1) == 1 ? 0x80 : 0) + ((sum & 0xff) >> 1) + data[i];
        }
        byte checkSum = (byte) (sum & 0xff);
        return checkSum;
    }
}

```

#### 3.2.3 USB连接设备的管理及数据发送

UsbConnection.java

```
public class UsbConnection {
    private static final String TAG = "UsbConnection";
    // device
    private UsbDevice usbDevice;
    private UsbInterface usbInterface;
    // 读   端
    private UsbEndpoint inEndpoint;

    private UsbEndpoint outEndpoint;

    public UsbEndpoint getInEndpoint() {
        return inEndpoint;
    }

    public UsbEndpoint getOutEndpoint() {
        return outEndpoint;
    }

    public UsbDeviceConnection getDeviceConnection() {
        return deviceConnection;
    }

    //    USB链接
    private UsbDeviceConnection deviceConnection;
    public UsbConnection(UsbDevice usbDevice,
                         UsbInterface usbInterface, 
                         UsbEndpoint inEndpoint, 
                         UsbEndpoint outEndpoint) {
        this.usbDevice = usbDevice;
        this.usbInterface = usbInterface;
        this.inEndpoint = inEndpoint;
        this.outEndpoint = outEndpoint;
    }
    
    public void connect(UsbManager usbManager) {
        // 连接
        deviceConnection =usbManager.openDevice(usbDevice);
        deviceConnection.claimInterface(usbInterface, true);
    }

    public void close() {
        deviceConnection.releaseInterface(usbInterface);
        deviceConnection.close();
    }

    // 发送数据
    public boolean sendMessageToPoint(byte[] buffer) {
        boolean state;
        if (deviceConnection==null){
            return false;
        }
        if (outEndpoint==null){
            return false;
        }
        int res = deviceConnection.bulkTransfer(outEndpoint, buffer, buffer.length, 2000);
        if (res < 0) {
            System.out.println("bulkOut返回输出为  负数");
            state = false;
        } else {
            System.out.println("-----------发送数据成功！");
            state=true;
        }
        return state;
    }

    public byte[] receiveMessageFromPoint() {
        if (deviceConnection == null){
            return null;
        }
        if (inEndpoint == null){
            return null;
        }
        int max = inEndpoint.getMaxPacketSize();
        byte[] buffer = new byte[max];
        if (deviceConnection.bulkTransfer(inEndpoint, buffer, buffer.length,
                2000) < 0) {
            System.out.println("------bulkIn返回输出为  负数");
            return null;
        }
        StringBuilder dataSb = new StringBuilder();
        for (byte b : buffer) {
            dataSb.append(Integer.toHexString(b & 0xff));
            dataSb.append(" ");
        }
        return buffer;
    }
}
```

#### 3.2.4 USB的检测工具类

USBHelper.java

```
public class USBHelper {

    private static USBHelper util;
    private static Context mContext;
    private USBHelper(Context _context) { 
        
    }
    
    public static USBHelper getInstance(Context _context) {
        if (util == null) util = new USBHelper(_context);
        mContext = _context;
        return util;
    }
    
    public static void findDevices(UsbManager usbManager, List<UsbDevice> devices) {
        // 连接N个设备
        Map<String, UsbDevice> deviceMap =   usbManager.getDeviceList();
        Collection<UsbDevice> values = deviceMap.values();
        Iterator<UsbDevice> iterator = values.iterator();
        while (iterator.hasNext()) {
            devices.add(iterator.next());
        }
    }
    public static UsbConnection getConnection(UsbDevice usbDevice) {
        for (int i = 0; i < usbDevice.getInterfaceCount(); i++) {
            UsbInterface usbInterface = usbDevice.getInterface(i);
            //android  U盘    类型 U盘 usbInterface  对应一个
            if (usbInterface.getInterfaceClass() == UsbConstants.USB_CLASS_MASS_STORAGE
                    && usbInterface.getInterfaceSubclass() == 0x06
                    && usbInterface.getInterfaceProtocol() == 0x50) {
                //每个存储设备一定有两个端点：in 和 out
                UsbEndpoint outEndpoint = null, inEndpoint = null;
                for (int j = 0; j < usbInterface.getEndpointCount(); j++) {
                    UsbEndpoint endpoint = usbInterface.getEndpoint(j);
                    if (endpoint.getDirection() == UsbConstants.USB_DIR_OUT) {
                        outEndpoint = endpoint;
                    } else {
                        inEndpoint = endpoint;
                    }
                }
                return new UsbConnection(usbDevice, usbInterface, inEndpoint, outEndpoint);
            }
        }
        return null;
    }

}

```

### 3.3 页面展示与交互

好了，下面我们在页面展示出来，写一个RecyclerView，展示在MainActivity页面

#### 3.3.1 RecyclerView的U盘列表展示：

```
public class UsbListAdapter extends BaseAdapter<UsbListAdapter.UsbListViewHolder> {

    private List<UsbDevice> usbDevices;

    public UsbListAdapter(Context context, List<UsbDevice> usbDevices) {
        super(context);
        this.usbDevices = usbDevices;
    }

    @Override
    public int getLayoutId(int viewType) {
        return R.layout.item_usb_info;
    }

    @Override
    protected UsbListViewHolder onCreateViewHolder(View view) {
        return new UsbListViewHolder(view);
    }

    @Override
    protected void onBindVH(UsbListViewHolder holder, int position) {
        UsbDevice usbDevice = usbDevices.get(position);
        holder.device_name.setText(usbDevice.getDeviceName());
        holder.manufacturer_name.setText("制造商:" + usbDevice.getManufacturerName());
        holder.product_name.setText("产品:" + usbDevice.getProductName());
    }

    @Override
    public int getItemCount() {
        return usbDevices.size();
    }

    static class UsbListViewHolder extends RecyclerView.ViewHolder {

        TextView device_name, manufacturer_name, product_name;

        public UsbListViewHolder(@NonNull View itemView) {
            super(itemView);
            device_name = itemView.findViewById(R.id.device_name);
            manufacturer_name = itemView.findViewById(R.id.manufacturer_name);
            product_name = itemView.findViewById(R.id.product_name);
        }
    }


}
```

```
public abstract class BaseAdapter<VH extends RecyclerView.ViewHolder> extends RecyclerView.Adapter<VH> {

    private LayoutInflater mInflater;
    private OnItemClickListener mListener;

    public BaseAdapter(Context context) {
        mInflater = LayoutInflater.from(context);
    }

    public void setOnItemClickListener(OnItemClickListener listener) {
        mListener = listener;
        notifyDataSetChanged();
    }

    public abstract int getLayoutId(int viewType);

    @NonNull
    @Override
    public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = mInflater.inflate(getLayoutId(viewType), parent, false);
        return onCreateViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull VH holder, int position) {
        //创建点击事件
        holder.itemView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mListener.onItemClick(v, position);
            }
        });
        onBindVH(holder, position);
    }

    protected abstract VH onCreateViewHolder(View view);

    protected abstract void onBindVH(VH holder, int position);

    public interface OnItemClickListener {

        public abstract void onItemClick(View view, int position);
    }
}
```

item\_usb\_info.xml

```
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:gravity="center_vertical"
    android:orientation="horizontal"
    android:padding="8dp">

    <TextView
        android:id="@+id/device_name"
        android:layout_width="0dp"
        android:layout_height="wrap_content"
        android:layout_weight="1"
        android:text="/dev/bus/usb/001/002" />

    <LinearLayout
        android:layout_width="0dp"
        android:layout_height="wrap_content"
        android:layout_weight="1"
        android:orientation="vertical">

        <TextView
            android:id="@+id/manufacturer_name"
            android:layout_width="wrap_content"
            android:layout_height="wrap_content"
            android:text="制造商:SanDisk" />

        <View

            android:layout_width="match_parent"
            android:layout_height="1dp"
            android:background="@android:color/darker_gray" />

        <TextView
            android:id="@+id/product_name"
            android:layout_width="wrap_content"
            android:layout_height="wrap_content"
            android:text="产品:Ultra USB 3.0" />
    </LinearLayout>
</LinearLayout>
```

#### 3.3.2 MainActivity页面展示

MainActivity.java

```
public class MainActivity extends AppCompatActivity implements BaseAdapter.OnItemClickListener {

    private UsbManager usbManager;
    private UsbListAdapter usbRVAdapter;
    private List<UsbDevice> devices;
    private UsbReceiver usbReceiver;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        RecyclerView recyclerView = findViewById(R.id.recyclerView);
        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        devices = new ArrayList<>();
        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
        USBHelper.findDevices(usbManager, devices);
        System.out.println(devices);
        usbRVAdapter = new UsbListAdapter(this,devices);
        usbRVAdapter.setOnItemClickListener(this);
        recyclerView.setAdapter(usbRVAdapter);
    }

    @Override
    public void onItemClick(View view, int position) {
        UsbDevice usbDevice = devices.get(position);
        if (!usbManager.hasPermission(usbDevice)) {
            // todo:申请权限
            Log.i("MainActivity", "无权限，需要申请权限！")
            return;
        }
        UsbConnection usbConnection= USBHelper.getConnection(usbDevice);
        usbConnection.connect(usbManager);
        InQueryNative inQueryNative = new InQueryNative(100);
        inQueryNative.execute(usbConnection);
    }
}
```

## 4 总结

  USB协议我们从理论到代码过了一遍，相信聪明的小伙伴应该有所收获。最后做下总结吧，USB协议是一套完整的传输协议，使用其传输的两端必须按照一致的指令协议来实现通讯，实现读写操作。  
  对于Android开发来说，能够掌握基本的USB读写操作就可以了，具体协议的详细在需要用到的时候再查找文档对应实现。**日常开发和工作中的数据读写操作，大部分都需要自定义协议，可以参考U盘等这些USB设备的协议来自定义一套适合自己的协议。**