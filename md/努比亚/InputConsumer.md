
consume方法中主要是从InputChannel获取输入事件的信息，然后根据消息中获取的事件类型构造出对应的event，并将消息中的事件信息赋值给event对象

```
status_t result = mChannel->receiveMessage(&mMsg);
```

```
status_t InputConsumer::consume(InputEventFactoryInterface* factory,  
        bool consumeBatches, nsecs_t frameTime, uint32_t* outSeq, InputEvent** outEvent) {  
#if DEBUG_TRANSPORT_ACTIONS  
    ALOGD("channel '%s' consumer ~ consume: consumeBatches=%s, frameTime=%" PRId64,  
            mChannel->getName().c_str(), consumeBatches ? "true" : "false", frameTime);  
#endif  
  
    *outSeq = 0;  
    *outEvent = nullptr;  
  
    // Fetch the next input message.  
    // Loop until an event can be returned or no additional events are received.    while (!*outEvent) {  
        if (mMsgDeferred) {  
            // mMsg contains a valid input message from the previous call to consume  
            // that has not yet been processed.            mMsgDeferred = false;  
        } else {  
            // Receive a fresh message.  
            status_t result = mChannel->receiveMessage(&mMsg);
            if (result) {  
                // Consume the next batched event unless batches are being held for later.  
                if (consumeBatches || result != WOULD_BLOCK) {  
                    result = consumeBatch(factory, frameTime, outSeq, outEvent);  
                    if (*outEvent) {  
#if DEBUG_TRANSPORT_ACTIONS  
                        ALOGD("channel '%s' consumer ~ consumed batch event, seq=%u",  
                                mChannel->getName().c_str(), *outSeq);  
#endif  
                        break;  
                    }  
                }  
                return result;  
            }  
        }  
  
        switch (mMsg.header.type) {  
        case InputMessage::TYPE_KEY: {  
            KeyEvent* keyEvent = factory->createKeyEvent();  
            if (!keyEvent) return NO_MEMORY;  
  
            initializeKeyEvent(keyEvent, &mMsg);  
            *outSeq = mMsg.body.key.seq;  
            *outEvent = keyEvent;  
#if DEBUG_TRANSPORT_ACTIONS  
            ALOGD("channel '%s' consumer ~ consumed key event, seq=%u",  
                    mChannel->getName().c_str(), *outSeq);  
#endif  
            break;  
        }  
  
        case InputMessage::TYPE_MOTION: {  
            ssize_t batchIndex = findBatch(mMsg.body.motion.deviceId, mMsg.body.motion.source);  
            if (batchIndex >= 0) {  
                Batch& batch = mBatches.editItemAt(batchIndex);  
                if (canAddSample(batch, &mMsg)) {  
                    batch.samples.push(mMsg);  
#if DEBUG_TRANSPORT_ACTIONS  
                    ALOGD("channel '%s' consumer ~ appended to batch event",  
                            mChannel->getName().c_str());  
#endif  
                    break;  
                } else if (isPointerEvent(mMsg.body.motion.source) &&  
                        mMsg.body.motion.action == AMOTION_EVENT_ACTION_CANCEL) {  
                    // No need to process events that we are going to cancel anyways  
                    const size_t count = batch.samples.size();  
                    for (size_t i = 0; i < count; i++) {  
                        const InputMessage& msg = batch.samples.itemAt(i);  
                        sendFinishedSignal(msg.body.motion.seq, false);  
                    }  
                    batch.samples.removeItemsAt(0, count);  
                    mBatches.removeAt(batchIndex);  
                } else {  
                    // We cannot append to the batch in progress, so we need to consume  
                    // the previous batch right now and defer the new message until later.                    mMsgDeferred = true;  
                    status_t result = consumeSamples(factory,  
                            batch, batch.samples.size(), outSeq, outEvent);  
                    mBatches.removeAt(batchIndex);  
                    if (result) {  
                        return result;  
                    }  
#if DEBUG_TRANSPORT_ACTIONS  
                    ALOGD("channel '%s' consumer ~ consumed batch event and "  
                            "deferred current event, seq=%u",  
                            mChannel->getName().c_str(), *outSeq);  
#endif  
                    break;  
                }  
            }  
  
            // Start a new batch if needed.  
            if (mMsg.body.motion.action == AMOTION_EVENT_ACTION_MOVE  
                    || mMsg.body.motion.action == AMOTION_EVENT_ACTION_HOVER_MOVE) {  
                mBatches.push();  
                Batch& batch = mBatches.editTop();  
                batch.samples.push(mMsg);  
#if DEBUG_TRANSPORT_ACTIONS  
                ALOGD("channel '%s' consumer ~ started batch event",  
                        mChannel->getName().c_str());  
#endif  
                break;  
            }  
  
            MotionEvent* motionEvent = factory->createMotionEvent();  
            if (! motionEvent) return NO_MEMORY;  
  
            updateTouchState(mMsg);  
            initializeMotionEvent(motionEvent, &mMsg);  
            *outSeq = mMsg.body.motion.seq;  
            *outEvent = motionEvent;  
  
#if DEBUG_TRANSPORT_ACTIONS  
            ALOGD("channel '%s' consumer ~ consumed motion event, seq=%u",  
                    mChannel->getName().c_str(), *outSeq);  
#endif  
            break;  
        }  
  
        default:  
            ALOGE("channel '%s' consumer ~ Received unexpected message of type %d",  
                    mChannel->getName().c_str(), mMsg.header.type);  
            return UNKNOWN_ERROR;  
        }  
    }  
    return OK;  
}

```