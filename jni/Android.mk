LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

OPENCV_INSTALL_MODULES:=on

OPENCV_CAMERA_MODULES:=off

OPENCV_LIB_TYPE:=STATIC

include ./OpenCV-3.0.0-android-sdk-1/OpenCV-android-sdk/sdk/native/jni/OpenCV.mk

include $(OPENCV_MK_PATH)

LOCAL_MODULE    := MarkerBasedAR
LOCAL_SRC_FILES := MarkerBasedAR.cpp CameraCalibration.cpp GeometryTypes.cpp Marker.cpp MarkerDetector.cpp TinyLA.cpp
LOCAL_LDLIBS +=  -llog -ldl -landroid -lEGL -lGLESv1_CM -lGLESv2

include $(BUILD_SHARED_LIBRARY)
