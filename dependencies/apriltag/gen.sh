PLATFORM="android-28"
# declare -a ABIs=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
declare -a ABIs=("arm64-v8a" "armeabi-v7a", "x86")
#declare -a MODEs=("Debug" "Release")
declare -a MODEs=("Debug")
CMAKE="/opt/android-sdk/cmake/3.6.4111459/bin/cmake"
NINJA="/opt/android-sdk/cmake/3.6.4111459/bin/ninja"
SDK="/opt/android-sdk"
export NDK="/opt/android-sdk/ndk/latest"
export CC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang
export CXX=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++
SRCDIR=$PWD
echo $CMAKE
echo $CC
echo $CXX

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
         -DCMAKE_C_COMPILER=$CC \
         -DANDROID_ABI=$ABI \
         -DANDROID_PLATFORM=$PLATFORM \
         -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=$SRCDIR/obj/$MODE/$ABI \
         -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=$SRCDIR/obj/$MODE/$ABI \
         -DCMAKE_BUILD_TYPE=$MODE \
         -DANDROID_NDK=$NDK \
         -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake  \
         -DCMAKE_MAKE_PROGRAM=$NINJA \
         -G"Android Gradle - Ninja" \
         $SRCDIR
   done
done
