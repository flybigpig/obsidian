# Android Camera2详解

原创 雪月青 [雪月清的随笔](javascript:void(0);) *2020-03-11*

收录于话题#Android Camera15个内容

Camera API2是Google从Android5.0开始推出的配合HAL3使用的一套新架构，相比于API1，对应用层开发者而言开放了更多的自主控制权，主要特性包括：

- 可以获取更多的帧(预览/拍照)信息以及手动控制每一帧的参数；
- 对Camera的控制更加精细(比如支持调整focus distance，对焦曝光模式等)；
- 支持更多图片格式(yuv/raw)；
- 高速连拍

当然，就像硬币总是存在正反两面，Camera2架构在让我们获得更多控制权的同时也增加了使用的复杂度.



**基本架构**

![图片](https://mmbiz.qpic.cn/mmbiz_png/PL4o2qj5jFLGPibQjkma6IVn6IHC2zga1LR9kfrRD2ko1QoUAFPHVunMZM0SyePUZXPVlg9HsnNwzXVGP2E4dibQ/640?wx_fmt=png&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

Android设备和Camera是通过管道pipeline的概念将两者进行串联的，在一个会话Session过程中系统发送Request，摄像头返回MetaData进行来回交互；预览和拍照等数据的传递是通过Surface进行.



**Camera2主要类**

![图片](https://mmbiz.qpic.cn/mmbiz_png/PL4o2qj5jFLGPibQjkma6IVn6IHC2zga1W6MAQk9SBjyLIng6eZSRMvpVq6pmrAVxwBaxHusUH0rPA3m2PxKV5Q/640?wx_fmt=png&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

CameraManager：

相机管理类，用于打开，关闭摄像头和获取相机属性描述信息，通过

*getSystemService(Context.CAMERA_SERVICE)*获取实例；

CameraDevice：具体的相机实例，用于建立会话；

CameraCaptureSession：用于向相机发送获取图像的请求

CameraMetaData：相机属性描述的基类；

CameraCharacteristics：相机静态属性描述类，获取它管理的属性是不依赖于摄像头打开的。比如闪光灯支持的模式，预览、拍照支持的size列表等；

CaptureRequest和CaptureResult：两者是在Camera会话期间使用，系统发送CaptureRequest，摄像头返回CaptureResult



**基本使用流程**

**启动预览**

![图片](https://mmbiz.qpic.cn/mmbiz_png/PL4o2qj5jFLGPibQjkma6IVn6IHC2zga1ibuUsrlHsAa0KYFVOZzPWAQeK892b2X4HWtMn5znKfOwfE93SuVwOMQ/640?wx_fmt=png&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

启动预览需要3个步骤，每一步都有StateCallback，在相应的callback中开启下一步。流程梳理起来比较简洁，但是实际用java编写代码的时候，callback的嵌套就让流程看起来不那么友好了...

此处吹一波kotlin的协程

![图片](https://res.wx.qq.com/mpres/htmledition/images/icon/common/emotion_panel/emoji_wx/Yellowdog.png?tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

```

lifecycleScope.launch(Dispatchers.Main) {
    // 1. Open the selected camera
    camera = openCamera(cameraManager, args.cameraId, cameraHandler)

    // 2. Create session
    // 显示预览可以使用GLSurfaceView, SurfaceView或者TextureView
    // 此处viewFinder使用SurfaceView
    // Creates list of Surfaces where the camera will output frames
    val targets = listOf(viewFinder.holder.surface, imageReader.surface)

    // Start a capture session using our open camera and list of Surfaces where frames will go
    session = createCaptureSession(camera, targets, cameraHandler)

    // 3. Start preview
    val captureRequest = camera.createCaptureRequest(
               CameraDevice.TEMPLATE_PREVIEW).apply { addTarget(viewFinder.holder.surface) }

     // This will keep sending the capture request as frequently as possible until the
     // session is torn down or session.stopRepeating() is called
     session.setRepeatingRequest(captureRequest.build(), null, cameraHandler)
}
```

**拍照**

存在拍照需求时，在创建Session的时候需要提前配置用于拍照的Surface

```
// Initialize an image reader which will be used to capture still photos
val size = characteristics.get(
            CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)!!
             .getOutputSizes(args.pixelFormat).maxBy { it.height * it.width }!!
imageReader = ImageReader.newInstance(
             size.width, size.height, args.pixelFormat, IMAGE_BUFFER_SIZE)

// Creates list of Surfaces where the camera will output frames
val targets = listOf(viewFinder.holder.surface, imageReader.surface)
```

点击拍照按钮进行拍照时，向Session提交一次携带了拍照Surface的Request，

照片数据可在OnImageAvailableListener回调获取



```

imageReader.setOnImageAvailableListener({ reader ->
            val image = reader.acquireNextImage()
            Log.d(TAG, "Image available in queue: ${image.timestamp}")
        }, imageReaderHandler)
session.capture(...)
```

**获取预览数据**

在Camera API1中，预览数据是直接通过byte[]的形式返回给开发者的。Camera2中要获取预览数据则需要额外配置一下。

首先需要通过ImageReader创建的Surface**，**在创建session的时候配置进去.

比如创建一个获取YUV格式的Surface

```

imageReader = ImageReader.newInstance(
                size.width, size.height, ImageFormat.YUV_420_888, IMAGE_BUFFER_SIZE
      )
```

然后对该imageReader设置数据回调，并在启动预览的Request中将它的Surface添加进去，这样每一帧预览生成时就能通过数据回调获得Image对象，从这个数据包装对象中我们就可以拿到Y，U，V各个通道的数据了。



**总结**

Camera2的基本使用总结到这里，使用起来肯定是不如Camera1方便，不过能让应用层有更多的操作空间总是值得的，而且从Android9.0开始也不得不使用这套架构了

![图片](https://res.wx.qq.com/mpres/htmledition/images/icon/common/emotion_panel/emoji_wx/Watermelon.png?tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

(文章的代码引用自官方的例子Camera2Basic)**
**