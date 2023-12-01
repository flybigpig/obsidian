# windowSoftInputMode



## 1. stateUnspecified

`android:windowSoftInputMode="stateUnspecified"` 未指定：这是Activity的默认状态，系统根据界面元素决定是否弹出软键盘。 当界面包含EditText等获取了焦点的输入控件，不包含RecyclerView、ScrollView等可滚动控件时，默认不弹出软键盘。 当界面包含EditText等获取了焦点的输入控件，也有滚动需求的控件时，默认弹出软键盘。

## 2. stateUnchanged

`android:windowSoftInputMode="stateUnchanged"` 保持原有状态：本界面软键盘的状态和上游界面离开时软键盘状态一致。上游界面有本界面就展示软键盘，上游界面没有则本界面不展示软键盘。

## 3. stateVisible

`android:windowSoftInputMode="stateVisible"` 显示软键盘：不管是否有输入控件和焦点，首次进入该界面都强制显示软键盘。但当栈顶页面主动隐藏软键盘，出栈回到本界面时本界面不会显示软键盘。

## 4. stateHidden

`android:windowSoftInputMode="stateHidden"` 隐藏软键盘：不论上一个界面是否显示软键盘，首次进入到本界面时一律不显示，但点击输入框仍然会显示软键盘。当栈顶界面显示软键盘，出栈后本界面仍然会显示软键盘

## 5. stateAlwaysVisible

`android:windowSoftInputMode="stateAlwaysVisible"` 总是显示软键盘：和stateVisible类似，但不论什么情况回到本界面时都会显示软键盘。

## 6. stateAlwaysHidden

`android:windowSoftInputMode="stateAlwaysHidden"` 总是隐藏软键盘：和stateHidden类似，不论什么情况回到本界面都不会显示软键盘。

## 7. adjustUnspecified

`android:windowSoftInputMode="stateAlwaysVisible|adjustUnspecified"` 默认配置：界面有可滚动控件时，优先收缩可滚动控件，为软键盘和焦点输入框都显示出来。 当没有可滚动控件时，屏幕会隐藏部分顶部控件，软键盘会覆盖一部分底部控件，确保软键盘和焦点输入框都在屏幕中。

## 8. adjustResize

`android:windowSoftInputMode="stateAlwaysVisible|adjustResize"` 调整大小：当界面有可滚动控件时，和默认配置一样。 当没有可滚动控件时，系统会重新调整布局，在保证屏幕中能显示软键盘和焦点输入框的同时会压缩一部分控件，让尽可能多的控件显示出来。

## 9. adjustPan

`android:windowSoftInputMode="stateAlwaysVisible|adjustPan"` 移动布局：系统会移动decorview，保证屏幕中能显示显示软键盘和焦点输入框。

## 10. adjustNothing

`android:windowSoftInputMode="stateAlwaysVisible|adjustNothing"` 不调整布局，直接显示软键盘，有可能覆盖了软键盘输入框。



## 通过代码调整软键盘弹出和隐藏

### 弹出

```Java
InputMethodManager imm = (InputMethodManager) requireActivity().getSystemService(Context.INPUT_METHOD_SERVICE);
imm.showSoftInput(englishWordEt, InputMethodManager.SHOW_IMPLICIT);
复制代码
```

### 隐藏

```Java
InputMethodManager imm = (InputMethodManager) requireActivity().getSystemService(Context.INPUT_METHOD_SERVICE);
imm.hideSoftInputFromWindow(v.getWindowToken(),0);
```

