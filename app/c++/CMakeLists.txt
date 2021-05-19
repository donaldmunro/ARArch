cmake_minimum_required(VERSION 3.18)
project(ARArch)
set(CMAKE_VERBOSE_MAKEFILE ON)
include(ExternalProject)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckCXXSourceCompiles)

#set(CMAKE_CXX_STANDARD 17) #Set in build.gradle via direct compile option
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CUSTOM_OPENCV TRUE)  # see script build-android-opencv.sh
set(CUSTOM_OPENCV_HOME "/opt/opencv-android-sdk-static")
set(SDK_OPENCV_HOME "/opt/opencv-android-sdk-dist")
set(COMPILE_SIMPLE_VULKAN_RENDERER TRUE)
set(USE_TBB_MALLOC TRUE)
set(USE_CUSTOM_EIGEN TRUE)
set(USE_APRILTAGS TRUE)
set(USE_FACE_RECOG FALSE)

if(NOT DEFINED ANDROID_PLATFORM)
   set(ANDROID_PLATFORM "android-27")
endif()
if(NOT DEFINED ANDROID_NATIVE_API_LEVEL)
   set(ANDROID_NATIVE_API_LEVEL "27")
endif()

include("${CMAKE_MODULE_PATH}/functions.cmake")

get_ndk(NDK)

MESSAGE(STATUS "C++ Version: " ${CMAKE_CXX_STANDARD})
MESSAGE(STATUS "C++ Compiler: " ${CMAKE_CXX_COMPILER})
MESSAGE(STATUS "Android NDK: " ${NDK})
MESSAGE(STATUS "Android ABI: " ${ANDROID_ABI})

find_library(log-lib log REQUIRED)

set(ASSETS "${CMAKE_SOURCE_DIR}/../assets/")
set(STATIC_LIBS_DIR "${CMAKE_SOURCE_DIR}/static-libs/")
set(DEPENDENCIES "${PROJECT_SOURCE_DIR}/libs")
set(EIGEN_REPO "https://github.com/eigenteam/eigen-git-mirror.git")
set(LOCAL_EIGEN "${DEPENDENCIES}/eigen")
set(LOCAL_EIGEN_GIT "${LOCAL_EIGEN}/.git")
set(BITS32 FALSE)
if(ANDROID_ABI STREQUAL "x86_64")
   set(ARCH_ONLY "x86_64")
   set(ARCH_SIMPLE "arch-x86_64")
   set(ARCH_SHORT "x86_64")
elseif (ANDROID_ABI STREQUAL "x86")
   set(ARCH_ONLY "x86")
   set(ARCH_SIMPLE "arch-x86")
   set(ARCH_SHORT "x86")
   set(BITS32 TRUE)
elseif (ANDROID_ABI STREQUAL "arm64-v8a")
   set(ARCH_ONLY "arm64-v8a")
   set(ARCH_SIMPLE "arch-arm64")
   set(ARCH_SHORT "aarch64")
elseif (ANDROID_ABI STREQUAL "armeabi-v7a")
   set(ARCH_ONLY "armeabi-v7a")
   set(ARCH_SIMPLE "arch-arm")
#   set(ARCH_SHORT "arm7")
   set(ARCH_SHORT "arm")
   set(BITS32 TRUE)
else()
   set(ANDROID_ABI "arm64-v8a")
   set(ARCH_ONLY "arm64-v8a")
   set(ARCH_SIMPLE "arch-arm64")
   set(ARCH_SHORT "aarch64")
endif()
set(INVOKE_CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${NDK}/build/cmake/android.toolchain.cmake;-DANDROID_ABI=${ANDROID_ABI};-DANDROID_PLATFORM=${ANDROID_PLATFORM};-DANDROID_NATIVE_API_LEVEL=${ANDROID_NATIVE_API_LEVEL}") # -DANDROID_SYSROOT=${ANDROID_SYSROOT} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD} -DCMAKE_CXX_EXTENSIONS=${CMAKE_CXX_EXTENSIONS} -DCMAKE_CXX_STANDARD_REQUIRED=${CMAKE_CXX_STANDARD_REQUIRED}"

MESSAGE(STATUS "Android Arch: " ${ARCH_ONLY})

set(FLAGS "-Wall;-frtti;-fexceptions;-std=c++17;-Wno-switch;-Wno-unused-variable")
if(BITS32)
   list(APPEND FLAGS "-DBITS32")
endif()
set(SYS_LIBS "m;z;dl;android")
set (INCLUDES_DIR "${PROJECT_SOURCE_DIR}/include")
set(AR_INCLUDE_DIR "${INCLUDES_DIR}/mar")
set(ARCH_INCLUDES "")
set(LIBS "")

if(USE_CUSTOM_EIGEN)
   file(MAKE_DIRECTORY ${LOCAL_EIGEN})
   if (EXISTS ${LOCAL_EIGEN} AND EXISTS ${LOCAL_EIGEN_GIT})
      execute_process(COMMAND git pull WORKING_DIRECTORY ${LOCAL_EIGEN} RESULT_VARIABLE status)
      if (NOT "${status}" STREQUAL "0")
        message(STATUS "WARNING: Error pulling Eigen")
   endif()
   else()
      execute_process(COMMAND git clone ${EIGEN_REPO} WORKING_DIRECTORY ${DEPENDENCIES} RESULT_VARIABLE status)
      if (NOT "${status}" STREQUAL "0")
        message(FATAL_ERROR "Fatal error cloning/pulling Eigen")
      endif()
      file(RENAME "${DEPENDENCIES}/eigen-git-mirror" "${LOCAL_EIGEN}")
   endif()
   set(EIGEN3_INCLUDE_DIR "${LOCAL_EIGEN}")
else()
   find_package(Eigen3 REQUIRED)
endif()
MESSAGE(STATUS "Eigen 3 Include: " ${EIGEN3_INCLUDE_DIR})
list(APPEND ARCH_INCLUDES "${EIGEN3_INCLUDE_DIR}")

set(OpenCV_LIBS "opencv_calib3d;opencv_imgcodecs;opencv_imgproc;opencv_core") # Linked OpenCV libraries

if (CUSTOM_OPENCV) # Built from source - see script build-android-opencv.sh
   set(OPENCV_HOME "${CUSTOM_OPENCV_HOME}")
#   set (OpenCV_DIR "${OPENCV_HOME}/${ARCH_ONLY}/sdk/native")
   set(OpenCV_DIR "${OPENCV_HOME}/${ANDROID_ABI}/sdk/native")
   set(OpenCV_INCLUDE_DIR "${OpenCV_DIR}/jni/include")
   set(OpenCV_LINK_DIRECTORIES "${OpenCV_DIR}/staticlibs/${ANDROID_ABI}")
   set(OPENCV_XTRA_LIBS "${OpenCV_DIR}/3rdparty/libs/${ANDROID_ABI}")
   set(USE_FACE_RECOG TRUE)
   list(APPEND FLAGS "-DHAS_FACE_DETECTION")
   list(FIND OpenCV_LIBS "opencv_imgproc" II)
#   math(EXPR II "${II}-1")
   list(INSERT OpenCV_LIBS ${II} "opencv_objdetect") #inserts before imgproc as requires cv::resize in imgproc
   #   list(APPEND OpenCV_LIBS "opencv_objdetect")
else()
   set(OPENCV_HOME "${SDK_OPENCV_HOME}") # Downloaded as zip
   set (OpenCV_DIR "${OPENCV_HOME}/sdk/native") #/abi-${ANDROID_ABI}"
   # file(GLOB_RECURSE OPENCV_CONFIG "${OPENCV_HOME}/OpenCVConfig.cmake*")
   # get_filename_component(OPENCV_CONFIG_DIR "${OPENCV_CONFIG}" DIRECTORY)
   set(OpenCV_INCLUDE_DIR "${OpenCV_DIR}/jni/include")
   if (NOT EXISTS ${OpenCV_INCLUDE_DIR})
      message(FATAL_ERROR "OpenCV include directory ${OpenCV_INCLUDE_DIR} not found.")
   endif()
   set(OpenCV_LINK_DIRECTORIES "${OPENCV_HOME}/sdk/native/staticlibs/${ANDROID_ABI}/")
   if (NOT EXISTS ${OpenCV_LINK_DIRECTORIES})
      message(FATAL_ERROR "OpenCV lib directory ${OpenCV_LINK_DIRECTORIES} not found.")
   else()
      message(STATUS "OpenCV link directory: ${OpenCV_LINK_DIRECTORIES}")
   endif()
   set(OPENCV_XTRA_LIBS "${OpenCV_DIR}/3rdparty/libs/${ANDROID_ABI}")
endif()
list(APPEND ARCH_INCLUDES "${OpenCV_INCLUDE_DIR}")
foreach(LIBFILE ${OpenCV_LIBS})
   set(LIBPATH "${OpenCV_LINK_DIRECTORIES}/lib${LIBFILE}.a")
   if (NOT EXISTS ${LIBPATH})
      message(FATAL_ERROR "OpenCV Library ${LIBPATH} not found.")
   else()
      list(APPEND LIBS ${LIBPATH})
      list(APPEND OPENCV_LIBRARIES ${LIBPATH})
   endif()
endforeach()
MESSAGE(STATUS "OpenCV include directory: ${OpenCV_INCLUDE_DIR}")
MESSAGE(STATUS "OpenCV libs: ${OpenCV_LINK_DIRECTORIES}")

#libtegra_hal.a" carotene_o4t link error (ARM only), libippiw.a;libippicv.a Intel math (x86 only), libittnotify.a Intel ?,
# liblibjpeg-turbo.a liblibpng.a etc imwrite/read only can be commented out
#Order is important with static libs
set(OPENCV_EXTRALIBS libtegra_hal.a libippiw.a libippicv.a libittnotify.a libcpufeatures.a
   liblibjpeg-turbo.a liblibpng.a libIlmImf.a liblibjasper.a liblibtiff.a liblibwebp.a
)
foreach(LIBNAME IN ITEMS ${OPENCV_EXTRALIBS})
   if (EXISTS "${OPENCV_XTRA_LIBS}/${LIBNAME}")
      list(APPEND LIBS "${OPENCV_XTRA_LIBS}/${LIBNAME}")
      list(APPEND OPENCV_LIBRARIES "${OPENCV_XTRA_LIBS}/${LIBNAME}")
   else()
      MESSAGE(STATUS "WARNING: OPENCV Extra lib ${OPENCV_XTRA_LIBS}/${LIBNAME} not found.")
   endif()
endforeach()
MESSAGE(STATUS "OpenCV 3rd party libs: ${OPENCV_XTRA_LIBS}")
MESSAGE(STATUS "All OpenCV Canonicalised Libraries: ${OPENCV_LIBRARIES}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
   if (EXISTS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbd.a")
      list(APPEND LIBS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbd.a")
      list(APPEND TBB_LIBS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbd.a")
      list(APPEND FLAGS "-DTBB_USE_ASSERT")
      list(APPEND FLAGS "-DTBB_USE_PERFORMANCE_WARNINGS")
   else()
      MESSAGE(FATAL_ERROR "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbd.a not found")
   endif()
   if(${USE_TBB_MALLOC})
      if (EXISTS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbmallocd.a")
         list(APPEND LIBS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbmallocd.a")
         list(APPEND TBB_LIBS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbmallocd.a")
      else()
         MESSAGE(STATUS "WARNING: TBB malloc lib ${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbmallocd.a not found.")
      endif()
   endif()
else()
   if (EXISTS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbb.a")
      list(APPEND LIBS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbb.a")
      list(APPEND TBB_LIBS  "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbb.a")
   else()
      MESSAGE(FATAL_ERROR "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbb.a not found")
   endif()
   if(${USE_TBB_MALLOC})
      if (EXISTS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbmalloc.a")
         list(APPEND LIBS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbmalloc.a")
         list(APPEND TBB_LIBS  "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbmalloc.a")
      else()
         MESSAGE(STATUS "WARNING: TBB malloc lib ${STATIC_LIBS_DIR}/${ANDROID_ABI}/libtbbmalloc.a not found.")
      endif()
   endif()
endif()

find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
   set(VULKAN_LIB "${NDK}/platforms/${ANDROID_PLATFORM}/${ARCH_SIMPLE}/usr/lib/libvulkan.so")
   if (NOT EXISTS ${VULKAN_LIB})
      message(STATUS "Vulkan library not found at ${VULKAN_LIB}")
      set(VULKAN_LIB "${NDK}/platforms/${ANDROID_PLATFORM}/${ARCH_SIMPLE}/usr/lib64/libvulkan.so")
      message(STATUS "Trying ${VULKAN_LIB}")
   endif()
   if (NOT EXISTS ${VULKAN_LIB})
      file(GLOB_RECURSE VULKAN_PATHS  ${NDK}/toolchains/llvm/prebuilt/libvulkan.so)
      foreach(VULKAN_PATH ${VULKAN_PATHS})
         STRING(FIND ${VULKAN_PATH} "${ARCH_SHORT}-" SPOS)
         if(SPOS GREATER -1)
            STRING(FIND ${VULKAN_PATH} "${ANDROID_NATIVE_API_LEVEL}" SPOS REVERSE)
            if(SPOS GREATER -1)
               set(VULKAN_LIB ${VULKAN_PATH})
               break()
            endif()
         endif()
      endforeach()
   endif()
else()
   message(STATUS "FindVulkan: "${Vulkan_LIBRARIES})
   set(VULKAN_LIB ${Vulkan_LIBRARIES})
endif()
if (NOT EXISTS ${VULKAN_LIB})
   if(NOT ${COMPILE_BULB_RENDERER})
      message(FATAL_ERROR "Vulkan library ${VULKAN_LIB} not found. Check NDK")
   else()
      # message(STATUS "Vulkan not found: Defaulting to OpenGL only")
      # set(VULKAN_LIB "")
      message(FATAL_ERROR "Vulkan not found")
   endif()
else()
   message(STATUS "Vulkan Library: ${VULKAN_LIB}")
   list(APPEND LIBS "${VULKAN_LIB}")
   list(APPEND FLAGS "-DVK_USE_PLATFORM_ANDROID_KHR")
endif()

#add_library(repository SHARED ${AR_INCLUDE_DIR}/Repository.h src/Repository.cc
#   ${AR_INCLUDE_DIR}/acquisition/FrameInfo.h  src/acquisition/FrameInfo.cc
#   ${AR_INCLUDE_DIR}/acquisition/Camera.h src/acquisition/Camera.cc)
#target_compile_options(repository PRIVATE ${FLAGS} )
#target_include_directories(repository PRIVATE ${PROJECT_SOURCE_DIR}/include)
#target_link_libraries(repository PRIVATE ${log-lib} ${SYS_LIBS} ${TBB_LIBS})

if(COMPILE_SIMPLE_VULKAN_RENDERER)
#   set(VRENDERER_FLAGS ${FLAGS})
#   set(VRENDERER_LIBS ${SYS_LIBS})
   list(APPEND FLAGS "-DHAS_SIMPLE_RENDERER")
   list(APPEND VRENDERER_LIBS ${VULKAN_LIB})
#   add_library(simplevulkan SHARED ${AR_INCLUDE_DIR}/render/VulkanRenderer.h src/render/vulkan/VulkanRenderer.cc
#   ${AR_INCLUDE_DIR}/render/VulkanTools.h src/render/vulkan/VulkanTools.cc src/render/vulkan/VmaUsage.cc
#   ${AR_INCLUDE_DIR}/util/android.hh) src/android.cc
#   target_compile_options(simplevulkan PRIVATE ${VRENDERER_FLAGS} )
#   target_include_directories(simplevulkan PRIVATE ${PROJECT_SOURCE_DIR}/include ${ARCH_INCLUDES})
##   target_link_libraries(simplevulkan PRIVATE ${log-lib} repository ${VRENDERER_LIBS} ${OPENCV_LIBRARIES} ${TBB_LIBS})
#   target_link_libraries(simplevulkan PRIVATE ${log-lib} ${VRENDERER_LIBS} ${OPENCV_LIBRARIES} ${TBB_LIBS})
#   message(STATUS "Building simple Vulkan renderer")
endif()

if (USE_APRILTAGS)
   if (CMAKE_BUILD_TYPE MATCHES Debug)
      if (NOT EXISTS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libapriltagd.a")
         message(FATAL_ERROR "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libapriltagd.a not found")
      else()
         set(APRIL_LIB "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libapriltagd.a")
      endif()
   else()
      if (NOT EXISTS "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libapriltag.a")
         message(FATAL_ERROR "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libapriltag.a not found")
      else()
         set(APRIL_LIB "${STATIC_LIBS_DIR}/${ANDROID_ABI}/libapriltag.a")
      endif()
   endif()
   list(APPEND LIBS "${APRIL_LIB}")
   list(APPEND FLAGS "-DHAS_APRILTAGS")
endif()

if (WIN32)
   set(SPIRV "${NDK}/shader-tools/windows-x86_64/glslc.exe")
else()
   set(SPIRV "${NDK}/shader-tools/linux-x86_64/glslc")
endif()
set(SHADER_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../shaders")
file(GLOB_RECURSE SHADER_SOURCES
    "${SHADER_SOURCE_DIR}/*.frag"
    "${SHADER_SOURCE_DIR}/*.vert"
)
message(STATUS "Compiling shader sources ${SHADER_SOURCES}")
foreach(SHADER_SOURCE ${SHADER_SOURCES})
  get_filename_component(FILE_NAME ${SHADER_SOURCE} NAME)
  set(SPIRV_OBJ "${ASSETS}/shaders/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV_OBJ}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSETS}/shaders"
    COMMAND ${SPIRV} -O ${SHADER_SOURCE} -o ${SPIRV_OBJ}
    DEPENDS ${SHADER_SOURCE})
  list(APPEND SPIRV_OBJS ${SPIRV_OBJ})
endforeach()
message(STATUS "Compiled shader object ${SPIRV_OBJS}")
add_custom_target( Shaders DEPENDS ${SPIRV_OBJS} )

foreach(LIBFILE ${LIBS})
   if (NOT EXISTS ${LIBFILE})
      message(FATAL_ERROR "Library ${LIBFILE} not found.")
   endif()
   message(STATUS ${LIBFILE})
endforeach()

add_library(MAR SHARED src/jni.cc ${AR_INCLUDE_DIR}/jniint.h
            ${AR_INCLUDE_DIR}/util/android.hh src/android.cc
            ${AR_INCLUDE_DIR}/Repository.h src/Repository.cc ${AR_INCLUDE_DIR}/Structures.h
            ${AR_INCLUDE_DIR}/CalibrationValues.hh ${AR_INCLUDE_DIR}/util/LockFreeStack.hh
            ${AR_INCLUDE_DIR}/acquisition/Sensors.h src/acquisition/Sensors.cc ${AR_INCLUDE_DIR}/acquisition/SensorData.hh
            ${AR_INCLUDE_DIR}/acquisition/EmulatorCamera.h src/acquisition/Camera.cc src/acquisition/EmulatorCamera.cc
            ${AR_INCLUDE_DIR}/acquisition/FrameInfo.h src/acquisition/FrameInfo.cc
            ${AR_INCLUDE_DIR}/util/util.hh src/util/util.cc ${AR_INCLUDE_DIR}/util/cv.h src/util/cv.cc
            ${AR_INCLUDE_DIR}/architecture/Architecture.h src/architecture/architecture.cc
            ${AR_INCLUDE_DIR}/render/Renderer.h ${AR_INCLUDE_DIR}/render/RendererFactory.hh

            ${AR_INCLUDE_DIR}/render/VulkanRenderer.h src/render/vulkan/VulkanRenderer.cc
            ${AR_INCLUDE_DIR}/render/VulkanTools.h src/render/vulkan/VulkanTools.cc src/render/vulkan/VmaUsage.cc
            ${AR_INCLUDE_DIR}/render/ArchVulkanRenderer.h src/render/vulkan/ArchVulkanRenderer.cc

            ${AR_INCLUDE_DIR}/architecture/tbb/TBBCameraSource.h src/architecture/tbb/TBBCameraSource.cc
            ${AR_INCLUDE_DIR}/architecture/tbb/TBBRouter.h src/architecture/tbb/TBBRouter.cc
            ${AR_INCLUDE_DIR}/architecture/tbb/TBBCalibration.h
            ${AR_INCLUDE_DIR}/architecture/tbb/TBBTimedTest.hh
            ${AR_INCLUDE_DIR}/architecture/tbb/TBBDetector.h src/architecture/tbb/TBBDetector.cc
            ${AR_INCLUDE_DIR}/architecture/tbb/TBBTracker.h src/architecture/tbb/TBBTracker.cc
            ${AR_INCLUDE_DIR}/architecture/tbb/TBBRender.h src/architecture/tbb/TBBRender.cc
            ${AR_INCLUDE_DIR}/RunningStatistics.hh)
target_compile_options(MAR PRIVATE ${FLAGS} )
target_include_directories(MAR PRIVATE ${INCLUDES_DIR} ${ARCH_INCLUDES})
#target_link_libraries(MAR PRIVATE ${log-lib} repository ${SYS_LIBS} ${LIBS})
target_link_libraries(MAR PRIVATE ${log-lib} ${SYS_LIBS} ${LIBS})
#if(COMPILE_SIMPLE_VULKAN_RENDERER)
#   target_link_libraries(MAR PRIVATE simplevulkan)
#endif()
add_dependencies(MAR Shaders)
