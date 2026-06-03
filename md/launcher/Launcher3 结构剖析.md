


# Android 12 Launcher3 结构剖析

**作者：** Iraqis 憔悴 / 码上就好

**从目录结构到核心架构：全面拆解 Android 桌面应用的内部世界**

Launcher3 是 Android 系统中最复杂的系统应用之一——它只有一个 Activity，但这个 Activity 的复杂度远超大多数应用。它承载着桌面布局管理、应用抽屉、拖拽排序、Widget 容器、手势导航等核心功能。本文将从目录结构、核心类、视图层级、数据模型、MVC 架构五个维度，系统性地剖析 Launcher3 的内部结构，帮助 Framework 新手建立完整的认知地图。

---

## 目录

- 一、Launcher3 在 AOSP 中的定位
- 二、源码目录结构全景
- 三、核心包与关键类一览
- 四、视图层级结构：从 DragLayer 到 BubbleTextView
- 五、数据模型层：LauncherProvider 与 LauncherSettings
- 六、MVC 架构：Launcher、LauncherModel、LoaderTask
- 七、拖拽系统：DragController 与 DropTarget
- 八、Quickstep 模块与手势导航
- 九、实战：编译与调试 Launcher3
- 十、架构全景图与调试技巧

---

## 一、Launcher3 在 AOSP 中的定位

### 1.1 什么是 Launcher？

Launcher（桌面启动器）是 Android 系统中的 **Home 应用**。当用户按下 Home 键或系统开机完成后，ActivityManagerService 会启动具有 `CATEGORY_HOME` 的 Activity，即 Launcher。

Launcher3 是 AOSP 中的默认桌面实现，位于 `packages/apps/Launcher3`。它是众多第三方桌面应用（如 Pixel Launcher、Nova Launcher）的基础代码库。

### 1.2 Launcher3 的特殊性

| 特点 | 说明 |
|------|------|
| 单 Activity 架构 | 整个应用只有一个 Launcher Activity，所有 UI 状态通过 View 切换实现 |
| 复杂的触摸事件 | 桌面滑动、图标拖拽、手势导航共存，触摸事件分发极其复杂 |
| 多进程交互 | 与 SystemUI（手势导航）、AMS（任务管理）、PMS（应用信息）深度交互 |
| 系统级权限 | 作为 privapp，拥有 MANAGE_ACTIVITY_STACKS 等特殊权限 |
| Quickstep 集成 | Android 10+ 将最近任务（Recents）集成到 Launcher3 中 |

---

## 二、源码目录结构全景

### 2.1 顶层目录

```
packages/apps/Launcher3/
├── src/                    // 主源码目录
├── quickstep/              // 手势导航 & 最近任务模块
├── go/                     // Go Edition 精简版
├── res/                    // 资源文件（布局/图标/配置）
├── iconloaderlib/          // 图标加载独立库
├── tests/                  // 测试代码
├── AndroidManifest.xml     // 清单文件
├── Android.bp              // 构建配置
└── proguard.flags          // 混淆配置
```

### 2.2 src 主源码包结构

```
com/android/launcher3/
├── Launcher.java                    // 核心 Activity
├── Workspace.java                   // 桌面工作区（多页滑动）
├── LauncherAppState.java            // 全局单例
├── LauncherModel.java               // 数据模型层
├── LauncherProvider.java            // ContentProvider 数据持久化
├── LauncherSettings.java            // 数据库 Contract 定义
├── IconCache.java                   // 图标缓存
├── BubbleTextView.java              // 图标+文字的视图控件
├── CellLayout.java                  // 网格布局（桌面每一页）
├── Hotseat.java                     // 底部快捷栏
│
├── allapps/                         // 应用抽屉
│   ├── AllAppsContainerView.java
│   ├── AllAppsStore.java
│   └── AlphabeticalAppsList.java
│
├── dragndrop/                       // 拖拽系统
│   ├── DragController.java
│   ├── DragLayer.java
│   └── DragView.java
│
├── folder/                          // 文件夹
│   ├── Folder.java
│   └── FolderIcon.java
│
├── widget/                          // Widget 管理
│   ├── WidgetCell.java
│   └── WidgetsContainerView.java
│
├── model/                           // 数据模型
│   ├── LoaderTask.java
│   ├── BgDataModel.java
│   └── ItemInfo.java
│
├── icons/                           // 图标处理（iconloaderlib）
│   ├── BitmapInfo.java
│   └── BaseIconCache.java
│
├── touch/                           // 触摸事件处理
├── anim/                            // 动画
├── graphics/                        // 图形绘制
├── logging/                         // 日志统计
└── util/                            // 工具类
```

---

## 三、核心包与关键类一览

| 包名 | 核心类 | 职责说明 |
|------|--------|---------|
| 根包 | Launcher | 唯一的 Activity，桌面所有 UI 状态的入口 |
| 根包 | Workspace | 桌面工作区，多页 CellLayout 的容器，支持左右滑动 |
| 根包 | CellLayout | 网格布局，桌面每一页的容器，管理图标/Widget 的位置 |
| 根包 | Hotseat | 底部常驻快捷栏（Dock 栏） |
| 根包 | LauncherAppState | 全局单例，初始化 IconCache、LauncherModel 等 |
| allapps | AllAppsContainerView | 应用抽屉容器，显示所有已安装应用 |
| dragndrop | DragLayer / DragController | 拖拽系统的视图层和控制器 |
| folder | Folder / FolderIcon | 文件夹的展开视图和图标视图 |
| model | LauncherModel / LoaderTask | 数据模型层，异步加载桌面数据 |
| widget | WidgetsContainerView | Widget 选择器和管理 |

---

## 四、视图层级结构：从 DragLayer 到 BubbleTextView

### 4.1 视图树全景

Launcher3 的整个 UI 构建在一棵以 `DragLayer` 为根的视图树上。DragLayer 是一个自定义 FrameLayout，负责拦截和分发拖拽相关的触摸事件。

```
DragLayer (FrameLayout - 根视图)
├── Workspace (PagedView - 桌面工作区)
│   ├── CellLayout[Page 0] (网格布局)
│   │   ├── ShortcutAndWidgetContainer
│   │   │   ├── BubbleTextView (应用图标)
│   │   │   ├── BubbleTextView (应用图标)
│   │   │   ├── FolderIcon (文件夹图标)
│   │   │   └── LauncherAppWidgetHostView (Widget)
│   ├── CellLayout[Page 1]
│   └── CellLayout[Page N]
│
├── Hotseat (底部 Dock 栏)
│   └── CellLayout (单行网格)
│       └── BubbleTextView * N
│
├── AllAppsContainerView (应用抽屉)
│   ├── SearchInput (搜索栏)
│   └── RecyclerView (应用列表)
│       └── BubbleTextView * N
│
├── Folder (展开的文件夹，浮层)
├── DragView (拖拽时的跟随视图)
└── ScrimView (半透明遮罩层)
```

### 4.2 关键视图解读

- **DragLayer**：最顶层容器，拦截所有触摸事件并判断是否为拖拽操作。所有浮层视图（DragView、Folder、Scrim）都添加在这一层。
- **Workspace**：继承自 `PagedView`（分页滑动容器），每一页是一个 CellLayout。它同时实现了 `DropTarget`、`DragSource` 等多个接口。
- **CellLayout**：网格布局，将桌面划分为若干行列的单元格。每个图标或 Widget 占据一个或多个单元格。内部使用 `ShortcutAndWidgetContainer` 来管理子视图的绝对定位。
- **BubbleTextView**：继承自 TextView，是所有桌面图标的基础视图。通过 `setCompoundDrawables()` 将图标 Drawable 放在文字上方。

---

## 五、数据模型层：LauncherProvider 与 LauncherSettings

### 5.1 数据持久化架构

Launcher3 使用 `LauncherProvider`（ContentProvider）+ `LauncherSettings`（Contract 类）实现数据持久化。所有桌面元素（快捷方式、文件夹、Widget）都存储在 SQLite 数据库 `launcher.db` 中。

### 5.2 核心数据表

| 表名 | 存储内容 | 对应 Contract |
|------|---------|--------------|
| favorites | 桌面快捷方式、文件夹、Widget | LauncherSettings.Favorites |
| workspaceScreens | 桌面页面的顺序和 ID | LauncherSettings.WorkspaceScreens |

### 5.3 favorites 表关键字段

```java
// LauncherSettings.Favorites 核心字段
_id              // 主键
title            // 显示名称
intent           // 启动 Intent（序列化字符串）
container        // 所在容器（桌面/Hotseat/文件夹ID）
screen           // 所在桌面页面编号
cellX            // 网格列位置
cellY            // 网格行位置
spanX            // 占据列数（Widget 可 > 1）
spanY            // 占据行数
itemType         // 类型：APP/SHORTCUT/FOLDER/APPWIDGET
appWidgetId      // Widget 实例 ID
profileId        // 用户 Profile（工作资料等）
```

### 5.4 ItemInfo 数据模型继承体系

数据库中的每条记录对应一个 `ItemInfo` 对象：

```
ItemInfo                          // 基类：id, title, container, screen, cellX/Y
├── WorkspaceItemInfo             // 快捷方式（intent + icon）
├── AppInfo                       // 应用信息（AllApps 列表中的条目）
├── FolderInfo                    // 文件夹（包含子 ItemInfo 列表）
└── LauncherAppWidgetInfo         // Widget 信息（appWidgetId + provider）
```

---

## 六、MVC 架构：Launcher、LauncherModel、LoaderTask

### 6.1 整体架构模式

Launcher3 采用类 MVC 架构：

| 层次 | 角色 | 核心类 | 职责 |
|------|------|--------|------|
| View | 视图层 | Launcher + 各视图类 | UI 展示、用户交互响应 |
| Model | 模型层 | LauncherModel + BgDataModel | 数据加载、缓存、变更通知 |
| Controller | 控制器 | LoaderTask + Callbacks | 异步加载调度、数据绑定 |
| Provider | 持久层 | LauncherProvider + SQLite | 数据库读写、备份恢复 |

### 6.2 LauncherAppState 全局初始化

```java
// LauncherAppState.java - 单例初始化
public class LauncherAppState {
    private final LauncherModel mModel;
    private final IconCache mIconCache;
    private final InvariantDeviceProfile mInvariantDeviceProfile;

    public LauncherAppState(Context context) {
        // 1. 设备配置（网格行列数、图标大小等）
        mInvariantDeviceProfile = new InvariantDeviceProfile(context);
        
        // 2. 图标缓存
        mIconCache = new IconCache(context, mInvariantDeviceProfile);
        
        // 3. 数据模型（注册包变更监听）
        mModel = new LauncherModel(context, this,
                mIconCache, new AppFilter());
    }
}
```

### 6.3 数据加载与绑定流程

LoaderTask 在工作线程中按顺序执行三个阶段：

1. **loadWorkspace()**：从 LauncherProvider 数据库读取桌面布局
2. **loadAllApps()**：通过 LauncherApps 查询所有应用，更新 IconCache
3. **loadWidgets()**：查询系统中所有可用的 AppWidget

每个阶段完成后，通过 `Callbacks` 接口（Launcher 实现）将数据绑定到 UI。

---

## 七、拖拽系统：DragController 与 DropTarget

### 7.1 拖拽系统核心角色

| 角色 | 接口/类 | 实现者 |
|------|--------|--------|
| 拖拽源 | DragSource | Workspace、Folder、AllAppsContainerView |
| 放置目标 | DropTarget | Workspace、Hotseat、Folder、DeleteDropTarget |
| 拖拽控制器 | DragController | 协调整个拖拽生命周期 |
| 拖拽视图 | DragView | 跟随手指移动的图标副本 |
| 拖拽容器 | DragLayer | 拦截触摸事件，承载 DragView |

### 7.2 拖拽生命周期

```java
// 拖拽生命周期回调
interface DropTarget {
    // 拖拽物进入区域
    void onDragEnter(DragObject d);
    
    // 拖拽物在区域内移动
    void onDragOver(DragObject d);
    
    // 拖拽物离开区域
    void onDragExit(DragObject d);
    
    // 拖拽物放下（松手）
    void onDrop(DragObject d);
    
    // 检测是否接受此次放置
    boolean acceptDrop(DragObject d);
}
```

**提示：** 拖拽图标到两个图标重叠时会自动创建文件夹——这是通过 Workspace.onDrop() 中检测 cellX/cellY 重叠并调用 FolderIcon.create() 实现的。

---

## 八、Quickstep 模块与手势导航

### 8.1 Quickstep 的定位

从 Android 10 开始，Google 将 **最近任务（Recents）** 功能从 SystemUI 迁移到 Launcher3 中，形成了 `quickstep` 模块。这使得桌面上滑手势能无缝过渡到最近任务界面。

```
quickstep/
├── src/com/android/quickstep/
│   ├── RecentsActivity.java          // 独立 Recents Activity
│   ├── TouchInteractionService.java  // 手势监听服务
│   ├── OverviewProxyService.java     // SystemUI ↔ Launcher 桥接
│   ├── RecentsView.java              // 最近任务视图
│   ├── TaskView.java                 // 单个任务卡片
│   └── SwipeUpAnimationLogic.java    // 上滑手势动画
└── res/
```

### 8.2 SystemUI 与 Launcher3 的桥接

Quickstep 通过 AIDL 接口实现 SystemUI 和 Launcher3 之间的跨进程通信：

- **IOverviewProxy**：SystemUI → Launcher（通知手势事件）
- **ISystemUiProxy**：Launcher → SystemUI（请求截图、分屏等）

---

## 九、实战：编译与调试 Launcher3

### 9.1 AOSP 中编译 Launcher3

```bash
# 初始化编译环境
source build/envsetup.sh
lunch aosp_x86_64-userdebug

# 单模块编译 Launcher3
mmm packages/apps/Launcher3

# 编译带 Quickstep 的版本
mmm packages/apps/Launcher3:Launcher3QuickStep

# 推送安装到设备
adb install -r out/target/product/*/system/product/priv-app/Launcher3QuickStep/Launcher3QuickStep.apk
```

### 9.2 Android Studio 中调试

社区已有多个项目支持在 Android Studio 中直接编译运行 Launcher3。关键步骤：

1. 从 AOSP 提取 Launcher3 源码和依赖库（iconloaderlib、SharedLibWrapper 等）
2. 配置 Gradle 构建脚本，处理系统 API 依赖
3. 通过 `@hide` API 的反射或 systemApi stubs 解决编译问题

### 9.3 常用调试命令

```bash
# 查看 Launcher 相关日志
adb logcat -s Launcher LauncherModel LoaderTask IconCache

# 查看当前桌面数据库内容
adb shell content query \
    --uri content://com.android.launcher3.settings/favorites

# 强制重新加载桌面
adb shell am force-stop com.android.launcher3

# 查看 Launcher 的 View 层级（需要 userdebug）
adb shell dumpsys activity top | head -100

# 查看桌面网格配置
adb shell dumpsys activity provider \
    com.android.launcher3.settings

# 使用 Layout Inspector（Android Studio）
# Tools → Layout Inspector → 选择 Launcher 进程
```

---

## 十、架构全景图与调试技巧

### 10.1 Launcher3 架构全景图

```
View 层（UI）
┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┐
│  DragLayer  │  Workspace  │  AllApps    │  Hotseat    │ RecentsView │
└──────┬──────┴──────┬──────┴──────┬──────┴──────┬──────┴──────┬──────┘
       │             │             │             │             │
       └─────────────┴─────────────┴─────────────┴─────────────┘
                           ↓ Callbacks 绑定 ↓
Controller 层（控制器）
┌──────────────┬──────────────┬──────────────┬──────────────────┐
│Launcher      │DragController│ StateManager │ TouchController  │
│Activity      │              │              │                  │
└──────┬───────┴──────┬───────┴──────┬───────┴────────┬─────────┘
       │              │              │                │
       └──────────────┴──────────────┴────────────────┘
                           ↓ 数据加载 ↓
Model 层（数据模型）
┌──────────────┬──────────────┬──────────────┬──────────────┐
│LauncherModel │  LoaderTask  │  BgDataModel │   IconCache  │
└──────┬───────┴──────┬───────┴──────┬───────┴──────┬───────┘
       │              │              │              │
       └──────────────┴──────────────┴──────────────┘
                           ↓ 数据持久化 ↓
Provider 层（持久化）
┌──────────────┬──────────────────┬──────────────────────┐
│Launcher      │  launcher.db     │     app_icons.db     │
│Provider      │  (SQLite)        │                      │
└──────┬───────┴──────┬───────────┴──────────┬───────────┘
       │              │                      │
       └──────────────┴──────────────────────┘
                           ↓ 系统交互 ↓
Framework 层（系统服务）
┌──────────────┬──────────────┬──────────────┬──────────────┐
│ LauncherApps │PackageManager│AppWidgetManager│    ATMS     │
└──────────────┴──────────────┴──────────────┴──────────────┘
```

### 10.2 OEM 定制常见切入点

| 定制需求 | 切入类/文件 | 说明 |
|---------|-----------|------|
| 修改桌面网格 | InvariantDeviceProfile | 行列数、图标大小、字体大小 |
| 图标形状/主题 | IconShape / LauncherIcons | 遮罩路径、图标缩放、阴影 |
| 默认桌面布局 | default_workspace.xml | 预置应用、文件夹、Widget |
| 手势交互 | TouchController 子类 | 自定义上滑/下拉/长按行为 |
| 搜索栏定制 | QsbContainerView | 替换 Google 搜索栏 |
| 最近任务 | RecentsView / TaskView | 任务卡片样式和交互 |

---

## 核心要点

1. Launcher3 是单 Activity 架构，所有 UI 状态通过 View 切换实现，DragLayer 是根视图
2. 源码分为 src（主逻辑）、quickstep（手势导航）、iconloaderlib（图标加载）三大模块
3. 视图层级：DragLayer → Workspace(CellLayout\*N) + Hotseat + AllApps + Folder
4. 数据持久化通过 LauncherProvider（ContentProvider）+ launcher.db 实现
5. 采用类 MVC 架构：Launcher(View) + LauncherModel(Model) + LoaderTask(Controller)
6. 拖拽系统基于 DragSource/DropTarget 接口，DragController 协调整个生命周期
7. Quickstep 模块通过 AIDL 与 SystemUI 桥接，实现手势导航和最近任务

---

## 新手学习路径

```
入门 → 通读本文，建立 Launcher3 目录结构和核心类的认知地图
  ↓
进阶 → 用 Layout Inspector 查看视图层级，阅读 Workspace 和 CellLayout 源码
  ↓
高级 → 深入 LauncherModel/LoaderTask 数据加载流程和 DragController 拖拽系统
  ↓
专家 → 定制 Quickstep 手势导航，实现自定义桌面布局和图标主题
```

---

## 本文小结

| 维度 | 要点 |
|------|------|
| 目录结构 | src(主逻辑) + quickstep(手势) + iconloaderlib(图标) + res(资源) |
| 视图层级 | DragLayer → Workspace/Hotseat/AllApps/Folder，BubbleTextView 为图标基础视图 |
| 数据模型 | LauncherProvider + launcher.db 持久化，ItemInfo 继承体系承载元素数据 |
| 拖拽系统 | DragSource/DropTarget 接口 + DragController 控制器 + DragLayer 容器 |
| 跨进程桥接 | Quickstep 通过 IOverviewProxy/ISystemUiProxy 与 SystemUI 通信 |

--- END ---