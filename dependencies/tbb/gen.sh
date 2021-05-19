PLATFORM="android-24"
declare -a ABIs=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
declare -a MODEs=("Debug" "Release")
CMAKE="/opt/android-sdk/cmake/3.6.4111459/bin/cmake"
SDK="/opt/android-sdk"
NDK="/opt/android-sdk/ndk/latest"
SRCDIR=$PWD
for ABI in "${ABIs[@]}"
do
   for MODE in "${MODEs[@]}"
   do
      BUILD_DIR="$SRCDIR/compile/$MODE/$ABI"
      echo $BUILD_DIR
      rm -rf $BUILD_DIR/*
      mkdir -p $BUILD_DIR/
      cd $BUILD_DIR
      $CMAKE \
         -DANDROID_ABI=$ABI \
         -DANDROID_PLATFORM=$PLATFORM \
         -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=$SRCDIR/obj/$MODE/$ABI \
         -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=$SRCDIR/obj/$MODE/$ABI \
         -DCMAKE_BUILD_TYPE=$MODE \
         -DANDROID_NDK=$NDK \
         -DCMAKE_CXX_FLAGS=-std=c++17 \
         -DANDROID_STL=c++_shared \
         -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake  \
         -DCMAKE_MAKE_PROGRAM=$SDK/cmake/3.6.4111459/bin/ninja \
         -G"Android Gradle - Ninja" \
         $SRCDIR
   done
done
