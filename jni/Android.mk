# Android.mk - ndk-build configuration for local_api
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := local_api

# Only two translation units:
#   main.cpp     — HTTP network interface + routes
#   detector.cpp — KernelSU / APatch / Magisk / SusFS detection + JSON
LOCAL_SRC_FILES := \
    ../src/main.cpp \
    ../src/detector.cpp

# include/  holds the UAPI headers + detector.hpp;
# third_party/ holds httplib.h and nlohmann/json.hpp (so that
# "#include <nlohmann/json.hpp>" resolves).
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../third_party

LOCAL_CPPFLAGS := -std=c++17 -frtti -fexceptions -Wall -Wextra

LOCAL_LDLIBS := -llog -pthread

include $(BUILD_EXECUTABLE)
