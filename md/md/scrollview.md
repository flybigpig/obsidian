## ScrollView

### pageScroll  &  fullScroll

- ```
  
  
  /**
       * <p>Handles scrolling in response to a "page up/down" shortcut press. This
       * method will scroll the view by one page up or down and give the focus
       * to the topmost/bottommost component in the new visible area. If no
       * component is a good candidate for focus, this scrollview reclaims the
       * focus.</p>
       *
       * @param direction the scroll direction: {@link android.view.View#FOCUS_UP}
       *                  to go one page up or
       *                  {@link android.view.View#FOCUS_DOWN} to go one page down
       * @return true if the key event is consumed by this method, false otherwise
       */
       // 向某个方向滑动一页
      public boolean pageScroll(int direction) {
          boolean down = direction == View.FOCUS_DOWN;
          int height = getHeight();
  
          if (down) {
              mTempRect.top = getScrollY() + height;
              int count = getChildCount();
              if (count > 0) {
                  View view = getChildAt(count - 1);
                  if (mTempRect.top + height > view.getBottom()) {
                      mTempRect.top = view.getBottom() - height;
                  }
              }
          } else {
              mTempRect.top = getScrollY() - height;
              if (mTempRect.top < 0) {
                  mTempRect.top = 0;
              }
          }
          mTempRect.bottom = mTempRect.top + height;
  
          return scrollAndFocus(direction, mTempRect.top, mTempRect.bottom);
      }
  
      /**
       * <p>Handles scrolling in response to a "home/end" shortcut press. This
       * method will scroll the view to the top or bottom and give the focus
       * to the topmost/bottommost component in the new visible area. If no
       * component is a good candidate for focus, this scrollview reclaims the
       * focus.</p>
       *
       * @param direction the scroll direction: {@link android.view.View#FOCUS_UP}
       *                  to go the top of the view or
       *                  {@link android.view.View#FOCUS_DOWN} to go the bottom
       * @return true if the key event is consumed by this method, false otherwise
       */
       // 底部或者顶部  oppo 手机出现问题 会跳到顶部。建议
        // scrollView.scroolto(0,view.getMeasuredHeight())
       
      public boolean fullScroll(int direction) {
          boolean down = direction == View.FOCUS_DOWN;
          int height = getHeight();
  
          mTempRect.top = 0;
          mTempRect.bottom = height;
  
          if (down) {
              int count = getChildCount();
              if (count > 0) {
                  View view = getChildAt(count - 1);
                  mTempRect.bottom = view.getBottom() + mPaddingBottom;
                  mTempRect.top = mTempRect.bottom - height;
              }
          }
  
          return scrollAndFocus(direction, mTempRect.top, mTempRect.bottom);
      }
  
      /**
       * <p>Scrolls the view to make the area defined by <code>top</code> and
       * <code>bottom</code> visible. This method attempts to give the focus
       * to a component visible in this area. If no component can be focused in
       * the new visible area, the focus is reclaimed by this ScrollView.</p>
       *
       * @param direction the scroll direction: {@link android.view.View#FOCUS_UP}
       *                  to go upward, {@link android.view.View#FOCUS_DOWN} to downward
       * @param top       the top offset of the new area to be made visible
       * @param bottom    the bottom offset of the new area to be made visible
       * @return true if the key event is consumed by this method, false otherwise
       */
      private boolean scrollAndFocus(int direction, int top, int bottom) {
          boolean handled = true;
  
          int height = getHeight();
          int containerTop = getScrollY();
          int containerBottom = containerTop + height;
          boolean up = direction == View.FOCUS_UP;
  
          View newFocused = findFocusableViewInBounds(up, top, bottom);
          if (newFocused == null) {
              newFocused = this;
          }
  
          if (top >= containerTop && bottom <= containerBottom) {
              handled = false;
          } else {
              int delta = up ? (top - containerTop) : (bottom - containerBottom);
              doScrollY(delta);
          }
  
          if (newFocused != findFocus()) newFocused.requestFocus(direction);
  
          return handled;
      }
  ```

