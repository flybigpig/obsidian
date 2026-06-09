LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := MySampleApp
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_REQUIRED_MODULES :=

LOCAL_RESOURCE_DIRS := $(LOCAL_PATH)/res
LOCAL_MANIFEST_FILE := AndroidManifest.xml

include $(BUILD_PACKAGE)
