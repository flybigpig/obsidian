> hongxi.zhu 2023-7-12  
> Android T

从前面inputflinger的启动分析中，我们知道事件来源是在`EventHub::getEvents`, 所以我们重点看下这个方法的流程来了解事件是如何从驱动上报中获取的。

#### EventHub::getEvents

frameworks/native/services/inputflinger/reader/EventHub.cpp

```cpp
size_t EventHub::getEvents(int timeoutMillis, RawEvent* buffer, size_t bufferSize) {
    ALOG_ASSERT(bufferSize >= 1);

    std::scoped_lock _l(mLock);
    //创建一个input_event数组，用于存放从epoll中读取到的input_events
    struct input_event readBuffer[bufferSize];
    //buffer是inputReader传入的RawEvent数组首地址，数组大小为256，将事件构造成RawEvent并装入后返回给inputReader
    //用这里把数组地址赋给event指针，后续使用这个指针操作这个数组
    RawEvent* event = buffer; //传入的RawEvent数组首地址
    //一次处理事件的最大容量为256个
    size_t capacity = bufferSize;
    bool awoken = false;
    for (;;) {
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);  //获取当前时间戳

		//处理有关设备状态变化的逻辑
        // Reopen input devices if needed.
        if (mNeedToReopenDevices) {
            mNeedToReopenDevices = false;

            ALOGI("Reopening all input devices due to a configuration change.");

            closeAllDevicesLocked();
            mNeedToScanDevices = true;
            break; // return to the caller before we actually rescan
        }

        // Report any devices that had last been added/removed.
        //当调用closeDeviceLocked时，就会把需要关闭的设备加入mClosingDevices，下一次循环到这里时就遍历这个列表处理
        for (auto it = mClosingDevices.begin(); it != mClosingDevices.end();) {
        	//移除一个设备就构建一个DEVICE_REMOVED类型的event并加入RawEvent数组中
            std::unique_ptr<Device> device = std::move(*it);
            ALOGV("Reporting device closed: id=%d, name=%s\n", device->id, device->path.c_str());
            event->when = now;
            event->deviceId = (device->id == mBuiltInKeyboardId)
                    ? ReservedInputDeviceId::BUILT_IN_KEYBOARD_ID
                    : device->id;
            event->type = DEVICE_REMOVED;
            event += 1;
            it = mClosingDevices.erase(it);  //从mClosingDevices中移除device
            mNeedToSendFinishedDeviceScan = true;
            if (--capacity == 0) {
                break;
            }
        }

		//当EventHub初始化时，mNeedToScanDevices = true， 所以首次进入需要scan输入设备
        if (mNeedToScanDevices) {
            mNeedToScanDevices = false;
            scanDevicesLocked();  //扫描设备"/dev/input"下的设备，例如event1、event2，这个方法很复杂，
            mNeedToSendFinishedDeviceScan = true;
        }
		//上一步进行了scan device的操作，现在mOpeningDevices是记录着获取到的Device
        while (!mOpeningDevices.empty()) {
        	//遍历取出mOpeningDevices中Device，构建RawEvent->DEVICE_ADDED事件，写入event缓冲区中
            std::unique_ptr<Device> device = std::move(*mOpeningDevices.rbegin());
            mOpeningDevices.pop_back();//把这个device对象从移除mOpeningDevices中
            ALOGV("Reporting device opened: id=%d, name=%s\n", device->id, device->path.c_str());
            //构建一个RawEvent时间，type = DEVICE_ADDED
            event->when = now;
            event->deviceId = device->id == mBuiltInKeyboardId ? 0 : device->id;
            event->type = DEVICE_ADDED;
            event += 1; //RawEvent对象偏移 + 1（RawEvent数组中RawEvent数量）
		
			...
			//从已经处理的设备中mOpeningDevices中的device加入mDevices Map中，以device->id标记
            auto [dev_it, inserted] = mDevices.insert_or_assign(device->id, std::move(device));

            mNeedToSendFinishedDeviceScan = true;  //标记扫描完成，可以退出扫描状态（退出也要发退出事件）
            //如果RawEvent数组装满了，就跳出循环往下执行（需要等数组中数据分发释放后进入这里再处理）
            if (--capacity == 0) {  
                break;
            }
        }
		//如果扫描结束需要发一个mNeedToSendFinishedDeviceScan事件，将这个事件构造并写入event（RawEvent）数组中
        if (mNeedToSendFinishedDeviceScan) {
            mNeedToSendFinishedDeviceScan = false;
            event->when = now;
            event->type = FINISHED_DEVICE_SCAN;
            event += 1;  //RawEvent对象偏移 + 1（RawEvent数组中RawEvent数量）
            if (--capacity == 0) {
                break;
            }
        }

        // Grab the next input event.
        //从epoll中下一个输入事件
        bool deviceChanged = false;  //这个变量标记当前设备是否有变化（拔插、配置改变等）
        //mPendingEventCount指epoll中返回的事件（在epoll event数组中）的数量
        //mPendingEventIndex指要处理的epoll事件在epoll返回列表中的下标
        //循环处理epoll返回列表中的epoll事件
        while (mPendingEventIndex < mPendingEventCount) {
            const struct epoll_event& eventItem = mPendingEventItems[mPendingEventIndex++];
            if (eventItem.data.fd == mINotifyFd) {
                if (eventItem.events & EPOLLIN) {
                    mPendingINotify = true;
                } else {
                    ALOGW("Received unexpected epoll event 0x%08x for INotify.", eventItem.events);
                }
                continue;
            }

            if (eventItem.data.fd == mWakeReadPipeFd) {
                if (eventItem.events & EPOLLIN) {
                    ALOGV("awoken after wake()");
                    awoken = true;
                    char wakeReadBuffer[16];
                    ssize_t nRead;
                    do {
                        nRead = read(mWakeReadPipeFd, wakeReadBuffer, sizeof(wakeReadBuffer));
                    } while ((nRead == -1 && errno == EINTR) || nRead == sizeof(wakeReadBuffer));
                } else {
                    ALOGW("Received unexpected epoll event 0x%08x for wake read pipe.",
                          eventItem.events);
                }
                continue;
            }

            //如果非mINotifyFd和非mWakeReadPipeFd，则是底层输入驱动上报的输入事件，那么通过fd获取这个事件对应的Device
            Device* device = getDeviceByFdLocked(eventItem.data.fd);
            
 			...
            // This must be an input event
            //如果是个epoll读事件
            if (eventItem.events & EPOLLIN) {
            	//通过read方法获取读缓冲区大小和数据。写入readBuffer，读取size为256个input_event
                int32_t readSize =
                        read(device->fd, readBuffer, sizeof(struct input_event) * capacity);
                        
                if (readSize == 0 || (readSize < 0 && errno == ENODEV)) {
                    // Device was removed before INotify noticed.
                    //如果读取的size <= 0 且返回异常可能是设备已经被移除了，只是INotify还没通知，
                    //那么就标记这个设备状态改变，并移除这个设备
                    deviceChanged = true;  //标记这个设备状态改变
                    closeDeviceLocked(*device);  //移除这个设备
                } else if (readSize < 0) {
					...
                } else if ((readSize % sizeof(struct input_event)) != 0) {
					...
                } else { //正常读到数据了
                	//(特殊)如果读到的device是内置键盘，name就设置它的device->id = 0
                    int32_t deviceId = device->id == mBuiltInKeyboardId ? 0 : device->id;
					//计算这次读到的epoll读事件中的readBuffer中包含的input_event数量
                    size_t count = size_t(readSize) / sizeof(struct input_event);
                    //从readBuffer循环取出读到的的input_event对象
                    //构造RawEvent对象写入RawEvent数组中，指针依次往后偏移
                    for (size_t i = 0; i < count; i++) {
                        struct input_event& iev = readBuffer[i];
                        event->when = processEventTimestamp(iev);
                        event->readTime = systemTime(SYSTEM_TIME_MONOTONIC);
                        event->deviceId = deviceId;
                        event->type = iev.type;
                        event->code = iev.code;
                        event->value = iev.value;
                        event += 1;
                        capacity -= 1;
                    }
                    //如果写满了RawEvent数组
                    if (capacity == 0) {
                        // The result buffer is full.  Reset the pending event index
                        // so we will try to read the device again on the next iteration.
                        // 如果RawEvent数组写满了，就把mPendingEventIndex - 1，(因为下次循环开始会加一，提前减一这样处理的就还是当前这个epoll事件)
                        // 说明我们本次epoll读事件我们没有处理完，下一个循环还要继续处理这个epoll事件
                        mPendingEventIndex -= 1;
                        break;
                    }
                }
            } else if (eventItem.events & EPOLLHUP) {  //如果是hang-up事件说明设备拔出，就移除这个设备，通知设备状态变化
                ALOGI("Removing device %s due to epoll hang-up event.",
                      device->identifier.name.c_str());
                deviceChanged = true;
                closeDeviceLocked(*device);
            } else {  //收到异常的epoll事件，不处理
                ALOGW("Received unexpected epoll event 0x%08x for device %s.", eventItem.events,
                      device->identifier.name.c_str());
            }
        }

        // readNotify() will modify the list of devices so this must be done after
        // processing all other events to ensure that we read all remaining events
        // before closing the devices.
        //当处理完一次一次epoll_wait返回列表中所有epoll事件后，检测下是否有底层设备变化（mPendingINotify = true）
        //如果有就通知设备状态改变
        if (mPendingINotify && mPendingEventIndex >= mPendingEventCount) {
            mPendingINotify = false;
            readNotifyLocked();  //通过read去读取INotify fd返回的事件，判断设备状态，是需要重新获取设备还是移除设备
            deviceChanged = true;  //标记设备状态改变，
        }

        // Report added or removed devices immediately.
        // 如果有设备状态改变（新增或者移除）需要马上到下一个循环处理
        if (deviceChanged) {event
            continue;
        }

        // Return now if we have collected any events or if we were explicitly awoken.
        //1.如果其他地方调用了`mEventHub->wake()`则会唤醒阻塞在epoll_wait()中的inputReader线程，下一次循环时然后从这里跳出getEvents方法，往下执行loopOnce，处理输入事件
        //2. 或者RawEvent数组中有数据则跳出getEvents方法，往下执行loopOnce，处理输入事件
        if (event != buffer || awoken) {
            break;
        }

		//如果RawEvent数组为空且没有inputReader线程没有被外部唤醒，则下面就准备开始获取下一次epoll事件(进入阻塞等待)
        mPendingEventIndex = 0; //准备进入下一次事件接收前，重置mPendingEventIndex下标

        mLock.unlock(); // release lock before poll
		//进入epoll_wait阻塞等待驱动上报事件
        int pollResult = epoll_wait(mEpollFd, mPendingEventItems, EPOLL_MAX_EVENTS, timeoutMillis);
		//从epoll_wait中唤醒
		//也许是外部调用mEventHub->wake()唤醒
		//或者内核通知事件上报唤醒
		//或者是超时退出休眠
		
        mLock.lock(); // reacquire lock after poll

        if (pollResult == 0) {
            // Timed out.
            // 超时退出的情况
            mPendingEventCount = 0;
            break;
        }

        if (pollResult < 0) {
            // An error occurred.
            mPendingEventCount = 0;

            // Sleep after errors to avoid locking up the system.
            // Hopefully the error is transient.
            if (errno != EINTR) {
                ALOGW("poll failed (errno=%d)\n", errno);
                usleep(100000);
            }
        } else {
            // Some events occurred.
            // 获取到epoll事件，将事件数量赋给mPendingEventCount
            mPendingEventCount = size_t(pollResult);
        }
    }

    // All done, return the number of events we read.
    // 处理结束，退出循环将事件返回到inputReader的loopOnce中处理
    return event - buffer;
}
```

#### EventHub::scanDevicesLocked()

```cpp
void EventHub::scanDevicesLocked() {
    status_t result;
    std::error_code errorCode;

    if (std::filesystem::exists(DEVICE_INPUT_PATH, errorCode)) {
        result = scanDirLocked(DEVICE_INPUT_PATH);
        
    } else {
		...
    }
	...
}

status_t EventHub::scanDirLocked(const std::string& dirname) {
	//遍历 /dev/input/event* 路径，打开这些设备并获取相关设备信息
    for (const auto& entry : std::filesystem::directory_iterator(dirname)) {
        openDeviceLocked(entry.path());
    }
    return 0;
}
```

#### EventHub::openDeviceLocked

这个方法很长，主要作用就是打开`/dev/input/eventX`设备节点，用返回的fd通过`ioctl`向驱动获取输入设备`device`相关信息。

```cpp
void EventHub::openDeviceLocked(const std::string& devicePath) {
	//如果目标路径是当前已存在的设备（之前扫描过的设备）的，就不再扫描这个路径了，避免出现重复设备
    for (const auto& [deviceId, device] : mDevices) {
        if (device->path == devicePath) {
            return; // device was already registered
        }
    }

    char buffer[80];

    ALOGV("Opening device: %s", devicePath.c_str());
	//通过open打开设备节点，返回该设备节点的fd
    int fd = open(devicePath.c_str(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
    
    InputDeviceIdentifier identifier;  //一个硬件设备的结构体在用户空间中描述, 包括name、vendor、product、descriptor等

    // Get device name.
    //获取设备 device name
    if (ioctl(fd, EVIOCGNAME(sizeof(buffer) - 1), &buffer) < 1) {
        ALOGE("Could not get device name for %s: %s", devicePath.c_str(), strerror(errno));
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        identifier.name = buffer;
    }

    // Check to see if the device is on our excluded list
    //通过device name检测下这个设备是不是在排除名单，如果是就忽略这个设备
    for (size_t i = 0; i < mExcludedDevices.size(); i++) {
        const std::string& item = mExcludedDevices[i];
        if (identifier.name == item) {
            ALOGI("ignoring event id %s driver %s\n", devicePath.c_str(), item.c_str());
            close(fd);
            return;
        }
    }

    // Get device driver version.
    //获取设备驱动版本
    int driverVersion;
    if (ioctl(fd, EVIOCGVERSION, &driverVersion)) {
    }

    // Get device identifier.
    //获取设备的identifier，是设备在内核空间的描述
    //内核描述为input_id结构体，内容为：bustype、product、product、version
    struct input_id inputId;
    if (ioctl(fd, EVIOCGID, &inputId)) {
    }
    identifier.bus = inputId.bustype;
    identifier.product = inputId.product;
    identifier.vendor = inputId.product;
    identifier.version = inputId.version;

    // Get device physical location.
    //获取设备的物理位置（物理拓扑中的位置）
    if (ioctl(fd, EVIOCGPHYS(sizeof(buffer) - 1), &buffer) < 1) {
        // fprintf(stderr, "could not get location for %s, %s\n", devicePath, strerror(errno));
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        identifier.location = buffer;
    }

    // Get device unique id.
    //获取设备的unique id（一般的设备这个字段都是没有的，为空）
    if (ioctl(fd, EVIOCGUNIQ(sizeof(buffer) - 1), &buffer) < 1) {
        // fprintf(stderr, "could not get idstring for %s, %s\n", devicePath, strerror(errno));
    } else {
        buffer[sizeof(buffer) - 1] = '\0';
        identifier.uniqueId = buffer;
    }

    // Fill in the descriptor.
    // 获取设备的descriptor，这个字段很重要，它是用于标识这个设备的标识符，无论重启、拔插、升级都不会变
    //根据unique_id、vendor_id、product_id、随机数组合后sha1转化生成，赋值给identifier.descriptor
    assignDescriptorLocked(identifier);

    // Allocate device.  (The device object takes ownership of the fd at this point.)
    //创建Device结构体，用于描述当前从驱动获取到的这个输入设备，将前面获取的设备fd、设备节点路径devicePath、设备硬件描述identifier赋给这个Device,
    //同时还有deviceId，这个id并不是驱动传上来的，而是我们每次通过ioctl获取到新设备时计数 + 1
    int32_t deviceId = mNextDeviceId++;
    std::unique_ptr<Device> device = std::make_unique<Device>(fd, deviceId, devicePath, identifier);

	//我们可以通过这个打印或者dumpsys input获取设备的信息
    ALOGV("add device %d: %s\n", deviceId, devicePath.c_str());
    ALOGV("  bus:        %04x\n"
          "  vendor      %04x\n"
          "  product     %04x\n"
          "  version     %04x\n",
          identifier.bus, identifier.vendor, identifier.product, identifier.version);
    ALOGV("  name:       \"%s\"\n", identifier.name.c_str());
    ALOGV("  location:   \"%s\"\n", identifier.location.c_str());
    ALOGV("  unique id:  \"%s\"\n", identifier.uniqueId.c_str());
    ALOGV("  descriptor: \"%s\"\n", identifier.descriptor.c_str());
    ALOGV("  driver:     v%d.%d.%d\n", driverVersion >> 16, (driverVersion >> 8) & 0xff,
          driverVersion & 0xff);

    // Load the configuration file for the device.
    //为当前获取到的设备加载`.idc`配置文件，格式一般是：/vendor/usr/idc/Vendor_XXXX_Product_XXXX_Version_XXXX.idc
    //通过product/vendor/version来检索主要路径下符合条件的`idc`文件
    //解析该文件后保存在device对象的configuration变量中
    device->loadConfigurationLocked();

	// 针对带电池，有LED灯的输入设备，需要设备associatedDevice来关联它的额外能力
    bool hasBattery = false;
    bool hasLights = false;
    // Check the sysfs root path
    std::optional<std::filesystem::path> sysfsRootPath = getSysfsRootPath(devicePath.c_str());
    if (sysfsRootPath.has_value()) {
        std::shared_ptr<AssociatedDevice> associatedDevice;
        for (const auto& [id, dev] : mDevices) {
            if (device->identifier.descriptor == dev->identifier.descriptor &&
                !dev->associatedDevice) {
                associatedDevice = dev->associatedDevice;
            }
        }
        if (!associatedDevice) {
            associatedDevice = std::make_shared<AssociatedDevice>(sysfsRootPath.value());
        }
        hasBattery = associatedDevice->configureBatteryLocked();
        hasLights = associatedDevice->configureLightsLocked();

        device->associatedDevice = associatedDevice;
    }

	//向ioctl驱动查询这个设备会上报那种类型的事件，每个类型都问一下支不支持，有点...
	//设备会上报哪一种事件，对应的XXBitmask就会有对应的值，用于判断它是什么类型的设备
    // Figure out the kinds of events the device reports.
    device->readDeviceBitMask(EVIOCGBIT(EV_KEY, 0), device->keyBitmask);
    device->readDeviceBitMask(EVIOCGBIT(EV_ABS, 0), device->absBitmask);
    device->readDeviceBitMask(EVIOCGBIT(EV_REL, 0), device->relBitmask);
    device->readDeviceBitMask(EVIOCGBIT(EV_SW, 0), device->swBitmask);
    device->readDeviceBitMask(EVIOCGBIT(EV_LED, 0), device->ledBitmask);
    device->readDeviceBitMask(EVIOCGBIT(EV_FF, 0), device->ffBitmask);
    device->readDeviceBitMask(EVIOCGBIT(EV_MSC, 0), device->mscBitmask);
    device->readDeviceBitMask(EVIOCGPROP(0), device->propBitmask);

    // See if this is a keyboard.  Ignore everything in the button range except for
    // joystick and gamepad buttons which are handled like keyboards for the most part.
    // 判断是否是键盘、游戏手柄等
    bool haveKeyboardKeys =
            device->keyBitmask.any(0, BTN_MISC) || device->keyBitmask.any(BTN_WHEEL, KEY_MAX + 1);
    bool haveGamepadButtons = device->keyBitmask.any(BTN_MISC, BTN_MOUSE) ||
            device->keyBitmask.any(BTN_JOYSTICK, BTN_DIGI);
    if (haveKeyboardKeys || haveGamepadButtons) {
        device->classes |= InputDeviceClass::KEYBOARD;
    }

    // See if this is a cursor device such as a trackball or mouse.
    //判断设备是不是鼠标或者轨迹球类型
    if (device->keyBitmask.test(BTN_MOUSE) && device->relBitmask.test(REL_X) &&
        device->relBitmask.test(REL_Y)) {
        device->classes |= InputDeviceClass::CURSOR;
    }

    // See if this is a rotary encoder type device.
    //判断设备是不是旋转编码器类型（旋钮）
    String8 deviceType = String8();
    if (device->configuration &&
        device->configuration->tryGetProperty(String8("device.type"), deviceType)) {
        if (!deviceType.compare(String8("rotaryEncoder"))) {
            device->classes |= InputDeviceClass::ROTARY_ENCODER;
        }
    }

    // See if this is a touch pad.
    // Is this a new modern multi-touch driver?
    //是不是触摸板，支不支持多点触摸
    if (device->absBitmask.test(ABS_MT_POSITION_X) && device->absBitmask.test(ABS_MT_POSITION_Y)) {
        // Some joysticks such as the PS3 controller report axes that conflict
        // with the ABS_MT range.  Try to confirm that the device really is
        // a touch screen.
        if (device->keyBitmask.test(BTN_TOUCH) || !haveGamepadButtons) {
            device->classes |= (InputDeviceClass::TOUCH | InputDeviceClass::TOUCH_MT);
        }
        // Is this an old style single-touch driver?
        //是不是老版的单点触摸驱动
    } else if (device->keyBitmask.test(BTN_TOUCH) && device->absBitmask.test(ABS_X) &&
               device->absBitmask.test(ABS_Y)) {
        device->classes |= InputDeviceClass::TOUCH;
        // Is this a BT stylus?
        //是不是蓝牙手写笔
    } else if ((device->absBitmask.test(ABS_PRESSURE) || device->keyBitmask.test(BTN_TOUCH)) &&
               !device->absBitmask.test(ABS_X) && !device->absBitmask.test(ABS_Y)) {
        device->classes |= InputDeviceClass::EXTERNAL_STYLUS;
        // Keyboard will try to claim some of the buttons but we really want to reserve those so we
        // can fuse it with the touch screen data, so just take them back. Note this means an
        // external stylus cannot also be a keyboard device.
        //外部手写笔不能同时是键盘设备
        device->classes &= ~InputDeviceClass::KEYBOARD;
    }

    // See if this device is a joystick.
    // Assumes that joysticks always have gamepad buttons in order to distinguish them
    // from other devices such as accelerometers that also have absolute axes.
    //是不是操作杆
    if (haveGamepadButtons) {
        auto assumedClasses = device->classes | InputDeviceClass::JOYSTICK;
        for (int i = 0; i <= ABS_MAX; i++) {
            if (device->absBitmask.test(i) &&
                (getAbsAxisUsage(i, assumedClasses).test(InputDeviceClass::JOYSTICK))) {
                device->classes = assumedClasses;
                break;
            }
        }
    }

    // Check whether this device is an accelerometer.
    //是不是加速计
    if (device->propBitmask.test(INPUT_PROP_ACCELEROMETER)) {
        device->classes |= InputDeviceClass::SENSOR;
    }

    // Check whether this device has switches.
    //是不是有开关
    for (int i = 0; i <= SW_MAX; i++) {
        if (device->swBitmask.test(i)) {
            device->classes |= InputDeviceClass::SWITCH;
            break;
        }
    }

    // Check whether this device supports the vibrator.
    //支不支持震动
    if (device->ffBitmask.test(FF_RUMBLE)) {
        device->classes |= InputDeviceClass::VIBRATOR;
    }

    // Configure virtual keys.
    //虚拟按键（类似于老手机上面三个虚拟按键back、home、recent）
    if ((device->classes.test(InputDeviceClass::TOUCH))) {
        // Load the virtual keys for the touch screen, if any.
        // We do this now so that we can make sure to load the keymap if necessary.
        bool success = device->loadVirtualKeyMapLocked();
        if (success) {
            device->classes |= InputDeviceClass::KEYBOARD;
        }
    }

    // Load the key map.
    // We need to do this for joysticks too because the key layout may specify axes, and for
    // sensor as well because the key layout may specify the axes to sensor data mapping.
    //如果设备是键盘等设备就加载按键映射配置文件，
    //包括kl(keyLayout)文件、kcm(KeyCharacterMap)文件
    status_t keyMapStatus = NAME_NOT_FOUND;
    if (device->classes.any(InputDeviceClass::KEYBOARD | InputDeviceClass::JOYSTICK |
                            InputDeviceClass::SENSOR)) {
        // Load the keymap for the device.
        keyMapStatus = device->loadKeyMapLocked();  //加载过程和idc文件类似
    }

    // Configure the keyboard, gamepad or virtual keyboard.
    //配置键盘、手柄、虚拟键盘相关
    if (device->classes.test(InputDeviceClass::KEYBOARD)) {
        // Register the keyboard as a built-in keyboard if it is eligible.
        //如果符合内置键盘的条件就把这个device指定为内置键盘
        if (!keyMapStatus && mBuiltInKeyboardId == NO_BUILT_IN_KEYBOARD &&
            isEligibleBuiltInKeyboard(device->identifier, device->configuration.get(),
                                      &device->keyMap)) {
            mBuiltInKeyboardId = device->id;
        }

        // 'Q' key support = cheap test of whether this is an alpha-capable kbd
        //如果有Q键就说明是一个标准全功能键盘，设置为ALPHAKEY类型
        if (device->hasKeycodeLocked(AKEYCODE_Q)) {
            device->classes |= InputDeviceClass::ALPHAKEY;
        }

        // See if this device has a DPAD.
        //如果有方向键
        if (device->hasKeycodeLocked(AKEYCODE_DPAD_UP) &&
            device->hasKeycodeLocked(AKEYCODE_DPAD_DOWN) &&
            device->hasKeycodeLocked(AKEYCODE_DPAD_LEFT) &&
            device->hasKeycodeLocked(AKEYCODE_DPAD_RIGHT) &&
            device->hasKeycodeLocked(AKEYCODE_DPAD_CENTER)) {
            device->classes |= InputDeviceClass::DPAD;
        }

        // See if this device has a gamepad.
        //如果是游戏手柄
        for (size_t i = 0; i < sizeof(GAMEPAD_KEYCODES) / sizeof(GAMEPAD_KEYCODES[0]); i++) {
            if (device->hasKeycodeLocked(GAMEPAD_KEYCODES[i])) {
                device->classes |= InputDeviceClass::GAMEPAD;
                break;
            }
        }
    }

    // If the device isn't recognized as something we handle, don't monitor it.
    //如果这个设备上面的类型都不符合，那么这个设备就是个屑，不用往下处理了，忽略，直接返回
    if (device->classes == ftl::Flags<InputDeviceClass>(0)) {
        ALOGV("Dropping device: id=%d, path='%s', name='%s'", deviceId, devicePath.c_str(),
              device->identifier.name.c_str());
        return;
    }

    // Classify InputDeviceClass::BATTERY.
    //如果这个设备还有关联的电池
    if (hasBattery) {
        device->classes |= InputDeviceClass::BATTERY;
    }

    // Classify InputDeviceClass::LIGHT.
    //如果这个设备还有先进的LED灯
    if (hasLights) {
        device->classes |= InputDeviceClass::LIGHT;
    }

    // Determine whether the device has a mic.
    //如果这个设备还有麦克风
    if (device->deviceHasMicLocked()) {
        device->classes |= InputDeviceClass::MIC;
    }

    // Determine whether the device is external or internal.
    //从设备的idc文件中获取这个设备是内部设备还是外接设备，这个会影响一些优先级等，比如多屏
    if (device->isExternalDeviceLocked()) {
        device->classes |= InputDeviceClass::EXTERNAL;
    }

	//游戏手柄和操作杆有时会有多个controller
    if (device->classes.any(InputDeviceClass::JOYSTICK | InputDeviceClass::DPAD) &&
        device->classes.test(InputDeviceClass::GAMEPAD)) {
        device->controllerNumber = getNextControllerNumberLocked(device->identifier.name);
        device->setLedForControllerLocked();
    }

	//将这个设备的fd加入epoll的监听
    if (registerDeviceForEpollLocked(*device) != OK) {
        return;
    }
	//使用ioctl设置fd参数，例如按键重复、挂起块和时钟类型
    device->configureFd();

    ALOGI("New device: id=%d, fd=%d, path='%s', name='%s', classes=%s, "
          "configuration='%s', keyLayout='%s', keyCharacterMap='%s', builtinKeyboard=%s, ",
          deviceId, fd, devicePath.c_str(), device->identifier.name.c_str(),
          device->classes.string().c_str(), device->configurationFile.c_str(),
          device->keyMap.keyLayoutFile.c_str(), device->keyMap.keyCharacterMapFile.c_str(),
          toString(mBuiltInKeyboardId == deviceId));

	//到这里从驱动获取到的这个设备已经完成各种初始化和配置，是时候加到mOpeningDevices中了，继续下一个循环，最后读取所有的设备
    addDeviceLocked(std::move(device));
}
```

这个方法很长，总的概括来看就做了几件事情：

1.  打开设备节点，从设备驱动中获取设备的各种描述信息并构造出Device对象
2.  根据设备的信息加载这个设备的`idc`文件
3.  向驱动查询这个设备支持的事件类型
4.  判断设备的类型，设置相关的属性到`device->classes`中
5.  如果是键盘灯设备还需要加载设备对应的`kl`文件和`kcm`文件，
6.  将这个`设备的fd`加入`epoll`的监听中
7.  通过`ioctl`设置`fd参数`，例如`按键重复、挂起块和时钟类型`
8.  最后再把这个设备加到`mOpeningDevices`中管理