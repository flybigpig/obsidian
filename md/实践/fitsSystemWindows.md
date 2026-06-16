The user is asking about `fitsSystemWindows`. Let me explain what this attribute does, how it works, and how it relates to their current issue of the bottom content being obscured by the navigation bar.
## `fitsSystemWindows` 详解

### 它是什么

`android:fitsSystemWindows="true"` 是一个 View 属性，告诉系统："请自动为这个 View 添加 padding，以避开系统栏（状态栏 + 导航栏）"。

### 工作原理

```
┌──────────────────────┐
│    状态栏 (24~48dp)   │  ← 系统栏区域
├──────────────────────┤
│                      │
│   你的内容区域        │  ← fitsSystemWindows=true 会自动加 paddingTop
│                      │
│                      │
├──────────────────────┤
│  导航栏/手势条(0~48dp)│  ← 系统栏区域
└──────────────────────┘
                       ← fitsSystemWindows=true 会自动加 paddingBottom
```

设置了 `fitsSystemWindows="true"` 后，系统会：
1. 检测当前窗口的系统栏 insets（状态栏顶部高度 + 导航栏底部高度）
2. 自动给该 View 设置 `paddingTop = 状态栏高度`，`paddingBottom = 导航栏高度`
3. **覆盖你手动设置的 padding**

---

### 对你项目的影响

你的 `BaseDbFragment` 中对 `HomeFragment` 做了：

```116:127:app/src/main/java/com/yto/customermanmagererp/base/BaseDbFragment.kt
                if (isImmersiveFragment) {
                    // 沉浸式布局：允许内容延伸到状态栏区域
                    WindowCompat.setDecorFitsSystemWindows(act.window, false)
                    // 状态栏背景透明
                    act.window.statusBarColor = Color.TRANSPARENT
```

`WindowCompat.setDecorFitsSystemWindows(window, false)` = 告诉系统"不要自动适配系统栏"，内容可以延伸到系统栏下方。

这就导致：**内容延伸到导航栏下方，底部被遮挡**。

---

### 为什么不能直接在根布局加 `fitsSystemWindows="true"`

如果直接给根 `RelativeLayout` 加 `android:fitsSystemWindows="true"`：

| 效果 | 说明 |
|------|------|
| 底部不被遮挡了 | paddingBottom = 导航栏高度，内容可见 |
| 顶部也加了 padding | paddingTop = 状态栏高度，**背景图被顶下去**，状态栏区域变成空白 |

这与你的沉浸式设计（背景图通铺到状态栏）**冲突**。

---

### 正确做法：只处理底部，不影响顶部

#### 方案 A：给根布局加 fitsSystemWindows，但手动处理顶部 padding

```xml
<RelativeLayout
    android:id="@+id/ll_root"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:fitsSystemWindows="true">
```

然后在 `HomeFragment.kt` 中，手动把顶部 padding 清零，只保留底部：

```kotlin
ViewCompat.setOnApplyWindowInsetsListener(ll_root) { v, insets ->
    val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
    // 顶部 padding 设为 0，保持沉浸式；底部保留导航栏高度
    v.setPadding(0, 0, 0, systemBars.bottom)
    insets
}
```

> `setOnApplyWindowInsetsListener` 会**接管** `fitsSystemWindows` 的默认行为，让你完全控制 padding 值。

#### 方案 B：不加 fitsSystemWindows，纯用代码控制（推荐）

布局不变，只加代码：

```kotlin
// HomeFragment.kt initView() 末尾
ViewCompat.setOnApplyWindowInsetsListener(ll_root) { v, insets ->
    val navBarHeight = insets.getInsets(WindowInsetsCompat.Type.systemBars()).bottom
    v.setPadding(0, 0, 0, navBarHeight)
    insets
}
```

---

### 总结对比

| 方式 | 顶部 padding | 底部 padding | 是否适配所有机型 |
|------|-------------|-------------|----------------|
| `fitsSystemWindows="true"` | 自动加（破坏沉浸式） | 自动加 | 是 |
| `fitsSystemWindows="true"` + 代码清顶部 | 代码设为 0 | 自动加 | 是 |
| 纯代码 `setOnApplyWindowInsetsListener` | 不加（保持沉浸式） | 代码加 | 是 |
| 硬编码 `paddingBottom="48dp"` | 不加 | 固定 48dp | 否 |

**推荐方案 B（纯代码）**，最简洁、最精确，不依赖 `fitsSystemWindows` 的隐式行为。
