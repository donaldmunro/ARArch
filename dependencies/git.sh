cd apriltag/
mv .git/ git.org/
cd ..
cd tbb
mv .git/ git.org/
cd ..
git add -Av apriltag/ concurrentqueue/ tbb/
cd apriltag/
mv git.org/ .git/
cd ..
cd tbb
mv git.org/ .git/
cd ..
