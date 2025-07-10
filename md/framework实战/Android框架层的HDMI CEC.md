
> 转载自http://www.cjcbill.com/2020/04/20/hdmi-cec/

## 1\. 背景

由于项目需求，需要了解Android框架层的HDMI CEC的工作原理，关注的重点是OTT作为CEC的source端如何和与TV端的sink端进行通信。 学习代码以Android的最新代码库https://cs.android.com/中截取，分支为master分支。

## 2\. 总体概述

### 2.1 设计架构

为了迅速了解整个设计架构，可以先去Google官网查阅相关信息:https://source.android.com/devices/tv/hdmi-cec,CEC的功能最主要包括:

可以看到所有的应用，都会间接通过HDMIControlManager或者输入通过Tv Input框架间接与HdmiControlService进行通信，HdmiControlService作为SystemServer服务的一个服务，负责处理CEC的命令并与HDMI-CEC HAl进行交互。HAL层和驱动都需要厂商去适配，最后通过CEC总线与CEC设备通信。

至于HDMI的设计架构，分为source端以及sink端，可以有多个source输入，也可以有多个sink输出。四个TMDS数据和时钟通道用于传输video,audio和辅助数据。DDC(Display Data Channel)用于单个source和sink端进行状态交换。CEC总线能够提供在不同音视频设备中进行控制等等。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/26e86afbf21d59d35aa4f599d0597361.png)

### 2.2 源码结构

本文涉及的代码分析包括:

+   frameworks/base/services/core/java/com/android/server/hdmi
+   frameworks/base/services/core/jni/ #对应的jni实现
+   hardware/interfaces/tv/cec/1.0/ #　Hal层接口
+   device/amlogic/yukawa/hdmicec/ #厂商实现，本例以Amlogic作为分析

## 3\. HDMI-CEC

本文重点关注自动开机和自动关机两个通路，以OTT作为source端，TV作为sink端为前提进行分析。自动开机的意思是当在TvSettings中设了自动开机后，两个设备均为关机状态，那么无论使用哪一方遥控器，都能够唤醒对端的设备。自动关机的功能与自动开机功能类似。

为了要支持HDMI-CEC,Android官方文档指出需要首先在方案中进行配置:

```cpp
PRODUCT_COPY_FILES += \
frameworks/native/data/etc/android.hardware.hdmi.cec.xml:system/etc/permissions/android.hardware.hdmi.cec.xml
```

OTT设备需要设置如下:

```cpp
PRODUCT_PROPERTY_OVERRIDES += ro.hdmi.device_type=4
```

Tv设备需要设置如下:

```cpp
PRODUCT_PROPERTY_OVERRIDES += ro.hdmi.device_type=0
```

本文以source的角度进行分析。

### 3.1 HdmiControlService

在分析通路之前，需要了解下HdmiContolService。之前提到为了要开启该服务，需要将android.hardware.hdmi.cec.xml拷贝到system/etc/permissions目录下，这是因为SystemServer在启动时会去检查权限目录下有没有hdmi.cec的xml文件:

```cpp
//framework/base/services/java/com/android/server/SystemServer.java
private void startOtherServices() {
...
    if (mPackageManager.hasSystemFeature(PackageManager.FEATURE_HDMI_CEC)) {
        traceBeginAndSlog("StartHdmiControlService");
        mSystemServiceManager.startService(HdmiControlService.class);
        traceEnd();
    }
...
}
```

转到HdmiControlService看其构造以及onStart方法:

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiControlService.java
//mLocalDevices是Integer列表
private final List<Integer> mLocalDevices;
//Handler用于在service线程运行,因为使用了默认的Looper。
private final Handler mHandler = new Handler();
...
public HdmiControlService(Context context) {
    super(context);
    //这里可以看出，当设置了"ro.hdmi.device_type"时，mLocalDevices就会根据属性
    //生成对应类型的LocalDevice
    mLocalDevices = getIntList(SystemProperties.get(Constants.PROPERTY_DEVICE_TYPE));
    //mSettingsObserver是为了后续创建ContentObserver监听数据库的变化做准备
    mSettingsObserver = new SettingsObserver(mHandler);
}

@Override
public void onStart() {
    //启动Io线程
    if (mIoLooper == null) {
        mIoThread.start();
        //后续将mIoLooper传到HdmiCecController的handler中，使得消息在mIoThread线程中处理。
        mIoLooper = mIoThread.getLooper();
    }
    mPowerStatus = HdmiControlManager.POWER_STATUS_TRANSIENT_TO_ON;
    mProhibitMode = false;
    //数据库hdmi_control_enabled用于控制Hdmi控制是否使能
    mHdmiControlEnabled = readBooleanSetting(Global.HDMI_CONTROL_ENABLED, true);
    ...
    //新建CecController,初始化Native层
    if (mCecController == null) {
        mCecController = dHdmiCecController.create(this);
    }
    if (mCecController != null) {
        if (mHdmiControlEnabled) {
            /*
            仅有mHdmiControlEnabled打开时，初始化Cec.
            注意到初始化还会传入初始化原因，包括:
            1. static final int INITIATED_BY_ENABLE_CEC = 0;
            2. static final int INITIATED_BY_BOOT_UP = 1;
            3. static final int INITIATED_BY_SCREEN_ON = 2;
            4. static final int INITIATED_BY_WAKE_UP_MESSAGE = 3;
            5. static final int INITIATED_BY_HOTPLUG = 4;
            */
            initializeCec(INITIATED_BY_BOOT_UP);
        } else {
            //假如mHdmiControlEnabled关闭时，向底层发送ENABLE_CEC设置为false
            mCecController.setOption(OptionKey.ENABLE_CEC, false);
        }
    } else {
        Slog.i(TAG, "Device does not support HDMI-CEC.");
        return;
    }
    ...
    initPortInfo();
    if (mMessageValidator == null) {
        mMessageValidator = new HdmiCecMessageValidator(this);
    }
    //创建Binder服务，用以和HdmiControlService进行交互
    publishBinderService(Context.HDMI_CONTROL_SERVICE, new BinderService());
    //注册广播接收器，用以监听如关屏，开屏，关机，配置改变的广播
    if (mCecController != null) {
        // Register broadcast receiver for power state change.
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SHUTDOWN);
        filter.addAction(Intent.ACTION_CONFIGURATION_CHANGED);
        getContext().registerReceiver(mHdmiControlBroadcastReceiver, filter);
        /*
         注册ContentObserver用以监听数据库的变化,包括:
            Global.HDMI_CONTROL_ENABLED,
            Global.HDMI_CONTROL_AUTO_WAKEUP_ENABLED,
            Global.HDMI_CONTROL_AUTO_DEVICE_OFF_ENABLED,
            Global.HDMI_SYSTEM_AUDIO_CONTROL_ENABLED,
            Global.MHL_INPUT_SWITCHING_ENABLED,
            Global.MHL_POWER_CHARGE_ENABLED,
            Global.HDMI_CEC_SWITCH_ENABLED,
            Global.DEVICE_NAME
        */
        registerContentObserver();
    }
    ...
}
```

### 3.2 HdmiController

看完服务的启动流程后，还需要看下控制器HdmiContoller，它在服务中的启动是调用create方法，跟着入口看实现逻辑:

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiCecController.java
static HdmiCecController create(HdmiControlService service) {
    //新建wrapper类，用于方便调用HdmiCecController的native接口
    return createWithNativeWrapper(service, new NativeWrapperImpl());
}

static HdmiCecController createWithNativeWrapper(
    HdmiControlService service, NativeWrapper nativeWrapper) {
        //这里才新建HdmiCecController
        HdmiCecController controller = new HdmiCecController(service, nativeWrapper);
        //nativeInit返回的是Native层HdmiCecController的对象的地址。
        long nativePtr = nativeWrapper
            .nativeInit(controller, service.getServiceLooper().getQueue());
        if (nativePtr == 0L) {
            controller = null;
            return null;
        }
        //HdmiCecController初始化
        controller.init(nativePtr);
        return controller;
}

private void init(long nativePtr) {
    //将IoLooper设置到新建mIoHandler中，得以将处理流程放在Io线程中。
    mIoHandler = new Handler(mService.getIoLooper());
    //将服务流程Looper放到新建mControlHandler中
    mControlHandler = new Handler(mService.getServiceLooper());
    mNativePtr = nativePtr;
}
```

至此，可以进入流程分析。

### 3.3 单键休眠

单键休眠应当分为两个方向，一是从source端，使用source的遥控器单击休眠按键，此时source端进入休眠，sink端也进入休眠。二是使用sink端遥控器，使得sink端进入休眠后，source端也进入休眠。从而真正实现单键休眠功能。

#### 3.3.1 SourceToSink

从source端设置休眠或者关机流程如下所示:  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/2806b334be541faa00c1305feec299b1.png)  
一切分析的源头来源于广播接收器，当PowerManagerService收到关机/休眠命令时，调用goToSleep并发送关屏或者关机广播，而此时HdmiControlService在启动时，会注册广播接收器HdmiControlBroadcastReceiver，用于监听这些关键广播，为的就是及时通知到驱动并将信息传送到CEC总线到sink端。

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiControlService.java
private class HdmiControlBroadcastReceiver extends BroadcastReceiver {
@ServiceThreadOnly
@Override
public void onReceive(Context context, Intent intent) {
    //确定该流程是在Service线程进行的。
    assertRunOnServiceThread();
    boolean isReboot = SystemProperties.get(SHUTDOWN_ACTION_PROPERTY).contains("1");
    switch (intent.getAction()) {
        case Intent.ACTION_SCREEN_OFF:
            //关屏前，检查mPowerStatus是否为POWER_STATUS_ON状态或者POWER_STATUS_TRANSIENT_TO_ON状态
            //且当前不是重启状态
            if (isPowerOnOrTransient() && !isReboot) {
                //调用onStandby,传参STANDBY_SCREEN_OFF
                onStandby(STANDBY_SCREEN_OFF);
            }
            break;
            ...
        case Intent.ACTION_SHUTDOWN:
            if (isPowerOnOrTransient() && !isReboot) {
                //调用onStandby,传参STANDBY_SHUTDOWN
                onStandby(STANDBY_SHUTDOWN);
            }
            break;
    }
}

@ServiceThreadOnly
@VisibleForTesting
protected void onStandby(final int standbyAction) {
    assertRunOnServiceThread();
    mPowerStatus = HdmiControlManager.POWER_STATUS_TRANSIENT_TO_STANDBY;
    //假如客户端设置了VendorCommandListener,在调用onStandby时会通知到客户端,
    //并告知原因为CONTROL_STATE_CHANGED_REASON_STANDBY
    invokeVendorCommandListenersOnControlStateChanged(false,
            HdmiControlManager.CONTROL_STATE_CHANGED_REASON_STANDBY);
    //获取LocalDevices，此处为Playback设备。
    final List<HdmiCecLocalDevice> devices = getAllLocalDevices();
    //假如还没收到sink端的消息，且设备设备的canGoToStandby还没有准备好进入休眠模式
    if (!isStandbyMessageReceived() && !canGoToStandby()) {
        //设置全局变量mPowerStatus为POWER_STATUS_STANDBY。
        mPowerStatus = HdmiControlManager.POWER_STATUS_STANDBY;
        for (HdmiCecLocalDevice device : devices) {
            //调用Playback设备的onStandby方法
            device.onStandby(mStandbyMessageReceived, standbyAction);
        }
        return;
    }
    //假设已经收到休眠消息　或者已经能够进入休眠状态了，此时再调用onStandBy，会调用disableDevices,
    //其实质是调用PlayBackdevice的disableDevice.并新建了一个callback,等待disable完成后，调用onCleared设置环境
    disableDevices(new PendingActionClearedCallback() {
        @Override
        public void onCleared(HdmiCecLocalDevice device) {
            Slog.v(TAG, "On standby-action cleared:" + device.mDeviceType);
            devices.remove(device);
            if (devices.isEmpty()) {
                onStandbyCompleted(standbyAction);
            }
        }
    });
}

private void disableDevices(PendingActionClearedCallback callback) {
    if (mCecController != null) {
        for (HdmiCecLocalDevice device : mCecController.getLocalDeviceList()) {
            device.disableDevice(mStandbyMessageReceived, callback);
        }
    }
    ....
```

HdmiCecLocalDevice为HdmiCecLocalDevicePlayback类型，当收到STANDBY\_SCREEN\_OFF和STANDBY\_SHUTDOWN时,发送CEC命令。

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiCecLocalDevicePlayback.java
@Override
    @ServiceThreadOnly
    protected void onStandby(boolean initiatedByCec, int standbyAction) {
        assertRunOnServiceThread();
        if (!mService.isControlEnabled() || initiatedByCec || !mAutoTvOff) {
            return;
        }
        switch (standbyAction) {
            case HdmiControlService.STANDBY_SCREEN_OFF:
                //source端为localDevice,所以为mAddress.dest为ADDR_TV，电视的地址
                mService.sendCecCommand(
                        HdmiCecMessageBuilder.buildStandby(mAddress, Constants.ADDR_TV));
                break;
            case HdmiControlService.STANDBY_SHUTDOWN:
                //source端为localDevice,所以为mAddress.dest为ADDR_BROADCAST，广播的地址
                mService.sendCecCommand(
                        HdmiCecMessageBuilder.buildStandby(mAddress, Constants.ADDR_BROADCAST));
                break;
        }
    }
```

在深入看HdmiControlService是如何发送Cec命令前，有必要看下HdmiCecMessageBuilder是如何生成命令的:

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiCecMessageBuilder.java
public static HdmiCecMessage buildStandby(int src, int dest) {
    return buildCommand(src, dest, Constants.MESSAGE_STANDBY);
}

private static HdmiCecMessage buildCommand(int src, int dest, int opcode) {
    //原来是返回了一新的HdmiCecMessage对象，并设置了opcode,src端以及dest端。
    return new HdmiCecMessage(src, dest, opcode, HdmiCecMessage.EMPTY_PARAM);
}

public HdmiCecMessage(int source, int destination, int opcode, byte[] params) {
    mSource = source;
    mDestination = destination;
    mOpcode = opcode & 0xFF;
    mParams = Arrays.copyOf(params, params.length);
}
```

了解了HdmiCecMessage的结构后，再回过头来看下HdmiControlService的sendCecCommand:

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiControlService.java
@ServiceThreadOnly
   void sendCecCommand(HdmiCecMessage command, @Nullable SendMessageCallback callback) {
       assertRunOnServiceThread();
       //MessageValidator会检查command命令是否有效，分别从source，dest等进行分析。
       if (mMessageValidator.isValid(command) == HdmiCecMessageValidator.OK) {
           //有效的命令将允许通过HdmiCecController发送下去
           mCecController.sendCommand(command, callback);
       } else {
           HdmiLogger.error("Invalid message type:" + command);
           if (callback != null) {
               callback.onSendCompleted(SendMessageResult.FAIL);
           }
       }
   }
```

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiCecController.java
@ServiceThreadOnly
void sendCommand(final HdmiCecMessage cecMessage,
        final HdmiControlService.SendMessageCallback callback) {
    assertRunOnServiceThread();
    //生成一个MessageHistoryRecord对象，加入到ArrayBlockingQueue中进行管理。
    addMessageToHistory(false /* isReceived */, cecMessage);
    //想运行到IoThread？只需要定义一个Runnable对象并post到IoThread中即可。
    runOnIoThread(new Runnable() {
        @Override
        public void run() {
            HdmiLogger.debug("[S]:" + cecMessage);
            //传的是二进制数byte
            byte[] body = buildBody(cecMessage.getOpcode(), cecMessage.getParams());
            int i = 0;
            int errorCode = SendMessageResult.SUCCESS;
            do {
                //核心是通过NativeWrapperImpl将消息发送到Native层。
                errorCode = mNativeWrapperImpl.nativeSendCecCommand(mNativePtr,
                    cecMessage.getSource(), cecMessage.getDestination(), body);
                if (errorCode == SendMessageResult.SUCCESS) {
                    break;
                }
                //将会在限定次数内进行尝试
            } while (i++ < HdmiConfig.RETRANSMISSION_COUNT);

            final int finalError = errorCode;
            if (finalError != SendMessageResult.SUCCESS) {
                Slog.w(TAG, "Failed to send " + cecMessage + " with errorCode=" + finalError);
            }
            //假如回调不为空，将会在Service线程里运行回调方法，告诉调用方已经发送完成了。
            if (callback != null) {
                runOnServiceThread(new Runnable() {
                    @Override
                    public void run() {
                        callback.onSendCompleted(finalError);
                    }
                });
            }
        }
    });
}
```

调到nativeSnedCecCommand看Native层的逻辑：

```cpp
//frameworks/base/services/core/jni/com_android_server_hdmi_HdmiCecController.cpp
static jint nativeSendCecCommand(JNIEnv* env, jclass clazz, jlong controllerPtr,
        jint srcAddr, jint dstAddr, jbyteArray body) {
    //将Java层的信息再次封装一层到Native的CecMessage中。
    CecMessage message;
    message.initiator = static_cast<CecLogicalAddress>(srcAddr);
    message.destination = static_cast<CecLogicalAddress>(dstAddr);

    jsize len = env->GetArrayLength(body);
    ScopedByteArrayRO bodyPtr(env, body);
    size_t bodyLength = MIN(static_cast<size_t>(len),
            static_cast<size_t>(MaxLength::MESSAGE_BODY));
    message.body.resize(bodyLength);
    for (size_t i = 0; i < bodyLength; ++i) {
        message.body[i] = static_cast<uint8_t>(bodyPtr[i]);
    }
    //转到Native层的HdmiCecController发送消息
    HdmiCecController* controller =
            reinterpret_cast<HdmiCecController*>(controllerPtr);
    return controller->sendMessage(message);
}

int HdmiCecController::sendMessage(const CecMessage& message) {
    //调用HIDL接口
    Return<SendMessageResult> ret = mHdmiCec->sendMessage(message);
    if (!ret.isOk()) {
        ALOGE("Failed to send CEC message.");
        return static_cast<int>(SendMessageResult::FAIL);
    }
    return static_cast<int>((SendMessageResult) ret);
}
```

```cpp
//hardware/interfaces/tv/cec/HdmiCec.cpp
Return<SendMessageResult> HdmiCec::sendMessage(const CecMessage& message) {
    cec_message_t legacyMessage {
        .initiator = static_cast<cec_logical_address_t>(message.initiator),
        .destination = static_cast<cec_logical_address_t>(message.destination),
        .length = message.body.size(),
    };
    for (size_t i = 0; i < message.body.size(); ++i) {
        legacyMessage.body[i] = static_cast<unsigned char>(message.body[i]);
    }
    //此处调用到HAL层实现
    return static_cast<SendMessageResult>(mDevice->send_message(mDevice, &legacyMessage));
}
```

由于各大厂商的HAL实现均不同，挑选高通的代码简单分析下流程:

```cpp
//hardware/qcom/sm8150/display/hdmi_cec/qhdmi_cec.cpp

static int cec_device_open(const struct hw_module_t* module,
        const char* name,
        struct hw_device_t** device)
{
    ALOGD_IF(DEBUG, "%s: name: %s", __FUNCTION__, name);
    int status = -EINVAL;
    if (!strcmp(name, HDMI_CEC_HARDWARE_INTERFACE )) {
        struct cec_context_t *dev;
        dev = (cec_context_t *) calloc (1, sizeof(*dev));
        if (dev) {
            cec_init_context(dev);

            //Setup CEC methods
            dev->device.common.tag       = HARDWARE_DEVICE_TAG;
            dev->device.common.version   = HDMI_CEC_DEVICE_API_VERSION_1_0;
            dev->device.common.module    = const_cast<hw_module_t* >(module);
            dev->device.common.close     = cec_device_close;
            dev->device.add_logical_address = cec_add_logical_address;
            dev->device.clear_logical_address = cec_clear_logical_address;
            dev->device.get_physical_address = cec_get_physical_address;
            dev->device.send_message = cec_send_message;//对应的cec_send_message
            dev->device.register_event_callback = cec_register_event_callback;
            dev->device.get_version = cec_get_version;
            dev->device.get_vendor_id = cec_get_vendor_id;
            dev->device.get_port_info = cec_get_port_info;
            dev->device.set_option = cec_set_option;
            dev->device.set_audio_return_channel = cec_set_audio_return_channel;
            dev->device.is_connected = cec_is_connected;

            *device = &dev->device.common;
            status = 0;
        } else {
            status = -EINVAL;
        }
    }
    return status;
}
}; //namespace qhdmicec



static int cec_send_message(const struct hdmi_cec_device* dev,
        const cec_message_t* msg)
{
    ATRACE_CALL();
    if(cec_is_connected(dev, 0) <= 0)
        return HDMI_RESULT_FAIL;

    cec_context_t* ctx = (cec_context_t*)(dev);
    ALOGD_IF(DEBUG, "%s: initiator: %d destination: %d length: %u",
            __FUNCTION__, msg->initiator, msg->destination,
            (uint32_t) msg->length);

    // Dump message received from framework
    char dump[128];
    if(msg->length > 0) {
        hex_to_string((char*)msg->body, msg->length, dump);
        ALOGD_IF(DEBUG, "%s: message from framework: %s", __FUNCTION__, dump);
    }

    char write_msg_path[MAX_PATH_LENGTH];
    char write_msg[MAX_CEC_FRAME_SIZE];
    memset(write_msg, 0, sizeof(write_msg));
    //开始解析msg内容
    write_msg[CEC_OFFSET_SENDER_ID] = msg->initiator;
    write_msg[CEC_OFFSET_RECEIVER_ID] = msg->destination;
    //Kernel splits opcode/operand, but Android sends it in one byte array
    write_msg[CEC_OFFSET_OPCODE] = msg->body[0];
    if(msg->length > 1) {
        memcpy(&write_msg[CEC_OFFSET_OPERAND], &msg->body[1],
                sizeof(char)*(msg->length - 1));
    }
    //msg length + initiator + destination
    write_msg[CEC_OFFSET_FRAME_LENGTH] = (unsigned char) (msg->length + 1);
    hex_to_string(write_msg, sizeof(write_msg), dump);
    ALOGD_IF(DEBUG, "%s: message to driver: %s", __FUNCTION__, dump);
    snprintf(write_msg_path, sizeof(write_msg_path), "%s/cec/wr_msg",
            ctx->fb_sysfs_path);
    int retry_count = 0;
    ssize_t err = 0;
    //HAL spec requires us to retry at least once.
    while (true) {
        //最后调用write_node将信息写入
        err = write_node(write_msg_path, write_msg, sizeof(write_msg));
        retry_count++;
        if (err == -EAGAIN && retry_count <= MAX_SEND_MESSAGE_RETRIES) {
            ALOGE("%s: CEC line busy, retrying", __FUNCTION__);
        } else {
            break;
        }
    }

    if (err < 0) {
       if (err == -ENXIO) {
           ALOGI("%s: No device exists with the destination address",
                   __FUNCTION__);
           return HDMI_RESULT_NACK;
       } else if (err == -EAGAIN) {
            ALOGE("%s: CEC line is busy, max retry count exceeded",
                    __FUNCTION__);
            return HDMI_RESULT_BUSY;
        } else {
            return HDMI_RESULT_FAIL;
            ALOGE("%s: Failed to send CEC message err: %zd - %s",
                    __FUNCTION__, err, strerror(int(-err)));
        }
    } else {
        ALOGD_IF(DEBUG, "%s: Sent CEC message - %zd bytes written",
                __FUNCTION__, err);
        return HDMI_RESULT_SUCCESS;
    }
}

//write_node实际是将data写到对应的节点中
static ssize_t write_node(const char *path, const char *data, size_t len)
{
    ssize_t err = 0;
    int fd = -1;
    err = access(path, W_OK);
    if (!err) {
        fd = open(path, O_WRONLY);
        errno = 0;
        err = write(fd, data, len);
        if (err < 0) {
            err = -errno;
        }
        close(fd);
    } else {
        ALOGE("%s: Failed to access path: %s error: %s",
                __FUNCTION__, path, strerror(errno));
        err = -errno;
    }
    return err;
}
```

至此分析完source端到sink端的框架流程,HAL层的代码各厂商实现都不同，需要结合实际平台分析。

#### 3.3.2 SinkToSource

首先贴出HAL层以上的流程图:  
\[暂无\]  
为了更好的了解整个过程，需要再深入一点qcom的代码:

```cpp
//hardware/qcom/sm8150/display/sdm/libs/core/fb/hw_events.cpp
DisplayError HWEvents::Init(int fb_num, DisplayType display_type, HWEventHandler *event_handler,
                            const vector<HWEvent> &event_list, const HWInterface *hw_intf) {
  if (!event_handler)
    return kErrorParameters;

  event_handler_ = event_handler;
  fb_num_ = display_type;
  event_list_ = event_list;
  poll_fds_.resize(event_list_.size());
  event_thread_name_ += " - " + std::to_string(fb_num_);
  //读cec/rd_msg节点来获取cec信息
  map_event_to_node_ = {
						{HWEvent::VSYNC, "vsync_event"},
                        {HWEvent::EXIT, "thread_exit"},
                        {HWEvent::IDLE_NOTIFY, "idle_notify"},
                        {HWEvent::SHOW_BLANK_EVENT, "show_blank_event"},
                        {HWEvent::CEC_READ_MESSAGE, "cec/rd_msg"},
                        {HWEvent::THERMAL_LEVEL, "msm_fb_thermal_level"},
                        {HWEvent::IDLE_POWER_COLLAPSE, "idle_power_collapse"},
                        {HWEvent::PINGPONG_TIMEOUT, "pingpong_timeout"}
						};
　//处理HWEventData
  PopulateHWEventData();
  //创建线程读取节点信息
  if (pthread_create(&event_thread_, NULL, &DisplayEventThread, this) < 0) {
    DLOGE("Failed to start %s, error = %s", event_thread_name_.c_str());
    return kErrorResources;
  }

  return kErrorNone;
}
```

```cpp
//hardware/qcom/sm8150/display/sdm/libs/core/fb/hw_events.cpp
void* HWEvents::DisplayEventHandler() {
  char data[kMaxStringLength] = {0};

  prctl(PR_SET_NAME, event_thread_name_.c_str(), 0, 0, 0);
  setpriority(PRIO_PROCESS, 0, kThreadPriorityUrgent);

  while (!exit_threads_) {
    //没有消息时阻塞
    int error = Sys::poll_(poll_fds_.data(), UINT32(event_list_.size()), -1);

    if (error <= 0) {
      DLOGW("poll failed. error = %s", strerror(errno));
      continue;
    }

    for (uint32_t event = 0; event < event_list_.size(); event++) {
      pollfd &poll_fd = poll_fds_[event];

      if (event_list_.at(event) == HWEvent::EXIT) {
        if ((poll_fd.revents & POLLIN) && (Sys::read_(poll_fd.fd, data, kMaxStringLength) > 0)) {
        　//event_parser为函数指针，在处理cec消息时，指向&HWEvents::HandleCECMessage
          (this->*(event_data_list_[event]).event_parser)(data);
        }
      } else {
        if ((poll_fd.revents & POLLPRI) &&
                (Sys::pread_(poll_fd.fd, data, kMaxStringLength, 0) > 0)) {
          (this->*(event_data_list_[event]).event_parser)(data);
        }
      }
    }
  }
  pthread_exit(0);
  return NULL;
}

//调用CECMessage
void HWEvents::HandleCECMessage(char *data) {
  event_handler_->CECMessage(data);
}
```

event\_handler\_指向的是HWCDisplay，其CECMessage实现是:

```cpp
//hardware/qcom/sm8150/display/sdm/libs/hwc2/hwc_display.cpp
DisplayError HWCDisplay::CECMessage(char *message) {
  if (qservice_) {
    /*
      调用qservice的onCECMessageReceived,qservice是一个binder服务,
      注册到ServiceManager中，其服务名为display.qservice，这里的调用涉及到Binder通讯
    */
    qservice_->onCECMessageReceived(message, 0);
  } else {
    DLOGW("Qservice instance not available.");
  }
  return kErrorNone;
}

//hardware/qcom/sm8150/display/libqservice/QService.cpp
void QService::onCECMessageReceived(char *msg, ssize_t len) {
    if(mHDMIClient.get()) {
        ALOGD_IF(QSERVICE_DEBUG, "%s: CEC message received", __FUNCTION__);
        mHDMIClient->onCECMessageRecieved(msg, len);
    } else {
        ALOGW("%s: Failed to get a valid HDMI client", __FUNCTION__);
    }
}

```

```cpp
//hardware/qcom/sm8150/display/hdmi_cec/QHDMIClient.cpp
void QHDMIClient::onCECMessageRecieved(char *msg, ssize_t len)
{
    ALOGD_IF(DEBUG, "%s: CEC message received len: %zd", __FUNCTION__, len);
    //到了关键的步骤
    cec_receive_message(mCtx, msg, len);
}
```

```cpp
//
void cec_receive_message(cec_context_t *ctx, char *msg, ssize_t len)
{
    if(!ctx->system_control)
        return;

    char dump[128];
    if(len > 0) {
        hex_to_string(msg, len, dump);
        ALOGD_IF(DEBUG, "%s: Message from driver: %s", __FUNCTION__, dump);
    }
    //使用hdmi_event_t这个结构体，并填充信息
    hdmi_event_t event;
    event.type = HDMI_EVENT_CEC_MESSAGE;
    event.dev = (hdmi_cec_device *) ctx;
    // Remove initiator/destination from this calculation
    event.cec.length = msg[CEC_OFFSET_FRAME_LENGTH] - 1;
    event.cec.initiator = (cec_logical_address_t) msg[CEC_OFFSET_SENDER_ID];
    event.cec.destination = (cec_logical_address_t) msg[CEC_OFFSET_RECEIVER_ID];
    //Copy opcode and operand
    size_t copy_size = event.cec.length > sizeof(event.cec.body) ?
                       sizeof(event.cec.body) : event.cec.length;
    memcpy(event.cec.body, &msg[CEC_OFFSET_OPCODE],copy_size);
    hex_to_string((char *) event.cec.body, copy_size, dump);
    ALOGD_IF(DEBUG, "%s: Message to framework: %s", __FUNCTION__, dump);
    //调用回调方法处理信息
    ctx->callback.callback_func(&event, ctx->callback.callback_arg);
}
```

至此，可以看到HAL层在通过poll监听cec节点的消息，当有消息时，将其一再封装，并通过binder通信发送，最后使用这个回调方法进行处理，而回调方法正是在上层初始化时注册时设置的。回过头看之前Native的HdmiCecController是如何初始化的:

```cpp
//frameworks/base/services/core/jni/com_android_server_hdmi_HdmiCecController.cpp
HdmiCecController::HdmiCecController(sp<IHdmiCec> hdmiCec,
        jobject callbacksObj, const sp<Looper>& looper)
        : mHdmiCec(hdmiCec),
          mCallbacksObj(callbacksObj),
          mLooper(looper) {
    //新建了一个HdmiCecCallback对象，通过setCallback设置回调
    mHdmiCecCallback = new HdmiCecCallback(this);
    Return<void> ret = mHdmiCec->setCallback(mHdmiCecCallback);
    if (!ret.isOk()) {
        ALOGE("Failed to set a cec callback.");
    }
}
```

上面的setCallback实际就是一层一层最后调用到HAL层的register\_event\_callback,最后对ctx->callback.callback\_fun进行了赋值。再看下HdmiCecCallback的实现:

```cpp
//frameworks/base/services/core/jni/com_android_server_hdmi_HdmiCecController.cpp
class HdmiCecCallback : public IHdmiCecCallback {
    public:
        explicit HdmiCecCallback(HdmiCecController* controller) : mController(controller) {};
        Return<void> onCecMessage(const CecMessage& event)  override;
        Return<void> onHotplugEvent(const HotplugEvent& event)  override;
    private:
        HdmiCecController* mController;
    };
  //实现了onCecMessage方法，即之前cec_receive_message调用的就是这个回调对象的方法
  Return<void> HdmiCecController::HdmiCecCallback::onCecMessage(const CecMessage& message) {
    //处理的Handler为HdmiCecEventHandler
    sp<HdmiCecEventHandler> handler(new HdmiCecEventHandler(mController, message));
    //在Native层发送消息，类型为CEC_MESSAGE
    mController->mLooper->sendMessage(handler, HdmiCecEventHandler::EventType::CEC_MESSAGE);
    return Void();
}

class HdmiCecEventHandler : public MessageHandler {
....
void handleMessage(const Message& message) {
        switch (message.what) {
        case EventType::CEC_MESSAGE:
            //处理CecCommand
            propagateCecCommand(mCecMessage);
            break;
        case EventType::HOT_PLUG:
            propagateHotplugEvent(mHotplugEvent);
            break;
        default:
            // TODO: add more type whenever new type is introduced.
            break;
        }
    }
...
    void propagateCecCommand(const CecMessage& message) {
        JNIEnv* env = AndroidRuntime::getJNIEnv();
        jint srcAddr = static_cast<jint>(message.initiator);
        jint dstAddr = static_cast<jint>(message.destination);
        jbyteArray body = env->NewByteArray(message.body.size());
        const jbyte* bodyPtr = reinterpret_cast<const jbyte *>(message.body.data());
        env->SetByteArrayRegion(body, 0, message.body.size(), bodyPtr);
        //从Native层调用Java方法，handleIncomingCecCommand。
        env->CallVoidMethod(mController->getCallbacksObj(),
                gHdmiCecControllerClassInfo.handleIncomingCecCommand, srcAddr,
                dstAddr, body);
        env->DeleteLocalRef(body);
        checkAndClearExceptionFromCallback(env, __FUNCTION__);
    }
}
```

再看看Java的实现handleIncomingCecCommand:

```cpp
    //frameworks/base/services/core/java/com/android/server/hdmi/HdmiCecController.java
    @ServiceThreadOnly
    private void handleIncomingCecCommand(int srcAddress, int dstAddress, byte[] body) {
        assertRunOnServiceThread();
        HdmiCecMessage command = HdmiCecMessageBuilder.of(srcAddress, dstAddress, body);
        HdmiLogger.debug("[R]:" + command);
        //生成一个新的MessageHistoryRecord对象加入到历史中(ArrayBlockingQueue)
        addMessageToHistory(true /* isReceived */, command);
        //调用onReceiveCommand
        onReceiveCommand(command);
    }
    
    @ServiceThreadOnly
    private void onReceiveCommand(HdmiCecMessage message) {
        assertRunOnServiceThread();
        //调用HdmiControlService的handleCecCommand处理消息
        if (isAcceptableAddress(message.getDestination()) && mService.handleCecCommand(message)) {
            return;
        }
        // Not handled message, so we will reply it with <Feature Abort>.
        maySendFeatureAbortCommand(message, Constants.ABORT_UNRECOGNIZED_OPCODE);
    }
```

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiControlService.java
@ServiceThreadOnly
    boolean handleCecCommand(HdmiCecMessage message) {
        assertRunOnServiceThread();
        int errorCode = mMessageValidator.isValid(message);
        if (errorCode != HdmiCecMessageValidator.OK) {
            // We'll not response on the messages with the invalid source or destination
            // or with parameter length shorter than specified in the standard.
            if (errorCode == HdmiCecMessageValidator.ERROR_PARAMETER) {
                maySendFeatureAbortCommand(message, Constants.ABORT_INVALID_OPERAND);
            }
            return true;
        }
        //传到localDevice中处理
        if (dispatchMessageToLocalDevice(message)) {
            return true;
        }

        return (!mAddressAllocated) ? mCecMessageBuffer.bufferMessage(message) : false;
    }
    
    @ServiceThreadOnly
    private boolean dispatchMessageToLocalDevice(HdmiCecMessage message) {
        assertRunOnServiceThread();
        for (HdmiCecLocalDevice device : mCecController.getLocalDeviceList()) {
            if (device.dispatchMessage(message)
                    && message.getDestination() != Constants.ADDR_BROADCAST) {
                return true;
            }
        }

        if (message.getDestination() != Constants.ADDR_BROADCAST) {
            HdmiLogger.warning("Unhandled cec command:" + message);
        }
        return false;
    }
```

```cpp
//frameworks/base/services/core/java/com/android/server/hdmi/HdmiCecLocalDevice.java
    @ServiceThreadOnly
    boolean dispatchMessage(HdmiCecMessage message) {
        assertRunOnServiceThread();
        int dest = message.getDestination();
        if (dest != mAddress && dest != Constants.ADDR_BROADCAST) {
            return false;
        }
        mCecMessageCache.cacheMessage(message);
        return onMessage(message);
    }

    @ServiceThreadOnly
    protected final boolean onMessage(HdmiCecMessage message) {
        assertRunOnServiceThread();
        if (dispatchMessageToAction(message)) {
            return true;
        }
        switch (message.getOpcode()) {
            ...
            case Constants.MESSAGE_STANDBY:
                //调用handleStandby
                return handleStandby(message);
            ...
            default:
                return false;
        }
    }
    
    @ServiceThreadOnly
    protected boolean handleStandby(HdmiCecMessage message) {
        assertRunOnServiceThread();
        // Seq #12
        if (mService.isControlEnabled()
                && !mService.isProhibitMode()
                && mService.isPowerOnOrTransient()) {
            //调用服务的standby方法
            mService.standby();
            return true;
        }
        return false;
    }

    //frameworks/base/services/core/java/com/android/server/hdmi/HdmiControlService.java
    @ServiceThreadOnly
    void standby() {
        assertRunOnServiceThread();
        if (!canGoToStandby()) {
            return;
        }
        mStandbyMessageReceived = true;
        //最终到PowerManager的goToSleep进入休眠
        mPowerManager.goToSleep(SystemClock.uptimeMillis(), PowerManager.GO_TO_SLEEP_REASON_HDMI, 0);
        // PowerManger will send the broadcast Intent.ACTION_SCREEN_OFF and after this gets
        // the intent, the sequence will continue at onStandby().
    }
```

至此，完成了从底层到framework层一键休眠的分析流程。

## 4\. 总结

从休眠的通路，可以清晰了解到整个HdmiControlService是如何工作，信息是如何从两个不同方向进行传输。后续如果有扩展，也可能基于该框架进行修改，也可以设计开关控制通路。

## 参考文献

[Android源码](https://cs.android.com/)  
[HDMI-CEC 控制服务](https://source.android.com/devices/tv/hdmi-cec)