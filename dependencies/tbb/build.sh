MODE="Debug"
declare -a ABIs=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
declare -a MODEs=("Debug" "Release")
CMAKE="/opt/android-sdk/cmake/3.6.4111459/bin/cmake"
SRCDIR=$PWD
for ABI in "${ABIs[@]}"
do
   for MODE in "${MODEs[@]}"
   do
      BUILD_DIR="$SRCDIR/compile/$MODE/$ABI"
      $CMAKE --build $BUILD_DIR
   done
done
rsync -avz include/ ../../app/c++/include/
