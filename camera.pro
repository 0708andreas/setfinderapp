TEMPLATE = app
TARGET = camera

QT += multimedia multimediawidgets

HEADERS = \
    camera.h \
    imagesettings.h \
    videosettings.h \
    guesser.h \
    metadatadialog.h

SOURCES = \
    main.cpp \
    camera.cpp \
    imagesettings.cpp \
    videosettings.cpp \
    guesser.cpp \
    metadatadialog.cpp

FORMS += \
    imagesettings.ui

android|ios {
    FORMS += \
        camera_mobile.ui \
        videosettings_mobile.ui
} else {
    FORMS += \
        camera.ui \
        videosettings.ui
}
RESOURCES += camera.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/multimediawidgets/camera
INSTALLS += target

QT += widgets
#include(../../multimedia/shared/shared.pri)

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
OTHER_FILES += android/AndroidManifest.xml

android {
    OPENCV_ANDROID = "/home/andreas/MEGA/projects/set_check/SetFinderQMLApp/SetFinderQMLApp/OpenCV-android-sdk"
    contains(ANDROID_TARGET_ARCH, arm64-v8a) {
        isEmpty(OPENCV_ANDROID) {
            error("Let OPENCV_ANDROID point to the opencv-android-sdk, recommended: v4.0")
        }
        INCLUDEPATH += "$$OPENCV_ANDROID/sdk/native/jni/include"
        LIBS += \
            -L"$$OPENCV_ANDROID/sdk/native/libs/arm64-v8a" \
            -L"$$OPENCV_ANDROID/sdk/native/3rdparty/libs/arm64-v8a" \
            -llibtiff \
            -llibjpeg-turbo \
            -llibpng \
            -lIlmImf \
            -ltbb \
            -lopencv_java4 \

        ANDROID_EXTRA_LIBS = $$OPENCV_ANDROID/sdk/native/libs/arm64-v8a/libopencv_java4.so
    }
    contains(ANDROID_TARGET_ARCH, armeabi-v7a) {
        isEmpty(OPENCV_ANDROID) {
            error("Let OPENCV_ANDROID point to the opencv-android-sdk, recommended: v4.0")
        }
        INCLUDEPATH += "$$OPENCV_ANDROID/sdk/native/jni/include"
        LIBS += \
            -L"$$OPENCV_ANDROID/sdk/native/libs/armeabi-v7a" \
            -L"$$OPENCV_ANDROID/sdk/native/3rdparty/libs/armeabi-v7a" \
            -llibtiff \
            -llibjpeg-turbo \
            -llibpng \
            -lIlmImf \
            -ltbb \
            -lopencv_java4 \

        ANDROID_EXTRA_LIBS = $$OPENCV_ANDROID/sdk/native/libs/armeabi-v7a/libopencv_java4.so
    }
}
