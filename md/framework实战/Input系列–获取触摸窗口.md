

> hongxi.zhu 2023-7-25  
> Android 13

#### InputDispatcher::dispatchMotionLocked

```cpp
bool InputDispatcher::dispatchMotionLocked(nsecs_t currentTime, std::shared_ptr<MotionEntry> entry,
                                           DropReason* dropReason, nsecs_t* nextWakeupTime) {
   
	...
    if (isPointerEvent) {
   
        // Pointer event.  (eg. touchscreen)
        // 获取触摸目标窗口
        injectionResult =
                findTouchedWindowTargetsLocked(currentTime, *entry, inputTargets, nextWakeupTime,
                                               &conflictingPointerActions);
    } else {
   
        // Non touch event.  (eg. trackball)
		...
    }
    ...
    dispatchEventLocked(currentTime, entry, inputTargets);
    return true;
}
```

#### InputDispatcher::findTouchedWindowTargetsLocked

```cpp
InputEventInjectionResult InputDispatcher::findTouchedWindowTargetsLocked(
        nsecs_t currentTime, const MotionEntry& entry, std::vector<InputTarget>& inputTargets,
        nsecs_t* nextWakeupTime, bool* outConflictingPointerActions) {
   
	
	...
	
    if (newGesture) {
     //如果是down事件（说明是一个新的触摸行为）
        bool down = maskedAction == AMOTION_EVENT_ACTION_DOWN;
		
        tempTouchState.reset();
        tempTouchState.down = down;
        tempTouchState.deviceId = entry.deviceId;
        tempTouchState.source = entry.source;
        tempTouchState.displayId = displayId;
        isSplit = false;
    } else if (switchedDevice && maskedAction == AMOTION_EVENT_ACTION_MOVE) {
   
		...
    }

    if (newGesture || (isSplit && maskedAction == AMOTION_EVENT_ACTION_POINTER_DOWN)) {
   
    	// 如果是down事件
        /* Case 1: New splittable pointer going down, or need target for hover or scroll. */

        int32_t x;
        int32_t y;
        const int32_t pointerIndex = getMotionEventActionPointerIndex(action);
        // Always dispatch mouse events to cursor position.
        if (isFromMouse) {
   
            x = int32_t(entry.xCursorPosition);
            y = int32_t(entry.yCursorPosition);
        } else {
   
            x = int32_t(entry.pointerCoords[pointerIndex].getAxisValue(AMOTION_EVENT_AXIS_X));
            y = int32_t(entry.pointerCoords[pointerIndex].getAxisValue(AMOTION_EVENT_AXIS_Y));
        }
        const bool isDown = maskedAction == AMOTION_EVENT_ACTION_DOWN;
        const bool isStylus = isPointerFromStylus(entry, pointerIndex);
        newTouchedWindowHandle = findTouchedWindowAtLocked(displayId, x, y, &tempTouchState,
                                                           isStylus, isDown /*addOutsideTargets*/);

        // Handle the case where we did not find a window.
        if (newTouchedWindowHandle == nullptr) {
   
            ALOGD("No new touched window at (%" PRId32 ", %" PRId32 ") in display %" PRId32, x, y,
                  displayId);
            // Try to assign the pointer to the first foreground window we find, if there is one.
            newTouchedWindowHandle = tempTouchState.getFirstForegroundWindowHandle();
        }

        // Verify targeted injection.
        if (const auto err = verifyTargetedInjection(newTouchedWindowHandle, entry); err) {
   
            ALOGW("Dropping injected touch event: %s", (*err).c_str());
            injectionResult = os::InputEventInjectionResult::TARGET_MISMATCH;
            newTouchedWindowHandle = nullptr;
            goto Failed;
        }

        // Figure out whether splitting will be allowed for this window.
        if (newTouchedWindowHandle != nullptr) {
   
            if (newTouchedWindowHandle->getInfo()->supportsSplitTouch()) {
   
                // New window supports splitting, but we should never split mouse events.
                isSplit = !isFromMouse;
            } else if (isSplit) {
   
                // New window does not support splitting but we have already split events.
                // Ignore the new window.
                newTouchedWindowHandle = nullptr;
            }
        } else {
   
            // No window is touched, so set split to true. This will allow the next pointer down to
            // be delivered to a new window which supports split touch. Pointers from a mouse device
            // should never be split.
            tempTouchState.split = isSplit = !isFromMouse;
        }

        // Update hover state.
        if (newTouchedWindowHandle != nullptr) {
   
            if (maskedAction == AMOTION_EVENT_ACTION_HOVER_EXIT) {
   
                newHoverWindowHandle = nullptr;
            } else if (isHoverAction) {
   
                newHoverWindowHandle = newTouchedWindowHandle;
            }
        }

        std::vector<sp<WindowInfoHandle>> newTouchedWindows =
                findTouchedSpyWindowsAtLocked(displayId, x, y, isStylus);
        if (newTouchedWindowHandle != nullptr) {
   
            // Process the foreground window first so that it is the first to receive the event.
            newTouchedWindows.insert(newTouchedWindows.begin(), newTouchedWindowHandle);
        }

        if (newTouchedWindows.empty()) {
   
            ALOGI("Dropping event because there is no touchable window at (%d, %d) on display %d.",
                  x, y, displayId);
            injectionResult = InputEventInjectionResult::FAILED;
            goto Failed;
        }

        for (const sp<WindowInfoHandle>& windowHandle : newTouchedWindows) {
   
            const WindowInfo& info = *windowHandle->getInfo();

            // Skip spy window targets that are not valid for targeted injection.
            if (const auto err = verifyTargetedInjection(windowHandle, entry); err) {
   
                continue;
            }

            if (info.inputConfig.test(WindowInfo::InputConfig::PAUSE_DISPATCHING)) {
   
                ALOGI("Not sending touch event to %s because it is paused",
                      windowHandle->getName().c_str());
                continue;
            }

            // Ensure the window has a connection and the connection is responsive
            const bool isResponsive = hasResponsiveConnectionLocked(*windowHandle);
            if (!isResponsive) {
   
                ALOGW("Not sending touch gesture to %s because it is not responsive",
                      windowHandle->getName().c_str());
                continue;
            }

            // Drop events that can't be trusted due to occlusion
            if (mBlockUntrustedTouchesMode != BlockUntrustedTouchesMode::DISABLED) {
   
                TouchOcclusionInfo occlusionInfo =
                        computeTouchOcclusionInfoLocked(windowHandle, x, y);
                if (!isTouchTrustedLocked(occlusionInfo)) {
   
                    if (DEBUG_TOUCH_OCCLUSION) {
   
                        ALOGD("Stack of obscuring windows during untrusted touch (%d, %d):", x, y);
                        for (const auto& log : occlusionInfo.debugInfo) {
   
                            ALOGD("%s", log.c_str());
                        }
                    }
                    sendUntrustedTouchCommandLocked(occlusionInfo.obscuringPackage);
                    if (mBlockUntrustedTouchesMode == BlockUntrustedTouchesMode::BLOCK) {
   
                        ALOGW("Dropping untrusted touch event due to %s/%d",
                              occlusionInfo.obscuringPackage.c_str(), occlusionInfo.obscuringUid);
                        continue;
                    }
                }
            }

            // Drop touch events if requested by input feature
            if (shouldDropInput(entry, windowHandle)) {
   
                continue;
            }

            // Set target flags.
            int32_t targetFlags = InputTarget::FLAG_DISPATCH_AS_IS;

            if (canReceiveForegroundTouches(*windowHandle->getInfo())) {
   
                // There should only be one touched window that can be "foreground" for the pointer.
                targetFlags |= InputTarget::FLAG_FOREGROUND;
            }

            if (isSplit) {
   
                targetFlags |= InputTarget::FLAG_SPLIT;
            }
            if (isWindowObscuredAtPointLocked(windowHandle, x, y)) {
   
                targetFlags |= InputTarget::FLAG_WINDOW_IS_OBSCURED;
            } else if (isWindowObscuredLocked(windowHandle)) {
   
                targetFlags |= InputTarget::FLAG_WINDOW_IS_PARTIALLY_OBSCURED;
            }

            // Update the temporary touch state.
            BitSet32 pointerIds;
            if (isSplit) {
   
                uint32_t pointerId = entry.pointerProperties[pointerIndex].id;
                pointerIds.markBit(pointerId);
            }

            tempTouchState.addOrUpdateWindow(windowHandle, targetFlags, pointerIds);
        }
    } else {
   
        /* Case 2: Pointer move, up, cancel or non-splittable pointer down. */

        // If the pointer is not currently down, then ignore the event.
        if (!tempTouchState.down) {
   
            if (DEBUG_FOCUS) {
   
                ALOGD("Dropping event because the pointer is not down or we previously "
                      "dropped the pointer down event in display %" PRId32,
                      displayId);
            }
            injectionResult = InputEventInjectionResult::FAILED;
            goto Failed;
        }

        addDragEventLocked
```