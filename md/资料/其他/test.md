# 车载



View的事件拦截

事件从窗口到Activity

从上往下传递

可以设置拦截并进行处理

否者会传递到view的子控件进行判断

 

点击事件onTouch

Onclick 点击是有前提条件

可点击 并且

 



先看一下第一个条件，mOnTouchListener这个变量是在哪里赋值的呢？我们寻找之后在View里发现了如下方法：

Bingo！找到了，mOnTouchListener正是在setOnTouchListener方法里赋值的，也就是说只要我们给控件注册了touch事件，mOnTouchListener就一定被赋值了。



第二个条件(mViewFlags & ENABLED_MASK) == ENABLED是判断当前点击的控件是否是enable的，按钮默认都是enable的，因此这个条件恒定为true。

  

第三个条件就比较关键了，mOnTouchListener.onTouch(this, event)，其实也就是去回调控件注册touch事件时的onTouch方法。也就是说如果我们在onTouch方法里返回true，就会让这三个条件全部成立，从而整个方法直接返回true。如果我们在onTouch方法里返回false，就会再去执行onTouchEvent(event)方法。



 

现在我们可以结合前面的例子来分析一下了，首先在dispatchTouchEvent中最先执行的就是onTouch方法，因此onTouch肯定是要优先于onClick执行的，也是印证了刚刚的打印结果。而如果在onTouch方法里返回了true，就会让dispatchTouchEvent方法直接返回true，不会再继续往下执行。而打印结果也证实了如果onTouch返回true，onClick就不会再执行了。

 

```
	public boolean onTouchEvent(MotionEvent event) {  
	    final int viewFlags = mViewFlags;  
	    if ((viewFlags & ENABLED_MASK) == DISABLED) {  
	        // A disabled view that is clickable still consumes the touch  
	        // events, it just doesn't respond to them.  
	        return (((viewFlags & CLICKABLE) == CLICKABLE ||  
	                (viewFlags & LONG_CLICKABLE) == LONG_CLICKABLE));  
	    }  
	    if (mTouchDelegate != null) {  
	        if (mTouchDelegate.onTouchEvent(event)) {  
	            return true;  
	        }  
	    }  
	    if (((viewFlags & CLICKABLE) == CLICKABLE ||  
	            (viewFlags & LONG_CLICKABLE) == LONG_CLICKABLE)) {  
	        switch (event.getAction()) {  
	            case MotionEvent.ACTION_UP:  
	                boolean prepressed = (mPrivateFlags & PREPRESSED) != 0;  
	                if ((mPrivateFlags & PRESSED) != 0 || prepressed) {  
	                    // take focus if we don't have it already and we should in  
	                    // touch mode.  
	                    boolean focusTaken = false;  
	                    if (isFocusable() && isFocusableInTouchMode() && !isFocused()) {  
	                        focusTaken = requestFocus();  
	                    }  
	                    if (!mHasPerformedLongPress) {  
	                        // This is a tap, so remove the longpress check  
	                        removeLongPressCallback();  
	                        // Only perform take click actions if we were in the pressed state  
	                        if (!focusTaken) {  
	                            // Use a Runnable and post this rather than calling  
	                            // performClick directly. This lets other visual state  
	                            // of the view update before click actions start.  
	                            if (mPerformClick == null) {  
	                                mPerformClick = new PerformClick();  
	                            }  
	                            if (!post(mPerformClick)) {  
	                                performClick();  
	                            }  
	                        }  
	                    }  
	                    if (mUnsetPressedState == null) {  
	                        mUnsetPressedState = new UnsetPressedState();  
	                    }  
	                    if (prepressed) {  
	                        mPrivateFlags |= PRESSED;  
	                        refreshDrawableState();  
	                        postDelayed(mUnsetPressedState,  
	                                ViewConfiguration.getPressedStateDuration());  
	                    } else if (!post(mUnsetPressedState)) {  
	                        // If the post failed, unpress right now  
	                        mUnsetPressedState.run();  
	                    }  
	                    removeTapCallback();  
	                }  
	                break;  
	            case MotionEvent.ACTION_DOWN:  
	                if (mPendingCheckForTap == null) {  
	                    mPendingCheckForTap = new CheckForTap();  
	                }  
	                mPrivateFlags |= PREPRESSED;  
	                mHasPerformedLongPress = false;  
	                postDelayed(mPendingCheckForTap, ViewConfiguration.getTapTimeout());  
	                break;  
	            case MotionEvent.ACTION_CANCEL:  
	                mPrivateFlags &= ~PRESSED;  
	                refreshDrawableState();  
	                removeTapCallback();  
	                break;  
	            case MotionEvent.ACTION_MOVE:  
	                final int x = (int) event.getX();  
	                final int y = (int) event.getY();  
	                // Be lenient about moving outside of buttons  
	                int slop = mTouchSlop;  
	                if ((x < 0 - slop) || (x >= getWidth() + slop) ||  
	                        (y < 0 - slop) || (y >= getHeight() + slop)) {  
	                    // Outside button  
	                    removeTapCallback();  
	                    if ((mPrivateFlags & PRESSED) != 0) {  
	                        // Remove any future long press/tap checks  
	                        removeLongPressCallback();  
	                        // Need to switch from pressed to not pressed  
	                        mPrivateFlags &= ~PRESSED;  
	                        refreshDrawableState();  
	                    }  
	                }  
	                break;  
	        }  
	        return true;  
	    }  
	    return false;  
	}  

```

