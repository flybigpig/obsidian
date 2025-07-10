> hongxi.zhu 2023-7-16  
> Android 13

### 回顾

从前面的InputFlinger的启动可知，InputReader线程启动后会循环执行loopOnce()方法，然后阻塞在`getEvents`等待事件的上报，这里就从loopOnce获取到事件被唤醒后来分析InputReader对事件的处理。

#### InputReader::loopOnce()

frameworks/native/services/inputflinger/reader/InputReader.cpp

```cpp
void InputReader::loopOnce() {
    int32_t oldGeneration;
    int32_t timeoutMillis;
    bool inputDevicesChanged = false;
    std::vector<InputDeviceInfo> inputDevices;
    
	...
	//从EventHub中读取事件
    size_t count = mEventHub->getEvents(timeoutMillis, mEventBuffer, EVENT_BUFFER_SIZE);

    { // acquire lock
		...
        if (count) {
        	//处理事件
            processEventsLocked(mEventBuffer, count);
        }
		...
		
    } // release lock

    // Send out a message that the describes the changed input devices.
    //如果输入设备状态改变则通过回调通知java层
    if (inputDevicesChanged) {
        mPolicy->notifyInputDevicesChanged(inputDevices);
    }

    // Flush queued events out to the listener.
    mQueuedListener.flush();
}
```

### 一、处理事件

#### InputReader::processEventsLocked

```cpp
void InputReader::processEventsLocked(const RawEvent* rawEvents, size_t count) {
    for (const RawEvent* rawEvent = rawEvents; count;) {
        int32_t type = rawEvent->type;
        size_t batchSize = 1;
        //除了设备添加、移除、结束扫描事件之外的普通事件采用批处理方式方式处理
        if (type < EventHubInterface::FIRST_SYNTHETIC_EVENT) {
            int32_t deviceId = rawEvent->deviceId;
            while (batchSize < count) {
            	//如果出现不符合批处理的事件（一般上面的判断已经足够）则退出本次事件流程
                if (rawEvent[batchSize].type >= EventHubInterface::FIRST_SYNTHETIC_EVENT ||
                    rawEvent[batchSize].deviceId != deviceId) {
                    break;
                }
                batchSize += 1;
            }
            //处理普通事件（批量处理）
            processEventsForDeviceLocked(deviceId, rawEvent, batchSize);
        } else {
            switch (rawEvent->type) {
            	//如果是设备添加事件
                case EventHubInterface::DEVICE_ADDED:
                	//添加设备主要就是创建设备对应的inputDevice
                	//通过设备的class类型创建对应的mapper
                	//将对应的mapper和EventHub、inputReader关联（InputDeviceContext）
                	//然后将对应的deviceId、mapper、InputDeviceContext加入inputDevice容器中
                    addDeviceLocked(rawEvent->when, rawEvent->deviceId);
                    break;
                //如果是设备移除事件
                case EventHubInterface::DEVICE_REMOVED:
                    removeDeviceLocked(rawEvent->when, rawEvent->deviceId);
                    break;
                //如果是设备结束扫描事件
                case EventHubInterface::FINISHED_DEVICE_SCAN:
                    handleConfigurationChangedLocked(rawEvent->when);
                    break;
                default:
                    ALOG_ASSERT(false); // can't happen
                    break;
            }
        }
        count -= batchSize;
        rawEvent += batchSize;
    }
}
```

#### InputReader::processEventsForDeviceLocked

```cpp
void InputReader::processEventsForDeviceLocked(int32_t eventHubId, const RawEvent* rawEvents,
                                               size_t count) {
    auto deviceIt = mDevices.find(eventHubId);  //通过device id找到事件对应的inputDevice对象
	...
    device->process(rawEvents, count);  //调用事件对应的inputDevice对象process处理当前事件
}
```

#### InputDevice::process

frameworks/native/services/inputflinger/reader/InputDevice.cpp

```cpp
void InputDevice::process(const RawEvent* rawEvents, size_t count) {
	//按顺序分发给对应的mapper处理，要求事件按照时间顺序处理
    for (const RawEvent* rawEvent = rawEvents; count != 0; rawEvent++) {

        if (mDropUntilNextSync) {  //抛弃下一个SYN事件前的所有事件
			...
        } else if (rawEvent->type == EV_SYN && rawEvent->code == SYN_DROPPED) {
			...
        } else {  //正常走这里
        	//通过deviceId查询mDevices中对应的mapper（这个是在设备add事件时添加的）
            for_each_mapper_in_subdevice(rawEvent->deviceId, [rawEvent](InputMapper& mapper) {
                mapper.process(rawEvent);
            });
        }
        --count;
    }
}
```

#### for\_each\_mapper\_in\_subdevice

frameworks/native/services/inputflinger/reader/include/InputDevice.h

```cpp
    // run a function against every mapper on a specific subdevice
    inline void for_each_mapper_in_subdevice(int32_t eventHubDevice,
                                             std::function<void(InputMapper&)> f) {
        auto deviceIt = mDevices.find(eventHubDevice);
        if (deviceIt != mDevices.end()) {
            auto& devicePair = deviceIt->second;
            auto& mappers = devicePair.second;
            for (auto& mapperPtr : mappers) {
                f(*mapperPtr);
            }
        }
    }
```

通过`rawEvent->deviceId`来查询这个事件对应的设备在`设备add事件`时添加的`mapper`，这个`mapper`是根据`device`的`class类型`来决定的

#### MultiTouchInputMapper::process

在`InputReader::addDeviceLocked`中可以知道触摸现在走的是`MultiTouchInputMapper`  
frameworks/native/services/inputflinger/reader/mapper/MultiTouchInputMapper.cpp

```cpp
void MultiTouchInputMapper::process(const RawEvent* rawEvent) {
    TouchInputMapper::process(rawEvent);
	//处理常规触摸EV_ABS事件，包括多指情况（多个slot）
    mMultiTouchMotionAccumulator.process(rawEvent);
}

void TouchInputMapper::process(const RawEvent* rawEvent) {
    mCursorButtonAccumulator.process(rawEvent);  //如果是光标按键事件，一般不是
    mCursorScrollAccumulator.process(rawEvent);  //如果是光标滚动事件，一般不是
    mTouchButtonAccumulator.process(rawEvent);   //如果是触摸按键事件，（手写笔有时有）

	//只有当收到一次EV_SYN事件上上报本次触摸的事件集
    if (rawEvent->type == EV_SYN && rawEvent->code == SYN_REPORT) {
        sync(rawEvent->when, rawEvent->readTime);
    }
}

void MultiTouchMotionAccumulator::process(const RawEvent* rawEvent) {
    if (rawEvent->type == EV_ABS) {
        bool newSlot = false;
        if (mUsingSlotsProtocol) {  //如果支持多指触摸协议
            if (rawEvent->code == ABS_MT_SLOT) {
                mCurrentSlot = rawEvent->value;
                newSlot = true;
            }
        } else if (mCurrentSlot < 0) {
            mCurrentSlot = 0;
        }

        if (mCurrentSlot < 0 || size_t(mCurrentSlot) >= mSlotCount) {
			...
        } else {
            Slot* slot = &mSlots[mCurrentSlot];
            // If mUsingSlotsProtocol is true, it means the raw pointer has axis info of
            // ABS_MT_TRACKING_ID and ABS_MT_SLOT, so driver should send a valid trackingId while
            // updating the slot.
            //如果支持多指触摸协议，那么驱动就需要上报有效的trackingId来给上层区分
            if (!mUsingSlotsProtocol) {
                slot->mInUse = true;
            }
			//根据事件的code来更新slot的信息
            switch (rawEvent->code) {
                case ABS_MT_POSITION_X:
                    slot->mAbsMTPositionX = rawEvent->value;
                    warnIfNotInUse(*rawEvent, *slot);
                    break;
                case ABS_MT_POSITION_Y:
                    slot->mAbsMTPositionY = rawEvent->value;
                    warnIfNotInUse(*rawEvent, *slot);
                    break;
                case ABS_MT_TOUCH_MAJOR:
                    slot->mAbsMTTouchMajor = rawEvent->value;
                    break;
                case ABS_MT_TOUCH_MINOR:
                    slot->mAbsMTTouchMinor = rawEvent->value;
                    slot->mHaveAbsMTTouchMinor = true;
                    break;
                case ABS_MT_WIDTH_MAJOR:
                    slot->mAbsMTWidthMajor = rawEvent->value;
                    break;
                case ABS_MT_WIDTH_MINOR:
                    slot->mAbsMTWidthMinor = rawEvent->value;
                    slot->mHaveAbsMTWidthMinor = true;
                    break;
                case ABS_MT_ORIENTATION:
                    slot->mAbsMTOrientation = rawEvent->value;
                    break;
                case ABS_MT_TRACKING_ID:
                    if (mUsingSlotsProtocol && rawEvent->value < 0) {
                        // The slot is no longer in use but it retains its previous contents,
                        // which may be reused for subsequent touches.
                        slot->mInUse = false;
                    } else {
                        slot->mInUse = true;
                        slot->mAbsMTTrackingId = rawEvent->value;
                    }
                    break;
                case ABS_MT_PRESSURE:
                    slot->mAbsMTPressure = rawEvent->value;
                    break;
                case ABS_MT_DISTANCE:
                    slot->mAbsMTDistance = rawEvent->value;
                    break;
                case ABS_MT_TOOL_TYPE:
                    slot->mAbsMTToolType = rawEvent->value;
                    slot->mHaveAbsMTToolType = true;
                    break;
            }
        }
    } else if (rawEvent->type == EV_SYN && rawEvent->code == SYN_MT_REPORT) {
        // MultiTouch Sync: The driver has returned all data for *one* of the pointers.
        mCurrentSlot += 1;
    }
}
```

#### TouchInputMapper::sync

```cpp
void TouchInputMapper::sync(nsecs_t when, nsecs_t readTime) {
	//创建一个RawState并加入mRawStatesPending列表
    // Push a new state.
    mRawStatesPending.emplace_back();
	//初始化上面的RawState, 将相关的各个mapper.process()中记录的事件的属性写入（sync）
    RawState& next = mRawStatesPending.back();
    next.clear();
    next.when = when;
    next.readTime = readTime;
	...
    // Sync touch
    //同步触摸的状态，这里是调用子类MultiTouchInputMapper的实现，将子类的process中赋值的多指触摸相关属性赋值给next
    syncTouch(when, &next);

    // The last RawState is the actually second to last, since we just added a new state
    const RawState& last =
            mRawStatesPending.size() == 1 ? mCurrentRawState : mRawStatesPending.rbegin()[1];

	...

    processRawTouches(false /*timeout*/);
}
```

#### TouchInputMapper::processRawTouches

frameworks/native/services/inputflinger/reader/mapper/TouchInputMapper.cpp

```cpp
void TouchInputMapper::processRawTouches(bool timeout) {
	//如果当前事件的设备被禁用，则中断这个触摸事件集的传递
    if (mDeviceMode == DeviceMode::DISABLED) {
        // Drop all input if the device is disabled.
        cancelTouch(mCurrentRawState.when, mCurrentRawState.readTime);
        mCurrentCookedState.clear();
        updateTouchSpots();
        return;
    }

    // Drain any pending touch states. The invariant here is that the mCurrentRawState is always
    // valid and must go through the full cook and dispatch cycle. This ensures that anything
    // touching the current state will only observe the events that have been dispatched to the
    // rest of the pipeline.
    const size_t N = mRawStatesPending.size();
    size_t count;
    for (count = 0; count < N; count++) {
        const RawState& next = mRawStatesPending[count];

        // A failure to assign the stylus id means that we're waiting on stylus data
        // and so should defer the rest of the pipeline.
        //检测手写笔id是否可以获取到，如果无法获取就中断本次触摸批事件处理
        if (assignExternalStylusId(next, timeout)) {
            break;
        }

        // All ready to go.
        clearStylusDataPendingFlags();  //移除手写笔状态相关变量
        mCurrentRawState.copyFrom(next);  //将next赋给mCurrentRawState
		...
        cookAndDispatch(mCurrentRawState.when, mCurrentRawState.readTime);
    }
    if (count != 0) {
    	//从mRawStatesPending移除本次已处理的RawState，一个RawState对应一次SYNC包含的触摸事件集
        mRawStatesPending.erase(mRawStatesPending.begin(), mRawStatesPending.begin() + count);
    }
	...
}
```

#### TouchInputMapper::cookAndDispatch

```cpp
void TouchInputMapper::cookAndDispatch(nsecs_t when, nsecs_t readTime) {
    // Always start with a clean state.
    mCurrentCookedState.clear();

    // Apply stylus buttons to current raw state.
    applyExternalStylusButtonState(when);  //处理手写笔按键状态

    // Handle policy on initial down or hover events.
    bool initialDown = mLastRawState.rawPointerData.pointerCount == 0 &&
            mCurrentRawState.rawPointerData.pointerCount != 0;

    uint32_t policyFlags = 0;
    bool buttonsPressed = mCurrentRawState.buttonState & ~mLastRawState.buttonState;
    if (initialDown || buttonsPressed) {
        // If this is a touch screen, hide the pointer on an initial down.
        if (mDeviceMode == DeviceMode::DIRECT) {
            getContext()->fadePointer();
        }

        if (mParameters.wake) {
            policyFlags |= POLICY_FLAG_WAKE;
        }
    }

    // Consume raw off-screen touches before cooking pointer data.
    // If touches are consumed, subsequent code will not receive any pointer data.
    //处理一些非屏幕区域的触摸行为，比如屏幕外的虚拟按键等，
    //如果本次触摸是虚拟按键，那么本次触摸事件将会被虚拟按键逻辑消费，不在往下处理
    if (consumeRawTouches(when, readTime, policyFlags)) {
        mCurrentRawState.rawPointerData.clear();
    }

    // Cook pointer data.  This call populates the mCurrentCookedState.cookedPointerData structure
    // with cooked pointer data that has the same ids and indices as the raw data.
    // The following code can use either the raw or cooked data, as needed.
    //这里就是将触摸设备(TP)的坐标范围映射到实际屏幕的坐标范围等（常识：TP的坐标不一定和屏幕的坐标对应，所以需要转化）
    cookPointerData();

    // Apply stylus pressure to current cooked state.
    applyExternalStylusTouchState(when);  ////处理手写笔触摸状态

    // Synthesize key down from raw buttons if needed.
    // 如果上面有处理触摸按键相关的事件，在这里会进行按键事件的分发，走按键分发逻辑（notifyKey）
    synthesizeButtonKeys(getContext(), AKEY_EVENT_ACTION_DOWN, when, readTime, getDeviceId(),
                         mSource, mViewport.displayId, policyFlags, mLastCookedState.buttonState,
                         mCurrentCookedState.buttonState);

    // Dispatch the touches either directly or by translation through a pointer on screen.
    if (mDeviceMode == DeviceMode::POINTER) {  //pointer光标
		...
    } else {  //触摸 DeviceMode::DIRECT touchscreen
        if (!mCurrentMotionAborted) {
            updateTouchSpots();  //更新触摸点相关
            dispatchButtonRelease(when, readTime, policyFlags);  //一般触摸不涉及，不进行任何处理
            dispatchHoverExit(when, readTime, policyFlags);  //一般触摸不涉及，不进行任何处理
            dispatchTouches(when, readTime, policyFlags);  // 分发触摸事件，主要是这里
            dispatchHoverEnterAndMove(when, readTime, policyFlags);  //一般触摸不涉及，不进行任何处理
            dispatchButtonPress(when, readTime, policyFlags);  //一般触摸不涉及，不进行任何处理
        }

        if (mCurrentCookedState.cookedPointerData.pointerCount == 0) {
            mCurrentMotionAborted = false;
        }
    }

    // Synthesize key up from raw buttons if needed.
    synthesizeButtonKeys(getContext(), AKEY_EVENT_ACTION_UP, when, readTime, getDeviceId(), mSource,
                         mViewport.displayId, policyFlags, mLastCookedState.buttonState,
                         mCurrentCookedState.buttonState);

    // Clear some transient state.
    mCurrentRawState.rawVScroll = 0;
    mCurrentRawState.rawHScroll = 0;

    // Copy current touch to last touch in preparation for the next cycle.
    mLastRawState.copyFrom(mCurrentRawState);
    mLastCookedState.copyFrom(mCurrentCookedState);
}
```

#### TouchInputMapper::cookPointerData

```cpp
void TouchInputMapper::cookPointerData() {
    uint32_t currentPointerCount = mCurrentRawState.rawPointerData.pointerCount;
	
    mCurrentCookedState.cookedPointerData.clear();
    mCurrentCookedState.cookedPointerData.pointerCount = currentPointerCount;
    mCurrentCookedState.cookedPointerData.hoveringIdBits =
            mCurrentRawState.rawPointerData.hoveringIdBits;
    mCurrentCookedState.cookedPointerData.touchingIdBits =
            mCurrentRawState.rawPointerData.touchingIdBits;
    mCurrentCookedState.cookedPointerData.canceledIdBits =
            mCurrentRawState.rawPointerData.canceledIdBits;

    if (mCurrentCookedState.cookedPointerData.pointerCount == 0) {
        mCurrentCookedState.buttonState = 0;
    } else {
        mCurrentCookedState.buttonState = mCurrentRawState.buttonState;
    }


    // Walk through the the active pointers and map device coordinates onto
    // display coordinates and adjust for display orientation.
    // 遍历活动指针并将设备坐标映射到显示坐标并调整显示方向。
    // 并将相关的转化值写入mCurrentCookedState.cookedPointerData
    for (uint32_t i = 0; i < currentPointerCount; i++) {
        const RawPointerData::Pointer& in = mCurrentRawState.rawPointerData.pointers[i];

        // Size
        // 长短轴大小校准
        float touchMajor, touchMinor, toolMajor, toolMinor, size;
        switch (mCalibration.sizeCalibration) {
            case Calibration::SizeCalibration::GEOMETRIC:  //几何类型
            case Calibration::SizeCalibration::DIAMETER:  //直径类型
            case Calibration::SizeCalibration::BOX:  // 方型？
            case Calibration::SizeCalibration::AREA:  //面积类型？
                if (mRawPointerAxes.touchMajor.valid && mRawPointerAxes.toolMajor.valid) {
                    touchMajor = in.touchMajor;
                    touchMinor = mRawPointerAxes.touchMinor.valid ? in.touchMinor : in.touchMajor;
                    toolMajor = in.toolMajor;
                    toolMinor = mRawPointerAxes.toolMinor.valid ? in.toolMinor : in.toolMajor;
                    size = mRawPointerAxes.touchMinor.valid ? avg(in.touchMajor, in.touchMinor)
                                                            : in.touchMajor;
                } else if (mRawPointerAxes.touchMajor.valid) {
                    toolMajor = touchMajor = in.touchMajor;
                    toolMinor = touchMinor =
                            mRawPointerAxes.touchMinor.valid ? in.touchMinor : in.touchMajor;
                    size = mRawPointerAxes.touchMinor.valid ? avg(in.touchMajor, in.touchMinor)
                                                            : in.touchMajor;
                } else if (mRawPointerAxes.toolMajor.valid) {
                    touchMajor = toolMajor = in.toolMajor;
                    touchMinor = toolMinor =
                            mRawPointerAxes.toolMinor.valid ? in.toolMinor : in.toolMajor;
                    size = mRawPointerAxes.toolMinor.valid ? avg(in.toolMajor, in.toolMinor)
                                                           : in.toolMajor;
                } else {
					...
                }

                if (mCalibration.haveSizeIsSummed && mCalibration.sizeIsSummed) {
                    uint32_t touchingCount = mCurrentRawState.rawPointerData.touchingIdBits.count();
                    if (touchingCount > 1) {
                        touchMajor /= touchingCount;
                        touchMinor /= touchingCount;
                        toolMajor /= touchingCount;
                        toolMinor /= touchingCount;
                        size /= touchingCount;
                    }
                }

                if (mCalibration.sizeCalibration == Calibration::SizeCalibration::GEOMETRIC) {
                    touchMajor *= mGeometricScale;
                    touchMinor *= mGeometricScale;
                    toolMajor *= mGeometricScale;
                    toolMinor *= mGeometricScale;
                } else if (mCalibration.sizeCalibration == Calibration::SizeCalibration::AREA) {
                    touchMajor = touchMajor > 0 ? sqrtf(touchMajor) : 0;
                    touchMinor = touchMajor;
                    toolMajor = toolMajor > 0 ? sqrtf(toolMajor) : 0;
                    toolMinor = toolMajor;
                } else if (mCalibration.sizeCalibration == Calibration::SizeCalibration::DIAMETER) {
                    touchMinor = touchMajor;
                    toolMinor = toolMajor;
                }

                mCalibration.applySizeScaleAndBias(&touchMajor);
                mCalibration.applySizeScaleAndBias(&touchMinor);
                mCalibration.applySizeScaleAndBias(&toolMajor);
                mCalibration.applySizeScaleAndBias(&toolMinor);
                size *= mSizeScale;
                break;
            default:
                touchMajor = 0;
                touchMinor = 0;
                toolMajor = 0;
                toolMinor = 0;
                size = 0;
                break;
        }

        // Pressure
        //压感校准
        float pressure;
        switch (mCalibration.pressureCalibration) {
            case Calibration::PressureCalibration::PHYSICAL:
            case Calibration::PressureCalibration::AMPLITUDE:
                pressure = in.pressure * mPressureScale;
                break;
            default:
                pressure = in.isHovering ? 0 : 1;
                break;
        }

        // Tilt and Orientation
        //倾斜角度和方向
        float tilt;
        float orientation;
        if (mHaveTilt) {
            float tiltXAngle = (in.tiltX - mTiltXCenter) * mTiltXScale;
            float tiltYAngle = (in.tiltY - mTiltYCenter) * mTiltYScale;
            orientation = atan2f(-sinf(tiltXAngle), sinf(tiltYAngle));
            tilt = acosf(cosf(tiltXAngle) * cosf(tiltYAngle));
        } else {
            tilt = 0;

            switch (mCalibration.orientationCalibration) {
                case Calibration::OrientationCalibration::INTERPOLATED:
                    orientation = in.orientation * mOrientationScale;
                    break;
                case Calibration::OrientationCalibration::VECTOR: {
                    int32_t c1 = signExtendNybble((in.orientation & 0xf0) >> 4);
                    int32_t c2 = signExtendNybble(in.orientation & 0x0f);
                    if (c1 != 0 || c2 != 0) {
                        orientation = atan2f(c1, c2) * 0.5f;
                        float confidence = hypotf(c1, c2);
                        float scale = 1.0f + confidence / 16.0f;
                        touchMajor *= scale;
                        touchMinor /= scale;
                        toolMajor *= scale;
                        toolMinor /= scale;
                    } else {
                        orientation = 0;
                    }
                    break;
                }
                default:
                    orientation = 0;
            }
        }

        // Distance
        //距离校准
        float distance;
        switch (mCalibration.distanceCalibration) {
            case Calibration::DistanceCalibration::SCALED:
                distance = in.distance * mDistanceScale;
                break;
            default:
                distance = 0;
        }

        // Coverage
        // 覆盖校准？
        int32_t rawLeft, rawTop, rawRight, rawBottom;
        switch (mCalibration.coverageCalibration) {
            case Calibration::CoverageCalibration::BOX:
                rawLeft = (in.toolMinor & 0xffff0000) >> 16;
                rawRight = in.toolMinor & 0x0000ffff;
                rawBottom = in.toolMajor & 0x0000ffff;
                rawTop = (in.toolMajor & 0xffff0000) >> 16;
                break;
            default:
                rawLeft = rawTop = rawRight = rawBottom = 0;
                break;
        }

        // Adjust X,Y coords for device calibration
        // TODO: Adjust coverage coords?
        float xTransformed = in.x, yTransformed = in.y;
        mAffineTransform.applyTo(xTransformed, yTransformed);
        rotateAndScale(xTransformed, yTransformed);

        // Adjust X, Y, and coverage coords for input device orientation.
        float left, top, right, bottom;
		// 转换触摸设备坐标到实际的屏幕方向上的坐标
        switch (mInputDeviceOrientation) {
            case DISPLAY_ORIENTATION_90:
                left = float(rawTop - mRawPointerAxes.y.minValue) * mYScale;
                right = float(rawBottom - mRawPointerAxes.y.minValue) * mYScale;
                bottom = float(mRawPointerAxes.x.maxValue - rawLeft) * mXScale;
                top = float(mRawPointerAxes.x.maxValue - rawRight) * mXScale;
                orientation -= M_PI_2;
                if (mOrientedRanges.haveOrientation &&
                    orientation < mOrientedRanges.orientation.min) {
                    orientation +=
                            (mOrientedRanges.orientation.max - mOrientedRanges.orientation.min);
                }
                break;
            case DISPLAY_ORIENTATION_180:
                left = float(mRawPointerAxes.x.maxValue - rawRight) * mXScale;
                right = float(mRawPointerAxes.x.maxValue - rawLeft) * mXScale;
                bottom = float(mRawPointerAxes.y.maxValue - rawTop) * mYScale;
                top = float(mRawPointerAxes.y.maxValue - rawBottom) * mYScale;
                orientation -= M_PI;
                if (mOrientedRanges.haveOrientation &&
                    orientation < mOrientedRanges.orientation.min) {
                    orientation +=
                            (mOrientedRanges.orientation.max - mOrientedRanges.orientation.min);
                }
                break;
            case DISPLAY_ORIENTATION_270:
                left = float(mRawPointerAxes.y.maxValue - rawBottom) * mYScale;
                right = float(mRawPointerAxes.y.maxValue - rawTop) * mYScale;
                bottom = float(rawRight - mRawPointerAxes.x.minValue) * mXScale;
                top = float(rawLeft - mRawPointerAxes.x.minValue) * mXScale;
                orientation += M_PI_2;
                if (mOrientedRanges.haveOrientation &&
                    orientation > mOrientedRanges.orientation.max) {
                    orientation -=
                            (mOrientedRanges.orientation.max - mOrientedRanges.orientation.min);
                }
                break;
            default:
                left = float(rawLeft - mRawPointerAxes.x.minValue) * mXScale;
                right = float(rawRight - mRawPointerAxes.x.minValue) * mXScale;
                bottom = float(rawBottom - mRawPointerAxes.y.minValue) * mYScale;
                top = float(rawTop - mRawPointerAxes.y.minValue) * mYScale;
                break;
        }

        // Write output coords.
        PointerCoords& out = mCurrentCookedState.cookedPointerData.pointerCoords[i];
        out.clear();
        out.setAxisValue(AMOTION_EVENT_AXIS_X, xTransformed);
        out.setAxisValue(AMOTION_EVENT_AXIS_Y, yTransformed);
        out.setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, pressure);
        out.setAxisValue(AMOTION_EVENT_AXIS_SIZE, size);
        out.setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, touchMajor);
        out.setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, touchMinor);
        out.setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, orientation);
        out.setAxisValue(AMOTION_EVENT_AXIS_TILT, tilt);
        out.setAxisValue(AMOTION_EVENT_AXIS_DISTANCE, distance);
        if (mCalibration.coverageCalibration == Calibration::CoverageCalibration::BOX) {
            out.setAxisValue(AMOTION_EVENT_AXIS_GENERIC_1, left);
            out.setAxisValue(AMOTION_EVENT_AXIS_GENERIC_2, top);
            out.setAxisValue(AMOTION_EVENT_AXIS_GENERIC_3, right);
            out.setAxisValue(AMOTION_EVENT_AXIS_GENERIC_4, bottom);
        } else {
            out.setAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR, toolMajor);
            out.setAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR, toolMinor);
        }

        // Write output relative fields if applicable.
        uint32_t id = in.id;
        if (mSource == AINPUT_SOURCE_TOUCHPAD &&
            mLastCookedState.cookedPointerData.hasPointerCoordsForId(id)) {
            const PointerCoords& p = mLastCookedState.cookedPointerData.pointerCoordsForId(id);
            float dx = xTransformed - p.getAxisValue(AMOTION_EVENT_AXIS_X);
            float dy = yTransformed - p.getAxisValue(AMOTION_EVENT_AXIS_Y);
            out.setAxisValue(AMOTION_EVENT_AXIS_RELATIVE_X, dx);
            out.setAxisValue(AMOTION_EVENT_AXIS_RELATIVE_Y, dy);
        }

        // Write output properties.
        PointerProperties& properties = mCurrentCookedState.cookedPointerData.pointerProperties[i];
        properties.clear();
        properties.id = id;
        properties.toolType = in.toolType;

        // Write id index and mark id as valid.
        mCurrentCookedState.cookedPointerData.idToIndex[id] = i;
        mCurrentCookedState.cookedPointerData.validIdBits.markBit(id);
    }
}
```

#### TouchInputMapper::dispatchTouches

```cpp
void TouchInputMapper::dispatchTouches(nsecs_t when, nsecs_t readTime, uint32_t policyFlags) {
    BitSet32 currentIdBits = mCurrentCookedState.cookedPointerData.touchingIdBits;
    BitSet32 lastIdBits = mLastCookedState.cookedPointerData.touchingIdBits;
    int32_t metaState = getContext()->getGlobalMetaState();
    int32_t buttonState = mCurrentCookedState.buttonState;

    if (currentIdBits == lastIdBits) {  //pointer id 没有改变
        if (!currentIdBits.isEmpty()) {
            // No pointer id changes so this is a move event.
            // The listener takes care of batching moves so we don't have to deal with that here.
            // 如果pointer id 没有改变，说明是个move事件，这里不进行处理，直接分发到listener
            dispatchMotion(when, readTime, policyFlags, mSource, AMOTION_EVENT_ACTION_MOVE, 0, 0,
                           metaState, buttonState, AMOTION_EVENT_EDGE_FLAG_NONE,
                           mCurrentCookedState.cookedPointerData.pointerProperties,
                           mCurrentCookedState.cookedPointerData.pointerCoords,
                           mCurrentCookedState.cookedPointerData.idToIndex, currentIdBits, -1,
                           mOrientedXPrecision, mOrientedYPrecision, mDownTime);
        }
    } else {
        // There may be pointers going up and pointers going down and pointers moving
        // all at the same time.
        // 获取并更新四个方向pointers的值，判断本次是down、up、move的哪一种
        BitSet32 upIdBits(lastIdBits.value & ~currentIdBits.value);
        BitSet32 downIdBits(currentIdBits.value & ~lastIdBits.value);
        BitSet32 moveIdBits(lastIdBits.value & currentIdBits.value);
        BitSet32 dispatchedIdBits(lastIdBits.value);  //用于记录上一次分发的事件

        // Update last coordinates of pointers that have moved so that we observe the new
        // pointer positions at the same time as other pointers that have just gone up.
        //更新最后一个pointers的坐标
        bool moveNeeded =
                updateMovedPointers(mCurrentCookedState.cookedPointerData.pointerProperties,
                                    mCurrentCookedState.cookedPointerData.pointerCoords,
                                    mCurrentCookedState.cookedPointerData.idToIndex,
                                    mLastCookedState.cookedPointerData.pointerProperties,
                                    mLastCookedState.cookedPointerData.pointerCoords,
                                    mLastCookedState.cookedPointerData.idToIndex, moveIdBits);
        if (buttonState != mLastCookedState.buttonState) {
            moveNeeded = true;
        }

        // Dispatch pointer up events.
        //分发up事件
        while (!upIdBits.isEmpty()) {
            uint32_t upId = upIdBits.clearFirstMarkedBit();  //这个id应该和downId相同（看上面的更新四个方向BitSet32可知）
            bool isCanceled = mCurrentCookedState.cookedPointerData.canceledIdBits.hasBit(uplastIdBitsId);
            if (isCanceled) {
                ALOGI("Canceling pointer %d for the palm event was detected.", upId);
            }
            dispatchMotion(when, readTime, policyFlags, mSource, AMOTION_EVENT_ACTION_POINTER_UP, 0,
                           isCanceled ? AMOTION_EVENT_FLAG_CANCELED : 0, metaState, buttonState, 0,
                           mLastCookedState.cookedPointerData.pointerProperties,
                           mLastCookedState.cookedPointerData.pointerCoords,
                           mLastCookedState.cookedPointerData.idToIndex, dispatchedIdBits, upId,
                           mOrientedXPrecision, mOrientedYPrecision, mDownTime);
            dispatchedIdBits.clearBit(upId);  //up事件分发后，重置dispatchedIdBits
            mCurrentCookedState.cookedPointerData.canceledIdBits.clearBit(upId);
        }

        // Dispatch move events if any of the remaining pointers moved from their old locations.
        // Although applications receive new locations as part of individual pointer up
        // events, they do not generally handle them except when presented in a move event.
        //分发move事件
        if (moveNeeded && !moveIdBits.isEmpty()) {
            ALOG_ASSERT(moveIdBits.value == dispatchedIdBits.value);
            dispatchMotion(when, readTime, policyFlags, mSource, AMOTION_EVENT_ACTION_MOVE, 0, 0,
                           metaState, buttonState, 0,
                           mCurrentCookedState.cookedPointerData.pointerProperties,
                           mCurrentCookedState.cookedPointerData.pointerCoords,
                           mCurrentCookedState.cookedPointerData.idToIndex, dispatchedIdBits, -1,
                           mOrientedXPrecision, mOrientedYPrecision, mDownTime);
        }

        // Dispatch pointer down events using the new pointer locations.
        //分发down事件
        while (!downIdBits.isEmpty()) {
            uint32_t downId = downIdBits.clearFirstMarkedBit();
            dispatchedIdBits.markBit(downId); //记录分发的down事件

            if (dispatchedIdBits.count() == 1) {
                // First pointer is going down.  Set down time.
                mDownTime = when;
            }

            dispatchMotion(when, readTime, policyFlags, mSource, AMOTION_EVENT_ACTION_POINTER_DOWN,
                           0, 0, metaState, buttonState, 0,
                           mCurrentCookedState.cookedPointerData.pointerProperties,
                           mCurrentCookedState.cookedPointerData.pointerCoords,
                           mCurrentCookedState.cookedPointerData.idToIndex, dispatchedIdBits,
                           downId, mOrientedXPrecision, mOrientedYPrecision, mDownTime);
        }
    }
}
```

#### TouchInputMapper::dispatchMotion

```cpp
void TouchInputMapper::dispatchMotion(nsecs_t when, nsecs_t readTime, uint32_t policyFlags,
                                      uint32_t source, int32_t action, int32_t actionButton,
                                      int32_t flags, int32_t metaState, int32_t buttonState,
                                      int32_t edgeFlags, const PointerProperties* properties,
                                      const PointerCoords* coords, const uint32_t* idToIndex,
                                      BitSet32 idBits, int32_t changedId, float xPrecision,
                                      float yPrecision, nsecs_t downTime) {
    PointerCoords pointerCoords[MAX_POINTERS];
    PointerProperties pointerProperties[MAX_POINTERS];
    uint32_t pointerCount = 0;
    while (!idBits.isEmpty()) {
        uint32_t id = idBits.clearFirstMarkedBit();
        uint32_t index = idToIndex[id];
        pointerProperties[pointerCount].copyFrom(properties[index]);
        pointerCoords[pointerCount].copyFrom(coords[index]);

        if (changedId >= 0 && id == uint32_t(changedId)) {
            action |= pointerCount << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        }

        pointerCount += 1;
    }

    ALOG_ASSERT(pointerCount != 0);

    if (changedId >= 0 && pointerCount == 1) {
        // Replace initial down and final up action.
        // We can compare the action without masking off the changed pointer index
        // because we know the index is 0.
        if (action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            action = AMOTION_EVENT_ACTION_DOWN;
        } else if (action == AMOTION_EVENT_ACTION_POINTER_UP) {
            if ((flags & AMOTION_EVENT_FLAG_CANCELED) != 0) {
                action = AMOTION_EVENT_ACTION_CANCEL;
            } else {
                action = AMOTION_EVENT_ACTION_UP;
            }
        } else {
            // Can't happen.
            ALOG_ASSERT(false);
        }
    }
    float xCursorPosition = AMOTION_EVENT_INVALID_CURSOR_POSITION;
    float yCursorPosition = AMOTION_EVENT_INVALID_CURSOR_POSITION;
    if (mDeviceMode == DeviceMode::POINTER) {  //是否是pointer模式，触摸是DIRECT模式，所以一般不设置这个
        mPointerController->getPosition(&xCursorPosition, &yCursorPosition);
    }
    //获取当前物理屏幕的id（有些设备有多个屏幕）
    //获取displayId主要从DisplayViewport中，而DisplayViewport主要来自：
    // 1. inputDevice中通过configure中指定。
  	// 2. WindowManagerService指定。
  	// 3.通过idc文件中的唯一id或显示类型获取匹配的Viewport
    const int32_t displayId = getAssociatedDisplayId().value_or(ADISPLAY_ID_NONE);
    const int32_t deviceId = getDeviceId();
    std::vector<TouchVideoFrame> frames = getDeviceContext().getVideoFrames();  //有关视频触摸事件的，一般不是
    std::for_each(frames.begin(), frames.end(),
                  [this](TouchVideoFrame& frame) { frame.rotate(this->mInputDeviceOrientation); });
    //创建NotifyMotionArgs对象
    NotifyMotionArgs args(getContext()->getNextId(), when, readTime, deviceId, source, displayId,
                          policyFlags, action, actionButton, flags, metaState, buttonState,
                          MotionClassification::NONE, edgeFlags, pointerCount, pointerProperties,
                          pointerCoords, xPrecision, yPrecision, xCursorPosition, yCursorPosition,
                          downTime, std::move(frames));
     // 调用mapper创建时关联的context持有的QueuedInputListener对象的notifyMotion
     // 这里是getListener是UnwantedInteractionBlocker
    getListener().notifyMotion(&args);
}
```

#### QueuedInputListener::notifyMotion

```cpp
void QueuedInputListener::notifyMotion(const NotifyMotionArgs* args) {
    ALOGD("hongxi.zhu: QueuedInputListener::notifyMotion() -> mArgsQueue.emplace_back(NotifyMotionArgs)");
    traceEvent(__func__, args->id);
    mArgsQueue.emplace_back(std::make_unique<NotifyMotionArgs>(*args));
}
```

`QueuedInputListener::notifyMotion`只是将`NotifyMotionArgs`加入当前`listener`对象的`mArgsQueue`列表, 到这里我们`InputReader::processEventsLocked`方法就执行结束，回到`loopOnce()`中继续往下执行到`mQueuedListener.flush()`，开始事件的传递。

### 二、mQueuedListener.flush()事件传递

接着上面，在`InputReader::loopOnce()`中往下执行到`mQueuedListener.flush()`

#### QueuedInputListener::flush()

frameworks/native/services/inputflinger/InputListener.cpp

```cpp
void QueuedInputListener::flush() {
    for (const std::unique_ptr<NotifyArgs>& args : mArgsQueue) {
        args->notify(mInnerListener);  //args这里是前面传进来的NotifyMotionArgs
    }
    mArgsQueue.clear();
}
```

#### NotifyMotionArgs::notify

frameworks/native/services/inputflinger/InputListener.cpp

```cpp
void NotifyMotionArgs::notify(InputListenerInterface& listener) const {
    listener.notifyMotion(this);
}
```

这个`listener`是一个`InputListenerInterface`子类对象，到底是哪个呢？回顾下`InputManager`的构造方法，  
frameworks/native/services/inputflinger/InputManager.cpp

```cpp
InputManager::InputManager(
        const sp<InputReaderPolicyInterface>& readerPolicy,
        const sp<InputDispatcherPolicyInterface>& dispatcherPolicy) {
    mDispatcher = createInputDispatcher(dispatcherPolicy);
    mClassifier = std::make_unique<InputClassifier>(*mDispatcher);
    mBlocker = std::make_unique<UnwantedInteractionBlocker>(*mClassifier);
    mReader = createInputReader(readerPolicy, *mBlocker);
}
```

`mDispatcher`、`mClassifier`、`mBlocker`都是`InputListenerInterface`的子类，从构造方法的传参得出事件传递的过程：  
`InputReader->UnwantedInteractionBlocker->InputClassifier->InputDispatcher`  
结合类继承关系，可知上面`InputReader::loopOnce()`中的`mQueuedListener`是`UnwantedInteractionBlocker`，调用它的`flush`方法(这几个子类都没有实现`flush`方法，所以执行的是`QueuedInputListener`的`flush`方法)，`QueuedInputListener`中的`mInnerListener`是`UnwantedInteractionBlocker`，那么`listener.notifyMotion(this)`实际就是：`UnwantedInteractionBlocker::notifyMotion(NotifyMotionArgs)`

#### UnwantedInteractionBlocker::notifyMotion

frameworks/native/services/inputflinger/UnwantedInteractionBlocker.cpp

```cpp
void UnwantedInteractionBlocker::notifyMotion(const NotifyMotionArgs* args) {
    { // acquire lock
        std::scoped_lock lock(mLock);
        const std::vector<NotifyMotionArgs> processedArgs =
                mPreferStylusOverTouchBlocker.processMotion(*args);
        for (const NotifyMotionArgs& loopArgs : processedArgs) {
            notifyMotionLocked(&loopArgs);
        }
    } // release lock

    // Call out to the next stage without holding the lock
    mQueuedListener.flush();
}
```

#### UnwantedInteractionBlocker::notifyMotionLocked

```cpp
void UnwantedInteractionBlocker::notifyMotionLocked(const NotifyMotionArgs* args) {
    auto it = mPalmRejectors.find(args->deviceId);
    const bool sendToPalmRejector = it != mPalmRejectors.end() && isFromTouchscreen(args->source);
    if (!sendToPalmRejector) {  //如果不支持手掌误触处理，走这里，一般走这里
    	//调用UnwantedInteractionBlocker中持有的QueuedListener对象的notifyMotion
    	//mQueuedListener这里是
        mQueuedListener.notifyMotion(args);  //一般走这里, 按照前面
        return;
    }
	//如果支持手掌误触
    std::vector<NotifyMotionArgs> processedArgs = it->second.processMotion(*args);
processedArgs.size());
    for (const NotifyMotionArgs& loopArgs : processedArgs) {
        mQueuedListener.notifyMotion(&loopArgs);  //同理这里的mQueuedListener是InputClassifier
    }
}
```

然后又是同样的走法`notifyMotion`\->`flush`\->`notifyMotion`, `mInnerListener`是`InputClassifier`

#### QueuedInputListener::notifyMotion

```cpp
void QueuedInputListener::notifyMotion(const NotifyMotionArgs* args) {
    traceEvent(__func__, args->id);
    mArgsQueue.emplace_back(std::make_unique<NotifyMotionArgs>(*args));
}

void QueuedInputListener::flush() {
    for (const std::unique_ptr<NotifyArgs>& args : mArgsQueue) {
   	    //args这里是前面传进来的NotifyMotionArgs，mInnerListener是InputClassifier
        args->notify(mInnerListener);
    }
    mArgsQueue.clear();
}

void NotifyMotionArgs::notify(InputListenerInterface& listener) const {
    listener.notifyMotion(this);
}
```

#### InputClassifier::notifyMotion

```cpp
void InputClassifier::notifyMotion(const NotifyMotionArgs* args) {
    { // acquire lock
        std::scoped_lock lock(mLock);
        // MotionClassifier is only used for touch events, for now
        const bool sendToMotionClassifier = mMotionClassifier && isTouchEvent(*args);
        if (!sendToMotionClassifier) {  //目前模拟器走这里，真机不清楚
            mQueuedListener.notifyMotion(args);
        } else {
            NotifyMotionArgs newArgs(*args);
            newArgs.classification = mMotionClassifier->classify(newArgs);
            mQueuedListener.notifyMotion(&newArgs);
        }
    } // release lock
    mQueuedListener.flush();
}
```

然后又是同样的走法`notifyMotion`\->`flush`\->`notifyMotion`, `mInnerListener`是`InputDispatcher`

#### QueuedInputListener::notifyMotion

```cpp
void QueuedInputListener::notifyMotion(const NotifyMotionArgs* args) {
    traceEvent(__func__, args->id);
    mArgsQueue.emplace_back(std::make_unique<NotifyMotionArgs>(*args));
}

void QueuedInputListener::flush() {
    for (const std::unique_ptr<NotifyArgs>& args : mArgsQueue) {
   	    //args这里是前面传进来的NotifyMotionArgs，mInnerListener是InputClassifier
        args->notify(mInnerListener);
    }
    mArgsQueue.clear();
}
```

#### InputDispatcher::notifyMotion

```cpp
void InputDispatcher::notifyMotion(const NotifyMotionArgs* args) {
	...
    uint32_t policyFlags = args->policyFlags;
    policyFlags |= POLICY_FLAG_TRUSTED;

    android::base::Timer t;
    //回调wms中的interceptMotionBeforeQueueingNonInteractive方法
    mPolicy->interceptMotionBeforeQueueing(args->displayId, args->eventTime, /*byref*/ policyFlags);

    bool needWake = false;
    { // acquire lock
        mLock.lock();
		...

        // Just enqueue a new motion event.
        //创建一个MotionEntry对象，将NotifyMotionArgs转化为MotionEntry
        std::unique_ptr<MotionEntry> newEntry =
                std::make_unique<MotionEntry>(args->id, args->eventTime, args->deviceId,
                                              args->source, args->displayId, policyFlags,
                                              args->action, args->actionButton, args->flags,
                                              args->metaState, args->buttonState,
                                              args->classification, args->edgeFlags,
                                              args->xPrecision, args->yPrecision,
                                              args->xCursorPosition, args->yCursorPosition,
                                              args->downTime, args->pointerCount,
                                              args->pointerProperties, args->pointerCoords);
		...
		//将MotionEntry加入InboundQueue
        needWake = enqueueInboundEventLocked(std::move(newEntry)); 
        mLock.unlock();
    } // release lock

    if (needWake) {
        mLooper->wake();  //唤醒InputDispatcher线程，处理InboundQueue中的MotionEntry
    }
}
```

#### InputDispatcher::enqueueInboundEventLocked

```cpp
bool InputDispatcher::enqueueInboundEventLocked(std::unique_ptr<EventEntry> newEntry) {
    bool needWake = mInboundQueue.empty();
    mInboundQueue.push_back(std::move(newEntry));
    EventEntry& entry = *(mInboundQueue.back());
    traceInboundQueueLengthLocked();

    switch (entry.type) {
        case EventEntry::Type::KEY: {
      		...
            break;
        }

        case EventEntry::Type::MOTION: {
            LOG_ALWAYS_FATAL_IF((entry.policyFlags & POLICY_FLAG_TRUSTED) == 0,
                                "Unexpected untrusted event.");
            if (shouldPruneInboundQueueLocked(static_cast<MotionEntry&>(entry))) {
                mNextUnblockedEvent = mInboundQueue.back();
                needWake = true;
            }
            break;
			...
    }

    return needWake;
}

```

到此，InputReader线程就将事件传递到InputDispatcher线程，InputDispatcher线程被唤醒开始处理触摸事件，而InputReader线程继续循环执行loopOnce()方法，再次阻塞在getEvents方法中等待事件的上报。