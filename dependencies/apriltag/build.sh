# declare -a ABIs=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
declare -a ABIs=("arm64-v8a" "armeabi-v7a", "x86")
#declare -a ABIs=("arm64-v8a")
#declare -a MODEs=("Debug" "Release")
declare -a MODEs=("Debug")
CMAKE="/opt/android-sdk/cmake/3.6.4111459/bin/cmake"
SRCDIR=$PWD
for ABI in "${ABIs[@]}"
do
   for MODE in "${MODEs[@]}"
   do
      # rm -rf $BUILD_DIR
      BUILD_DIR="$SRCDIR/compile/$MODE/$ABI"
      $CMAKE --build $BUILD_DIR
   done
done
rm -rf ../../app/c++/include/apriltags 
mkdir ../../app/c++/include/apriltags
cp $SRCDIR/*.h ../../app/c++/include/apriltags
rsync -avz $SRCDIR/common ../../app/c++/include/apriltags
rsync -avz obj/Debug/ ../../app/c++/static-libs/
