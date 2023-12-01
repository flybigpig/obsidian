# 导航栏 状态栏



```
//让虚拟键盘一直不显示
Window window = getWindow();
WindowManager.LayoutParams params = window.getAttributes();
params.systemUiVisibility = View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE;
window.setAttributes(params);
```

