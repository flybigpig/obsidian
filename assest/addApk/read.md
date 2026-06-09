我先看一下你目前的工作区内容,确认 `addApk/read.md` 是不是空文件、`code/MySampleApp/Android.bp` 是否已存在,再决定给你写"从已有 APK / 从已有源码两种方式"的实例。
工作区现状:
- `app源码/code/MySampleApp/Android.bp` 已有,你之前已经把"自己写源码 + framework 编译"那套搭好了。
- `addApk/read.md` 是空文件,显然你想把"添加 APK(已有 .apk / 预编译 .apk)"也整理一份文档。

下面给你一份**完整实例**,从"只想把第三方 APK 塞进 system image"到"预编译 APK + 自定义 Android.mk 接入"两套方案都写清楚。

# Android Framework 添加 APK 完整实例(预编译 APK 方案)

## 方案对比

| 方案 | 适用 | 是否需要重签 | 推荐度 |
|---|---|---|---|
| A. **PRODUCT_PACKAGES 直接加** | 有源码/标准 `.bp` 的 app | 否(按 bp 走) | ★★★★★ |
| B. **预编译 APK (prebuilt) + `Android.mk`** | 已有第三方 `.apk` / `.aar` | **必须**用 platform 重签 | ★★★★ |
| C. 直接 `adb push` + `pm install` | 调试阶段,不入 system | 临时,不入镜像 | ★★ |

下面是 **B 方案** 的完整实例(以一个 `MyVendorApp.apk` 为例)。

## 1. 目录结构

```
vendor/<vendor>/<product>/prebuilts/
└── MyVendorApp/
    ├── Android.mk
    ├── MyVendorApp.apk           ← 你的预编译 APK
    └── Android.mk 内的签名配置(可选)
```

也可以放到 `frameworks/base/prebuilts/`、`device/<vendor>/<product>/prebuilts/`、`packages/apps/` 等任意位置,只要 `.mk` 路径能 `LOCAL_PATH` 找到即可。

## 2. 把 APK 放进源码树

```bash
# 假设你的 vendor 目录
mkdir -p vendor/myvendor/myproduct/prebuilts/MyVendorApp
cp /path/to/MyVendorApp.apk vendor/myvendor/myproduct/prebuilts/MyVendorApp/

# 关键:默认 644,APK 也能跑,但建议保留签名信息
chmod 644 vendor/myvendor/myproduct/prebuilts/MyVendorApp/MyVendorApp.apk
```

> ⚠️ APK 自身的 **META-INF 签名** 在 AOSP 默认 `LOCAL_CERTIFICATE := platform` 重新签名时**会被覆盖**,所以原 APK 里写死的权限检查(`signatureOrSystem`、自定义 signature 权限)需要重新评估。

## 3. 编写 `Android.mk`

**`vendor/myvendor/myproduct/prebuilts/MyVendorApp/Android.mk`**

```makefile
LOCAL_PATH := $(call my-dir)

# -------- 1) 用 platform 签名重新打 APK --------
include $(CLEAR_VARS)
LOCAL_MODULE := MyVendorApp
LOCAL_SRC_FILES := MyVendorApp.apk
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_OVERRIDES_PACKAGES :=                    # 可选:替换系统同名包
LOCAL_REQUIRED_MODULES :=
include $(BUILD_PREBUILT)

# -------- 2) (可选)把 APK 里的 .so 库单独提取到 system/lib --------
include $(CLEAR_VARS)
LOCAL_MODULE := MyVendorApp_native
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/arm64-v8a/libmysdk.so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib
include $(BUILD_PREBUILT)
```

要点解释:
- `LOCAL_CERTIFICATE := platform` —— 用 AOSP 的 `build/target/product/security/platform.{pk8,pem}` 重新签名,这是和 `system_server` 同签名的关键。
- `LOCAL_PRIVILEGED_MODULE := true` —— 装到 `/system/priv-app/`,可声明 `android:protectionLevel="signature|privileged"` 的系统权限。
- `LOCAL_MODULE_CLASS := APPS` —— 让 Soong/make 把它识别为 APK,而不是普通文件。
- `LOCAL_MODULE_TAGS := optional` —— AOSP 默认的 product `BUILD_WITHOUT_PATCH` 不会带,但被 `PRODUCT_PACKAGES` 引用后一定带上。

## 4. 集成到默认 x86-64 product

按上一轮 `addSrc.md` 的经验,**最简**是改 `build/target/product/aosp_x86_64.mk` 末尾:

```makefile
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_x86_64.mk)

# --- 末尾追加 ---
PRODUCT_PACKAGES += \
    MySampleApp \      # 你之前的源码方案
    MyVendorApp        # 这次的预编译 APK 方案
```

如果你要把 APK 推到 `vendor/`,还需要让 build 系统能 `find` 到这个目录,通常在 `device/<vendor>/<product>/device.mk` 中加:

```makefile
$(call inherit-product-if-exists, vendor/myvendor/myproduct/*.mk)
PRODUCT_PACKAGES += MyVendorApp
```

## 5. 编译并验证

```bash
source build/envsetup.sh
lunch aosp_x86_64-eng

# 单模块编译
make MyVendorApp -j$(nproc)

# 整编(收集进 system image)
make -j$(nproc)
```

产物路径:

```
out/target/product/emulator_x86_64/system/priv-app/MyVendorApp/MyVendorApp.apk
```

> 注意 AOSP 13+ 默认使用 `vendor` 目录:`out/target/product/emulator_x86_64/vendor/...`,看 PRODUCT 配置决定。

启动模拟器并验证:

```bash
emulator
adb shell pm list packages | grep myvendorapp
adb shell dumpsys package com.vendor.myvendorapp | grep -E "signingInfo|flags"
# flags 应包含 SYSTEM,signature 应为 platform
```

## 6. 常见问题

### 6.1 提示 `LOCAL_CERTIFICATE := PRESIGNED`?
- 保留 APK 原签名,这样不替换就不能在 `priv-app` 拿 signature 权限。
- 只有当 APK **不是** 系统应用,或者你不希望重签时,才用 `PRESIGNED`。

### 6.2 想把 APK 放到 `/system/app` 而不是 `priv-app`?
```makefile
LOCAL_PRIVILEGED_MODULE :=
LOCAL_MODULE_PATH := $(TARGET_OUT)/app      # 等价于 /system/app
```

### 6.3 想给 APK 配 dexpreopt(更快启动)?
```makefile
WITH_DEXPREOPT := true
```
在 `device.mk` 顶层或 `BoardConfig.mk` 里设置。

### 6.4 想覆盖同名系统应用?
```makefile
LOCAL_OVERRIDES_PACKAGES := Settings SystemUI
```
被列出的模块,本 APK 安装时会"挤掉"它们的位置(必须是相同包名)。

### 6.5 第三方 APK 想要"以 system 身份启动另一个 app"?
重签为 platform 后,可以使用:
```xml
<uses-permission android:name="android.permission.INSTALL_PACKAGES" />
<uses-permission android:name="android.permission.SHUTDOWN" />
```
等 `signature`/`privileged` 级权限。

## 7. 关键点小结

| 项 | 设置 | 作用 |
|---|---|---|
| `LOCAL_CERTIFICATE := platform` | mk | 用 platform 重签 → 拿 signature 权限 |
| `LOCAL_PRIVILEGED_MODULE := true` | mk | 装到 `/system/priv-app/` |
| `LOCAL_MODULE_CLASS := APPS` | mk | 让 make 识别为 APK |
| `BUILD_PREBUILT` | mk | 用预编译的 .apk 而不是 BUILD_PACKAGE |
| `PRODUCT_PACKAGES += MyVendorApp` | device.mk | 把模块收集进 system image |
| `local-include` 模式 | - | 仅供单一 product,不要污染主仓 |

---

## 8. 需要我落盘吗?

你目前 `addApk/read.md` 是空的。我可以**直接把上面这份实例写入** `c:\D\otherproject\obsidian\assest\addApk\read.md`,并保持 Obsidian 友好的标题层级 / 标签。要的话告诉我 **"写入 addApk/read.md"** 即可。