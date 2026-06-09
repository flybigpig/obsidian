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
