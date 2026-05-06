# Android.mk for Binder Demo (Legacy)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# 静态库版本
include $(CLEAR_VARS)
LOCAL_MODULE := libmybinder
LOCAL_SRC_FILES := IMyBinder.cpp MyBinderService.cpp MyBinderClient.cpp
LOCAL_SHARED_LIBRARIES := libbinder libutils liblog
LOCAL_CFLAGS := -Wall -Werror
include $(BUILD_SHARED_LIBRARY)

# 服务端
include $(CLEAR_VARS)
LOCAL_MODULE := mybinder_service
LOCAL_SRC_FILES := service_main.cpp
LOCAL_SHARED_LIBRARIES := libmybinder libbinder libutils liblog
include $(BUILD_EXECUTABLE)

# 客户端
include $(CLEAR_VARS)
LOCAL_MODULE := mybinder_client
LOCAL_SRC_FILES := client_main.cpp
LOCAL_SHARED_LIBRARIES := libmybinder libbinder libutils liblog
include $(BUILD_EXECUTABLE)
