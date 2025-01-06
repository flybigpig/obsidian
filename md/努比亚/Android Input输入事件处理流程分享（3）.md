_努比亚技术团队原创内容，转载请务必注明出处。_

- 实例介绍（开发者模式中的触摸小白点）
    - 开发者模式中的触摸小白点实现原理
        - 如何开启触摸小白点
        - 触摸小白点的开启
            - 设置中开启触摸小白点
            - IMS处理触摸小白点开关状态的改变
            - InputReader中配置触摸小白点开关的变更
        - 触摸小白点的绘制
            - 触摸小白点在触摸事件处理中的入口
            - 触摸小白点的参数获取
            - 触摸小白点之Sprite的准备
            - 触摸小白点的绘制
                - 触摸小白点Sprite列表锁定
                - SurfaceControl的构建
                - SurfaceControl的size修改
                - 绘制Sprite
                - Sprite参数调整
                - 更新SurfaceControl
        - 小结
    - 通过Systrace看触摸小白点绘制过程中输入事件的传递
        - Systrace抓取
        - Systrace的打开方式
        - Systrace上的InputReader
        - Systrace上的InputDispatcher
        - Systrace上的应用进程
- 总结

## 实例介绍（开发者模式中的触摸小白点）

通过以上理论上的介绍，相信大家对与整个输入事件的传输过程有了一个概念，但是对应实际中的传递流程可能还是有些生疏，下面我会通过介绍安卓开发者模式中的触摸小白点来实例介绍下具体的传递过程。

### 开发者模式中的触摸小白点实现原理

在设置中打开开发者模式，然后进入开发者模式对应的页面，找到“显示点按操作反馈”，然后打开。之后再在屏幕上触摸，就会看到在触摸的地方显示出一个白色的小圆点。本节我会带领大家一起了解这个小圆点的实现原理。

![](https://upload-images.jianshu.io/upload_images/26874665-a516c7d4cad24faf.png?imageMogr2/auto-orient/strip|imageView2/2/w/395/format/webp)

触摸小白点绘制流程

#### 如何开启触摸小白点

打开设置，然后找到开发者模式，如果手机还未开启开发者模式，请找到系统信息界面，并多次点击“版本号”，之后就会开启开发者模式。进入开发者模式界面，找到“显示点按操作反馈”，并打开后面的开关。这样就看起了触摸小白点。

|打开触摸小白点|小白点效果|
|---|---|
|![](https://upload-images.jianshu.io/upload_images/26874665-73ca427507914080.png?imageMogr2/auto-orient/strip\|imageView2/2/w/444/format/webp)<br><br>打开触摸小白点|![](//upload-images.jianshu.io/upload_images/26874665-7e2d383f93884686.png?imageMogr2/auto-orient/strip\|imageView2/2/w/445/format/webp)<br><br>小白点效果|

#### 触摸小白点的开启

要搞清楚触摸小白点的原理，我们需要先搞明白开关的触发，触发后系统会做什么操作，以及后续界面以及输入事件该如何响应等。

##### 设置中开启触摸小白点

通过开启小圆点的过程，我们能够了解到它的入口是在设置中的开发者模式界面，于是我们去设置中找到此界面，然后看对应页面的实现：

```java
private static List<AbstractPreferenceController> buildPreferenceControllers(
    Context context,
    Activity activity,
    Lifecycle lifecycle,
    DevelopmentSettingsDashboardFragment fragment,
    BluetoothA2dpConfigStore bluetoothA2dpConfigStore) {
        // 省略若干行
        // 这里显示触摸小白点对应的controller是ShowTapsPreferenceController
        controllers.add(new ShowTapsPreferenceController(context));
        controllers.add(new PointerLocationPreferenceController(context));
}
```

在DevelopmentSettingsDashboardFragment的中buildPreferenceControllers方法中，我们发现触摸小白点对应的controller是ShowTapsPreferenceController，继续看。

```java
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        // 获取开关状态
        final boolean isEnabled = (Boolean) newValue;
        // 根据开关状态修改System的值
        Settings.System.putInt(mContext.getContentResolver(),
                Settings.System.SHOW_TOUCHES, isEnabled
                ? SETTING_VALUE_ON : SETTING_VALUE_OFF);
        return true;
    }
```

##### IMS处理触摸小白点开关状态的改变

在ShowTapsPreferenceController的onPreferenceChange方法中，我们能够看到当界面开关的状态发生改变时，程序会修改Settings.System中对应的值Settings.System.SHOW_TOUCHES，于是我们继续去frameworks中查找Settings.System.SHOW_TOUCHES值的监听者。

```java
    private void registerShowTouchesSettingObserver() {
        // 注册Settings.System.SHOW_TOUCHES值改变的监听
        mContext.getContentResolver().registerContentObserver(
                Settings.System.getUriFor(Settings.System.SHOW_TOUCHES), true,
                new ContentObserver(mHandler) {
                    @Override
                    public void onChange(boolean selfChange) {
                        // 当值发生改变时
                        // 调用updateShowTouchesFromSettings进行处理
                        updateShowTouchesFromSettings();
                    }
                }, UserHandle.USER_ALL);
    }
  
    private void updateShowTouchesFromSettings() {
        int setting = getShowTouchesSetting(0);
        // 继续调用native方法做进一步处理
        nativeSetShowTouches(mPtr, setting != 0);
    }
```

通过搜索我们发现，在InputManagerService中有对Settings.System.SHOW_TOUCHES这个值进行监听，并且当其值发生改变时，会继续调用到native当中进行处理。

```cpp
static void nativeSetShowTouches(JNIEnv* /* env */,
        jclass /* clazz */, jlong ptr, jboolean enabled) {
    // 获取native中的InputManager
    NativeInputManager* im = reinterpret_cast<NativeInputManager*>(ptr);
    // 调用其setShowTouches方法设置状态
    im->setShowTouches(enabled);
}
```

在native方法中，首先会获取到native层的InputManager，然后调用其setShowTouches方法将状态设置后去。

```cpp
void NativeInputManager::setShowTouches(bool enabled) {
    { // acquire lock
        AutoMutex _l(mLock);
        // 若待改变的值和现在的状态相同则不处理
        if (mLocked.showTouches == enabled) {
            return;
        }
  
        ALOGI("Setting show touches feature to %s.", enabled ? "enabled" : "disabled");
        // 改变状态值
        mLocked.showTouches = enabled;
    } // release lock
    // 调用InputReader进行更新配置
    mInputManager->getReader()->requestRefreshConfiguration(
            InputReaderConfiguration::CHANGE_SHOW_TOUCHES);
}
```

##### InputReader中配置触摸小白点开关的变更

setShowTouches方法中，首先会判断改变状态和现有状态是否一致，不一致则改变现有状态，然后继续通知InputReader刷新配置。

```cpp
void InputReader::requestRefreshConfiguration(uint32_t changes) {
    AutoMutex _l(mLock);
    // 只处理开启状态
    if (changes) {
        bool needWake = !mConfigurationChangesToRefresh;
        // 修改配置变量
        mConfigurationChangesToRefresh |= changes;
        // 若需要唤醒EventHub，则进行唤醒
        if (needWake) {
            mEventHub->wake();
        }
    }
}
```

InputReader的requestRefreshConfiguration只会处理开启状态的事件，首先改变配置状态，然后继续唤醒EventHub进行处理。

```cpp
void EventHub::wake() {
    ALOGV("wake() called");
  
    ssize_t nWrite;
    do {
        // 向管道写入内容以唤醒EventHuab继续工作
        nWrite = write(mWakeWritePipeFd, "W", 1);
    } while (nWrite == -1 && errno == EINTR);
  
    if (nWrite != 1 && errno != EAGAIN) {
        ALOGW("Could not write wake signal: %s", strerror(errno));
    }
}
```

在EventHub的wake方法中，会向mWakeWritePipeFd对应的管道中写入内容，然后就可以唤醒EventHub继续开始工作。

```cpp
size_t EventHub::getEvents(int timeoutMillis, RawEvent* buffer, size_t bufferSize) {
    // 省略若干行
    bool awoken = false;
    // 处理管道唤醒事件
    if (eventItem.data.fd == mWakeReadPipeFd) {
        if (eventItem.events & EPOLLIN) {
            ALOGV("awoken after wake()");
            // 标识被唤醒
            awoken = true;
            char buffer[16];
            ssize_t nRead;
            do {
                // 读取管道中的数据
                nRead = read(mWakeReadPipeFd, buffer, sizeof(buffer));
            } while ((nRead == -1 && errno == EINTR) || nRead == sizeof(buffer));
    // 省略若干行
    // 跳出循环，进入到InputReader中处理
    if (event != buffer || awoken) {
        break;
    }
    // 省略若干行
}
```

在上述getEvents中，当EventHub被唤醒后，就会跳出循环，紧接着会返回到getEvents的调用方，也就是InputReader中去。

```cpp
void InputReader::loopOnce() {
    // 省略若干行
    { // acquire lock
        // 省略若干行
        uint32_t changes = mConfigurationChangesToRefresh;
        // 如果配置状态发生改变
        if (changes) {
            mConfigurationChangesToRefresh = 0;
            timeoutMillis = 0;
            // 刷新配置
            refreshConfigurationLocked(changes);
        }
    } // release lock
    // 省略若干行
}
```

EventHub被唤醒后，就会致使InputReader继续进入工作，使线程继续循环。在loopOnce中，会调用refreshConfigurationLocked方法来刷新配置。

```cpp
void InputReader::refreshConfigurationLocked(uint32_t changes) {
    // 省略若干行
    if (changes) {
        // 省略若干行
        // 处理reopen设备事件
        if (changes & InputReaderConfiguration::CHANGE_MUST_REOPEN) {
            mEventHub->requestReopenDevices();
        } else {
            // 遍历所有的设备，并更新其configure
            for (auto& devicePair : mDevices) {
                std::shared_ptr<InputDevice>& device = devicePair.second;
                device->configure(now, &mConfig, changes);
            }
        }
    }
}
```

刷新配置方法refreshConfigurationLocked，先是判断处理reopen的事件，然后会循环遍历所有的device，并调用其configure方法更新配置。

```cpp
void InputDevice::configure(nsecs_t when,
    const InputReaderConfiguration* config,
    uint32_t changes) {
    // 省略若干行
    if (!isIgnored()) {
        // 省略若干行
        // 遍历所有的device以及其mapper，并更新mapper的配置
        for_each_mapper([this, when, config, changes](InputMapper& mapper) {
            mapper.configure(when, config, changes);
            mSources |= mapper.getSources();
        });
    }
    // 省略若干行
}
```

上述inputDevice的configure方法中，会遍历每一个可用的device，并且遍历device的每一个mapper，然后更新其对于的配置。这里的InputMapper是一种和device的映射关系，能够处理一种类型的事件，例如键盘事件的有KeyboardInputMapper。这里是触摸相关的配置，所以我们继续看TouchInputMapper。

```cpp
void TouchInputMapper::configure(nsecs_t when,
    const InputReaderConfiguration* config,
    uint32_t changes) {
    // 省略若干行
        if (!changes ||
        (changes &
         (InputReaderConfiguration::CHANGE_DISPLAY_INFO |
          InputReaderConfiguration::CHANGE_POINTER_GESTURE_ENABLEMENT |
          InputReaderConfiguration::CHANGE_SHOW_TOUCHES |
          InputReaderConfiguration::CHANGE_EXTERNAL_STYLUS_PRESENCE))) {
        // Configure device sources, surface dimensions, orientation and
        // scaling factors.
        // 如果是上述这些配置发生改变，则会继续调用configureSurface
        // 去改变显示相关的参数，如显示方向、缩放等
        configureSurface(when, &resetNeeded);
    }
}
```

上面方法中会继续调用configureSurface去改变显示相关的参数，包括显示的方向、缩放的大小等等，后面的这里就不再进行展开了。下面我们继续介绍触摸小白点的绘制过程。

#### 触摸小白点的绘制

触摸小白点儿的显示是在触摸时发生，所以我们继续看touch事件的分发。从前面介绍过的InputReader中处理输入事件，我们能够看到，事件会经过InputReader，然后到达InputDevice，接着会到InputMapper的process方法。

##### 触摸小白点在触摸事件处理中的入口

触摸小白点是在手指触摸屏幕时进行绘制的，所以这里我们从看TouchInputMapper的process方法开始，继续追踪其绘制入口。

```cpp
void TouchInputMapper::process(const RawEvent* rawEvent) {
    // 省略若干行
    if (rawEvent->type == EV_SYN && rawEvent->code == SYN_REPORT) {
        // 调用sync继续处理事件
        sync(rawEvent->when);
    }
}
  
void TouchInputMapper::sync(nsecs_t when) {
    // 省略若干行
    // 直接调用processRawTouches继续处理
    processRawTouches(false /*timeout*/);
}
  
void TouchInputMapper::processRawTouches(bool timeout) {
    // 省略若干行
    // 需要处理的事件数
    const size_t N = mRawStatesPending.size();
    size_t count;
    for (count = 0; count < N; count++) {
        // 获取下一个需要处理的state
        const RawState& next = mRawStatesPending[count];
        // 省略若干行
        mCurrentRawState.copyFrom(next);
        if (mCurrentRawState.when < mLastRawState.when) {
            mCurrentRawState.when = mLastRawState.when;
        }
        // 生成并分发事件
        cookAndDispatch(mCurrentRawState.when);
    }
    // 省略若干行
}
```

上述过程中最后调用到processRawTouches方法中，此方法中会遍历每一个需要处理的state，然后计算出事件的时间，然后继续调用cookAndDispatch生成并分发touch事件。

```cpp
void TouchInputMapper::cookAndDispatch(nsecs_t when) {
    // 省略若干行
    // 上面NativieInputManager的setShowTouches方法中
    // 已经设置过mConfig.showTouches为true，所以会触发这里的逻辑
    if (mDeviceMode == DEVICE_MODE_DIRECT && mConfig.showTouches &&
        mPointerController != nullptr) {
        // 设置绘制为spot圆点
        mPointerController->setPresentation
        (PointerControllerInterface::PRESENTATION_SPOT);
        // 设置fade类型
        mPointerController->fade
        (PointerControllerInterface::TRANSITION_GRADUAL);
        // 设置button的状态
        mPointerController->setButtonState(mCurrentRawState.buttonState);
        // 设置spot对应的坐标、显示id等数据进行绘制
        mPointerController->setSpots(
            mCurrentCookedState.cookedPointerData.pointerCoords,
            mCurrentCookedState.cookedPointerData.idToIndex,
            mCurrentCookedState.cookedPointerData.touchingIdBits,
            mViewport.displayId);
    }
    // 省略若干行
}
```

上面方法调用中会设置绘制类型、fade类型，最后会将触摸的位置坐标以及显示的displayId等信息传递到PointerController中进行绘制。

##### 触摸小白点的参数获取

触摸小白点的绘制需要屏幕显示位置坐标、绘制的icon、显示id等等信息，这里介绍这些参数的获取。

```cpp
void PointerController::setSpots(const PointerCoords* spotCoords,
        const uint32_t* spotIdToIndex,
        BitSet32 spotIdBits,
        int32_t displayId) {
    // 省略若干行
    // 处理手指按下或者移动的绘制
    // Add or move spots for fingers that are down.
    for (BitSet32 idBits(spotIdBits); !idBits.isEmpty(); ) {
        uint32_t id = idBits.clearFirstMarkedBit();
        // 取出坐标
        const PointerCoords& c = spotCoords[spotIdToIndex[id]];
        // 取出icon
        const SpriteIcon& icon = c.getAxisValue(AMOTION_EVENT_AXIS_PRESSURE) > 0
                ? mResources.spotTouch : mResources.spotHover;
        // 取出x坐标
        float x = c.getAxisValue(AMOTION_EVENT_AXIS_X);
        // 取出y坐标
        float y = c.getAxisValue(AMOTION_EVENT_AXIS_Y);
        // 获取spot对象
        Spot* spot = getSpot(id, newSpots);
        if (!spot) {
            spot = createAndAddSpotLocked(id, newSpots);
        }
        // 根据displayId在屏幕上更新spot
        spot->updateSprite(&icon, x, y, displayId);
    }
    // 处理手指移除小圆点的绘制
    // Remove spots for fingers that went up.
    for (size_t i = 0; i < newSpots.size(); i++) {
        Spot* spot = newSpots[i];
        if (spot->id != Spot::INVALID_ID
                && !spotIdBits.hasBit(spot->id)) {
            // 渐变移除spot
            fadeOutAndReleaseSpotLocked(spot);
        }
    }
    // 省略若干行
}
```

上述调用过程中，会遍历处理每一个手指的按下和释放。首先获取到手指触摸的坐标、InputMapper传过来的displayId以及spot的icon，最后调用Spot的updateSprite进行绘制；接着后面还会处理可能的手指移除时spot的改变。

##### 触摸小白点之Sprite的准备

通过分析发现，触摸小白点是通过sprite来进行控制的，这里我们介绍下sprite的创建以及update过程。

```cpp
void PointerController::Spot::updateSprite(const SpriteIcon* icon,
    float x, float y, int32_t displayId) {
    // 设置显示的layer
    sprite->setLayer(Sprite::BASE_LAYER_SPOT + id);
    // 设置alpha
    sprite->setAlpha(alpha);
    // 设置transform矩阵
    sprite->setTransformationMatrix(SpriteTransformationMatrix(scale,
    0.0f, 0.0f, scale));
    // 设置x、y坐标
    sprite->setPosition(x, y);
    // 设置显示id
    sprite->setDisplayId(displayId);
    this->x = x;
    this->y = y;
    // icon不同则更新
    if (icon != lastIcon) {
        lastIcon = icon;
        if (icon) {
            // icon有效则显示出来
            sprite->setIcon(*icon);
            sprite->setVisible(true);
        } else {
            // icon无效则隐藏
            sprite->setVisible(false);
        }
    }
}
```

updateSprite方法中设置sprite的各种数据和参数，最后会通过setVisible让其显示出来。那么这里的sprite又是什么呢？回到上面PointerController的setSpots中，首先会通过getSpot获取spot，如果没有获取到则会调用createAndAddSpotLocked方法来创建和点击spot，那么我们继续看这个方法。

```cpp
PointerController::Spot* PointerController::createAndAddSpotLocked(uint32_t id,
        std::vector<Spot*>& spots) {
    // 省略若干行
    // Obtain a sprite from the recycled pool.
    sp<Sprite> sprite;
    // recycled池不为空，则获取一个
    if (! mLocked.recycledSprites.empty()) {
        sprite = mLocked.recycledSprites.back();
        mLocked.recycledSprites.pop_back();
    } else {
        // 否则通过SpriteController的createSprite方法创建一个
        sprite = mSpriteController->createSprite();
    }
    // 创建出一个Spot并返回
    // Return the new spot.
    Spot* spot = new Spot(id, sprite);
    spots.push_back(spot);
    return spot;
}
```

createAndAddSpotLocked首先尝试从回收池中回收sprite，若无法回收，则继续调用SpriteController的createSprite方法创建一个sprite，那么，继续看创建过程。

```cpp
sp<Sprite> SpriteController::createSprite() {
    // 直接构建一个SpriteImpl，传入的controller是SpriteController
    return new SpriteImpl(this);
}
```

接下来我们继续看上面Spot的updateSprite方法中，调用sprite的setVisible，其实就是SpriteImpl的setVisible方法。

##### 触摸小白点的绘制

准备好sprite之后，就到了绘制的时机了，下面将详细介绍Sprite是如果绘制到界面上的。

```cpp
void SpriteController::SpriteImpl::setVisible(bool visible) {
    AutoMutex _l(mController->mLock);
    // 只有visible改变时在刷新
    if (mLocked.state.visible != visible) {
        mLocked.state.visible = visible;
        // 刷新小圆点
        invalidateLocked(DIRTY_VISIBILITY);
    }
}
  
void SpriteController::SpriteImpl::invalidateLocked(uint32_t dirty) {
    bool wasDirty = mLocked.state.dirty;
    mLocked.state.dirty |= dirty;
    // 有数据要显示
    if (!wasDirty) {
        // 调用SpriteController的invalidateSpriteLocked方法刷新
        mController->invalidateSpriteLocked(this);
    }
}
```

上面setVisible中会调用到mController的invalidateSpriteLocked方法，这里的mController是构造是传入的SpriteController，继续看。

```cpp
void SpriteController::invalidateSpriteLocked(const sp<SpriteImpl>& sprite) {
    bool wasEmpty = mLocked.invalidatedSprites.isEmpty();
    mLocked.invalidatedSprites.push(sprite);
    // 更新前数组为空，说明有数据到达
    if (wasEmpty) {
        // 存在transaction的sprite
        if (mLocked.transactionNestingCount != 0) {
            mLocked.deferredSpriteUpdate = true;
        } else {
            // 通过looper发送消息触发更新sprite
            mLooper->sendMessage(mHandler, Message(MSG_UPDATE_SPRITES));
        }
    }
}
  
void SpriteController::handleMessage(const Message& message) {
    switch (message.what) {
    case MSG_UPDATE_SPRITES:
        // 更新sprite
        doUpdateSprites();
        break;
    case MSG_DISPOSE_SURFACES:
        doDisposeSurfaces();
        break;
    }
}
```

上面方法中首先会判断是否存在transaction过程中的sprite，然后通过looper发送更新sprite的消息，然后再handleMessage中进行处理。

```cpp
void SpriteController::doUpdateSprites() {
    Vector<SpriteUpdate> updates;
    // 1、取出需要刷新的sprites
  
    // Create missing surfaces.
    // 2、遍历每一个需要刷新的sprite，有选择的创建surfaceControl
  
    // Resize and/or reparent sprites if needed.
    SurfaceComposerClient::Transaction t;
    bool needApplyTransaction = false;
    for (size_t i = 0; i < numSprites; i++) {
        SpriteUpdate& update = updates.editItemAt(i);
        if (update.state.surfaceControl == nullptr) {
            continue;
        }
  
        // 3、修改surfaceControl的size
    }
    // Redraw sprites if needed.
    // 4、绘制sprite
  
    // 5、根据参数调整sprite
    // If any surfaces were changed, write back the new surface properties
    // to the sprites.
    // 6、如果surface改变，则进行更新
}
```

doUpdateSprites较长，其中主要完成sprite的获取、surface的创建、surface尺寸的调整、绘制sprite、调整sprite位置等信息以及最后更新可能改变的surface这一系列过程。下面我们按照这个顺序介绍各个阶段。

###### 触摸小白点Sprite列表锁定

列表的锁定主要是从全局lock对象中获取待刷新的sprite，并添加到updates列表当中。

```cpp
void SpriteController::doUpdateSprites() {
    Vector<SpriteUpdate> updates;
    size_t numSprites;
    { // acquire lock
        AutoMutex _l(mLock);
        // 获取需要刷新的Sprite数量
        numSprites = mLocked.invalidatedSprites.size();
        for (size_t i = 0; i < numSprites; i++) {
            // 遍历取出每一个需要刷新的Sprite，并将其添加的updates数组中
            const sp<SpriteImpl>& sprite = mLocked.invalidatedSprites.itemAt(i);
  
            updates.push(SpriteUpdate(sprite, sprite->getStateLocked()));
            sprite->resetDirtyLocked();
        }
        mLocked.invalidatedSprites.clear();
    } // release lock
}
```

Sprite列表的锁定过程，主要是从全局的lock对象中获取到数据列表，然后挨个取出放到updates数组中去，这样的操作主要是为了避免长时间持有mLock锁，导致前面Sprite准备过程可能出现阻塞进而引起触摸小白点显示不及时等问题。

###### SurfaceControl的构建

此过程主要是遍历每一个需要绘制的Sprite，创建可能缺失的SurfaceControl。

```cpp
void SpriteController::doUpdateSprites() {
    // Create missing surfaces.
    bool surfaceChanged = false;
    // 挨个遍历updates数组处理每一个sprite
    for (size_t i = 0; i < numSprites; i++) {
        // 取出sprite
        SpriteUpdate& update = updates.editItemAt(i);
        // 如果还不存在surfaceControl并且需要绘制
        if (update.state.surfaceControl == NULL && update.state.
        wantSurfaceVisible()) {
            // 从绘制的icon中获取宽高
            update.state.surfaceWidth = update.state.icon.bitmap
            .getInfo().width;
            update.state.surfaceHeight = update.state.icon.bitmap
            .getInfo().height;
            update.state.surfaceDrawn = false;
            update.state.surfaceVisible = false;
            // 根据宽高构建surfaceControl
            update.state.surfaceControl = obtainSurface(
                    update.state.surfaceWidth, update.state.surfaceHeight);
            if (update.state.surfaceControl != NULL) {
                // 标识需要刷新surfaceControl
                update.surfaceChanged = surfaceChanged = true;
            }
        }
    }
}
```

准备surfaceControl阶段，主要是遍历每一个Sprite，然后判断如果还不存在surfaceControl并且需要绘制的话，则构建surfaceControl，最后标识需要刷新surfaceControl。

###### SurfaceControl的size修改

主要是遍历每一个需要update的Sprite，然后获取其对应的icon中bitmap的宽和高，判断surface的宽高小于icon的，则更新之。

```cpp
void SpriteController::doUpdateSprites() {
    SurfaceComposerClient::Transaction t;
    bool needApplyTransaction = false;
    for (size_t i = 0; i < numSprites; i++) {
        // 取出Sprite
        SpriteUpdate& update = updates.editItemAt(i);
        if (update.state.surfaceControl == nullptr) {
            continue;
        }
        // 需要绘制
        if (update.state.wantSurfaceVisible()) {
            // 获取icon宽和高
            int32_t desiredWidth = update.state.icon.bitmap.getInfo().width;
            int32_t desiredHeight = update.state.icon.bitmap.getInfo().height;
            // surface宽和高小于icon的则使用icon的宽高
            if (update.state.surfaceWidth < desiredWidth
                    || update.state.surfaceHeight < desiredHeight) {
                needApplyTransaction = true;
                // 更新宽高
                t.setSize(update.state.surfaceControl,
                        desiredWidth, desiredHeight);
                update.state.surfaceWidth = desiredWidth;
                update.state.surfaceHeight = desiredHeight;
                update.state.surfaceDrawn = false;
                update.surfaceChanged = surfaceChanged = true;
  
                if (update.state.surfaceVisible) {
                    t.hide(update.state.surfaceControl);
                    update.state.surfaceVisible = false;
                }
            }
        }
}
```

调整surface宽和高阶段，首先会过滤掉需要刷新显示的Sprite，然后从icon中获取实际的宽和高，然后判断surface的尺寸如果小于icon的，则使用icon的尺寸进行更新。

###### 绘制Sprite

绘制Sprite采用直接使用Surface进行绘制，期间会构建出Paint以及canvas，并通过drawBitmap方法将bitmap绘制到canvas，最后通过unlockAndPost提交。

```cpp
void SpriteController::doUpdateSprites() {
    for (size_t i = 0; i < numSprites; i++) {
        // 取出每一个需要刷新的Sprite
        SpriteUpdate& update = updates.editItemAt(i);
        // 省略若干行
        if (update.state.surfaceControl != NULL && !update.state.surfaceDrawn
                && update.state.wantSurfaceVisible()) {
            // 获取surface
            sp<Surface> surface = update.state.surfaceControl->getSurface();
            ANativeWindow_Buffer outBuffer;
            // lock buffer
            status_t status = surface->lock(&outBuffer, NULL);
            if (status) {
                ALOGE("Error %d locking sprite surface before drawing.", status);
            } else {
                graphics::Paint paint;
                paint.setBlendMode(ABLEND_MODE_SRC);
                // 构建canvas
                graphics::Canvas canvas(outBuffer, (int32_t) surface->getBuffersDataSpace());
                // 绘制icon
                canvas.drawBitmap(update.state.icon.bitmap, 0, 0, &paint);
  
                const int iconWidth = update.state.icon.bitmap.getInfo().width;
                const int iconHeight = update.state.icon.bitmap.getInfo().height;
                // 省略若干行
                // unlock并提交绘制
                status = surface->unlockAndPost();
            }
        }
    }
}
```

绘制Sprite时，会遍历所有的需要更新的Sprite，然后获取到Surface并lock绑定buffer，接着构建Paint和canvas并将icon对应的bitmap绘制到canvas上，最后unlock并提交。

###### Sprite参数调整

这块主要是修改绘制icon的alpha、显示位置以及matrix和显示的layer，最后会根据是否显示分别调用show和hide方法来更新小白点的现实状态。

```cpp
void SpriteController::doUpdateSprites() {
    for (size_t i = 0; i < numSprites; i++) {
        // 取出需要更新的Sprite
        SpriteUpdate& update = updates.editItemAt(i);
        // 省略若干行
        // 更新alpha
        if (wantSurfaceVisibleAndDrawn
                && (becomingVisible || (update.state.dirty & DIRTY_ALPHA))) {
            t.setAlpha(update.state.surfaceControl,
                    update.state.alpha);
        }
        // 更新显示位置
        if (wantSurfaceVisibleAndDrawn
                && (becomingVisible || (update.state.dirty & (DIRTY_POSITION
                        | DIRTY_HOTSPOT)))) {
            t.setPosition(
                    update.state.surfaceControl,
                    update.state.positionX - update.state.icon.hotSpotX,
                    update.state.positionY - update.state.icon.hotSpotY);
        }
        // 更新matrix
        if (wantSurfaceVisibleAndDrawn
                && (becomingVisible
                        || (update.state.dirty & DIRTY_TRANSFORMATION_MATRIX))) {
            t.setMatrix(
                    update.state.surfaceControl,
                    update.state.transformationMatrix.dsdx,
                    update.state.transformationMatrix.dtdx,
                    update.state.transformationMatrix.dsdy,
                    update.state.transformationMatrix.dtdy);
        }
        // 设置layer
        int32_t surfaceLayer = mOverlayLayer + update.state.layer;
        if (wantSurfaceVisibleAndDrawn
                && (becomingVisible || (update.state.dirty & DIRTY_LAYER))) {
            t.setLayer(update.state.surfaceControl, surfaceLayer);
        }
  
        if (becomingVisible) {
            t.show(update.state.surfaceControl);
            // 显示icon
            update.state.surfaceVisible = true;
            update.surfaceChanged = surfaceChanged = true;
        } else if (becomingHidden) {
            t.hide(update.state.surfaceControl);
            // 隐藏icon
            update.state.surfaceVisible = false;
            update.surfaceChanged = surfaceChanged = true;
        }
        }
    }
}
```

首先遍历每一个Sprite，然后会依次修改alpha、position、matrix以及显示的layer，最后调用show或者hide来刷新触摸小白点的显示状态。

###### 更新SurfaceControl

触摸小白点绘制过程中如果surface发生改变，则会做相应的处理，主要就是将改变的信息同步到全局的mLocked中去。

```cpp
void SpriteController::doUpdateSprites() {
    // If any surfaces were changed, write back the new surface properties to the sprites.
    if (surfaceChanged) { // acquire lock
        AutoMutex _l(mLock);
        // 遍历每一个刷新的Sprite
        for (size_t i = 0; i < numSprites; i++) {
            const SpriteUpdate& update = updates.itemAt(i);
            // surface发生改变
            if (update.surfaceChanged) {
                // 将surface对应的信息同步到全局中lock中去
                update.sprite->setSurfaceLocked(update.state.surfaceControl,
                        update.state.surfaceWidth, update.state.surfaceHeight,
                        update.state.surfaceDrawn, update.state.surfaceVisible);
            }
        }
    } // release lock
}
  
inline void setSurfaceLocked(const sp<SurfaceControl>& surfaceControl,
        int32_t width, int32_t height, bool drawn, bool visible) {
    mLocked.state.surfaceControl = surfaceControl;
    mLocked.state.surfaceWidth = width;
    mLocked.state.surfaceHeight = height;
    mLocked.state.surfaceDrawn = drawn;
    mLocked.state.surfaceVisible = visible;
}
```

这里更新的信息其实是surface的一些参数，因为上面绘制过程会改变update的数组，这里主要是将surface变更的信息同步到全局的lock对象中去。

#### 小结

通过追踪触摸小白点的开关状态改变的处理过程，最终我们了解到触摸小白点的实现原理：其实就是在分发输入事件时，如果是touch事件，就会去通过构建Sprite，进而创建出Surface，并将对应的icon绘制到触摸事件发生的位置，从而在屏幕对应位置显示出小白点的效果。

### 通过Systrace看触摸小白点绘制过程中输入事件的传递

下面我们结果Systrace来查看界面点击时输入事件的传递流程，以下Systrace抓取时机为：在设置触摸小白点开关界面点击时抓取。

#### Systrace抓取

Systrace的抓取方法是通过安卓sdk工具包中的systrace.py的python脚本实现的，具体抓取方式如下：

![](//upload-images.jianshu.io/upload_images/26874665-3cb2c6686e20dcca.png?imageMogr2/auto-orient/strip|imageView2/2/w/575/format/webp)

Systrace抓取

通过以上方法我们就得到了触摸过程的Systrace文件，接着我们就可以使用Chrome浏览器来分析了。

#### Systrace的打开方式

打开Systrace我们可以使用Chrome浏览器，打开Chrome浏览器，然后再地址栏输入chrome：//tracing，接着将Systrace文件拖入即可，或者点击左上的load按钮选择抓取的Systrace文件即可。

#### Systrace上的InputReader

通过前面的介绍，我们已经知道，输入事件首先会在InputReader中进行处理。所以，我们去Systrace上找到InputReader，并查看其状态：

![](https://upload-images.jianshu.io/upload_images/26874665-251c23401575c8c1.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

Systrace上的InputReader

从Systrace能够看到在InputReader线程中，调用了notifyMotion方法和interceptMotionBeforeQueueing方法。

#### Systrace上的InputDispatcher

在InputDispatcher中，首先我们会执行到dispatchMotionLocked，然后会执行findTouchedWindowTargetsLocked查找焦点窗口，接着会通过dispatchEventLocked方法将输入事件朝焦点窗口分发。

![](https://upload-images.jianshu.io/upload_images/26874665-bb286ddea464f2b9.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

Systrace上的InputDispatcher

继续看dispatchEventLocked方法的调用过程，调用过程是如下这样的：

![](https://upload-images.jianshu.io/upload_images/26874665-f26fe3189b15c549.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

dispatchEventLocked方法调用过程

① prepareDispatchCycleLocked(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server), id=0x74b2f34)

② enqueueDispatchEntriesLocked(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server), id=0x74b2f34)

③ enqueueDispatchEntry(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server), dispatchMode=DISPATCH_AS_HOVER_EXIT)

④ enqueueDispatchEntry(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server), dispatchMode=DISPATCH_AS_OUTSIDE)

⑤  
enqueueDispatchEntry(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server), dispatchMode=DISPATCH_AS_HOVER_ENTER)

⑥ enqueueDispatchEntry(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server), dispatchMode=DISPATCH_AS_IS)

⑦ enqueueDispatchEntry(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server), dispatchMode=DISPATCH_AS_SLIPPERY_EXIT)

⑧ enqueueDispatchEntry(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server), dispatchMode=DISPATCH_AS_SLIPPERY_ENTER)

⑨ startDispatchCycleLocked(inputChannel=8621987 com.android.settings/com.android.settings.Settings$DevelopmentSettingsDashboardActivity (server))

通过查看上述过程，我们也能够看到先通过prepareDispatchCycleLocked方法处理，接着会调用分别处理不同的flag对应的event，请添加到分发队列中，最后调用startDispatchCycleLocked进行事件分发。

#### Systrace上的应用进程

这里的应用进程是Settings，所以这里在Systrace中找到Settings对应的进程，然后查看事件处理流程。

![](https://upload-images.jianshu.io/upload_images/26874665-0ce5ee45dc9072b2.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

Systrace上的应用进程

首先我们能够看到在InputEventReceiver中事件会到达①deliverInputEvent方法，接着依次会通过②EarlyPostImeInputStage、③NativePostImeInputStage以及④ViewPostImeInputStage，在ViewPostImeInputStage方法中会继续将事件进行分发，最终会到达View树上。

## 总结

安卓中的输入系统，占用了很大一部分，而且牵扯的模块也有很多，比如对各类外设的控制，以及窗口及视图层事件的分发处理等，均离不开输入系统的支持。本篇仅仅是冰山一角地介绍了输入事件从native层是如何从底层获取，然后又是如何向上层分发的过程，而且也仅仅是介绍了个大概，至于更具体的流程以及原理，还需大家仔细去研读安卓源码。另外，文中若存在某些方面描述有误，还请大家多多指正，感谢！

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/3afb58d2a96e  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。