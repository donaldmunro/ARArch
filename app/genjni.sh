#!/bin/bash
KOTLIN=/opt/android-sdk/tools/lib/kotlin-stdlib-1.1.3-2.jar
ANDROID=/opt/android-sdk/platforms/android-28/android.jar
CLASSES=build/tmp/kotlin-classes/debug
PACKAGE=no.pack.drill.ararch.mar
/opt/android-studio/jre/bin/javah -jni -cp "$KOTLIN:$ANDROID:$CLASSES" $PACKAGE.HardwareCamera
/opt/android-studio/jre/bin/javah -jni -cp "$KOTLIN:$ANDROID:$CLASSES" $PACKAGE.MAR
/opt/android-studio/jre/bin/javah -jni -cp "$KOTLIN:$ANDROID:$CLASSES" $PACKAGE.CPUFrameHandler
