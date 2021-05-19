#include <string>
#include <vector>
#include <memory>
#include <sstream>

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

//#include <bulb/AssetReader.hh>

#include "mar/architecture/Architecture.h"
#include "mar/Repository.h"
#include "mar/acquisition/Camera.h"
#include "mar/acquisition/FrameInfo.h"
#include "mar/acquisition/Sensors.h"
#include <mar/util/cv.h>
#include "mar/render/ArchVulkanRenderer.h"

using namespace toMAR;

static Repository* repository = Repository::instance();
static std::string packageName;
static std::shared_ptr<FlowGraphArchitecture> architecture;
static JavaVM* vm = nullptr;
//static jweak classLoader = nullptr;
static jobject classLoader = nullptr;
static AAssetManager* pAssetManager = nullptr;
static jobject assetManagerRef;
AAssetManager* get_asset_manager() { return pAssetManager; }
static jmethodID javaAllocationCopyto = nullptr;
static ANativeWindow* androidWindow = nullptr;
static int orientation =0, androidWindowWidth, androidWindowHeight;
static bool isAprilTags = false,  isFacialRecognition =false;
static FaceRenderType faceRenderType = FaceRenderType::NONE;

JavaVM* getVM() { return vm; }
int get_screen_width() { return androidWindowWidth; }
int get_screen_height() { return androidWindowHeight; }

bool getEnv(JNIEnv*& env)
//--------------
{
   env = nullptr;
   if (vm == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::getEnv", "Call to getEnv before vm initialized");
      return false;
   }

   int status = vm->GetEnv((void **)&env, JNI_VERSION_1_6);
   if (status == JNI_OK)
      return true;
   else
   {
      if (status == JNI_EDETACHED)
      {
         __android_log_print(ANDROID_LOG_WARN, "jni::getEnv", "Thread not attached. Attempting to attach");
         if (vm->AttachCurrentThread(&env, NULL) != 0)
            __android_log_print(ANDROID_LOG_ERROR, "jni::getEnv", "Error attaching thread");
         else
            return true;
      } else if (status == JNI_EVERSION)
         __android_log_print(ANDROID_LOG_ERROR, "jni::getEnv", "Java version not supported");
   }
   return false;
}

extern "C"
jint JNI_OnLoad(JavaVM* vm_, void*)
//--------------------------------
{
	vm = vm_;
   JNIEnv* env;
   if (vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK)
      return JNI_ERR;
   init_class_loader(env);
   repository->javaVM(vm);
   return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT void JNICALL Java_no_pack_drill_ararch_mar_MAR_initialize
   (JNIEnv* env, jobject, jstring packageNameJava, jobject assman)
//-----------------------------------------------------------
{
   const char *psz = env->GetStringUTFChars(packageNameJava, 0);
   packageName = psz;
   env->ReleaseStringUTFChars(packageNameJava, psz);
   assetManagerRef = env->NewGlobalRef(assman);
   pAssetManager = AAssetManager_fromJava(env, assetManagerRef);
   if (pAssetManager == nullptr)
      __android_log_print(ANDROID_LOG_WARN, "jni::Java_no_pack_drill_arach_mar_MAR_initialize",
                                   "Android AssetManager invalid/null.");
   repository->pAssetManager = pAssetManager;
//   bulb::AssetReader& assetReader = bulb::AssetReader::instance();
//   assetReader.set_manager(pAssetManager);
   Sensors& sensor_controller(Sensors::instance());
   sensor_controller.package_name = packageName;
}

extern "C"
JNIEXPORT void JNICALL Java_no_pack_drill_ararch_mar_MAR_setDeviceRotation
   (JNIEnv *, jobject, jint orient)
//---------------------------------------------------------------
{  // 0 degree = 0, 90 degree rotation = 1, 180 degree rotation = 2, 270 degree rotation = 3
   orientation = orient;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_MAR_setSurface
   (JNIEnv* env, jobject, jobject surface, jint w, jint h)
//-----------------------------------------------------------
{
   androidWindow = ANativeWindow_fromSurface(env, surface);
   if ( (androidWindow == nullptr) || (androidWindow == NULL) || (! androidWindow) )
      return JNI_FALSE;
   repository->set_native_window(androidWindow);
   androidWindowWidth = w; //ANativeWindow_getWidth(androidWindow);
   androidWindowHeight = h; //ANativeWindow_getHeight(androidWindow);
   return JNI_TRUE;
}

//#define QUEUE_STATS
#ifdef QUEUE_STATS
tbb::concurrent_unordered_map<unsigned long,
      std::pair<int64_t, RunningStatistics<uint64_t, long double>>> enqueueStats;
#endif

extern "C"
JNIEXPORT jstring JNICALL Java_no_pack_drill_ararch_mar_MAR_getStats
      (JNIEnv* env, jobject inst)
//------------------------------------------------------------------
{
   std::stringstream ss;
   ss << "[\n";
   constexpr long double nano2s = static_cast<long double>(1000000000.0);
   const std::vector<std::shared_ptr<Camera>> &cameras = repository->all_cameras();
   for (auto it = cameras.begin(); it != cameras.end(); ++it)
   {
      std::shared_ptr<Camera> cameraPtr = *it;
      unsigned long id = cameraPtr->camera_id();
      ss << "\"Camera\": { \"id:\" " << id << std::endl;
      RunningStatistics<uint64_t, long double>* pstats = repository->detectionStats(id, false);
      if (pstats)
      {
         ss << "\"detector\": { \"mean\":" << std::fixed << std::setprecision(8)
            << pstats->mean() / nano2s << ",\"deviation\":" << pstats->deviation() / nano2s
               << ",\"count\":" << pstats->size() << "}"
               << ",\"time\":" << pstats->time()/nano2s << "}"
               << ",\"meanframes\":" << (pstats->size() / (pstats->time()/nano2s)) << "}\n";
      }
      pstats = repository->rendererStats(id, false);
      if (pstats)
      {
         ss << "\"renderer\": { \"mean\":" << std::fixed << std::setprecision(8)
            << pstats->mean() / nano2s << ",\"deviation\":" << pstats->deviation() / nano2s
            << ",\"count\":" << pstats->size() << "}"
            << ",\"time\":" << pstats->time()/nano2s << "}"
            << ",\"meanframes\":" << (pstats->size() / (pstats->time()/nano2s)) << "}\n";
      }

#ifdef QUEUE_STATS
      std::pair<int64_t, RunningStatistics<uint64_t, long double>> pp = enqueueStats[id];
      pstats = &pp.second;
      ss << "\"enqueue\": { \"mean\":" << std::fixed << std::setprecision(8)
         << pstats->mean() / nano2s << ",\"deviation\":" << pstats->deviation() / nano2s
         << ",\"meanframes\":" << (nano2s / pstats->mean()) << "}\n";
#endif
      ss << "}\n";
   }
   ss << "]\n";
   std::ofstream ofs("/sdcard/detector-stats.json");
   if (ofs)
   {
      ofs << ss.str() << std::endl;
      ofs.close();
   }
   const std::string sss = ss.str();
   const char* pch = sss.c_str();
   jstring ret = env->NewStringUTF(pch);
   if (env->ExceptionCheck())
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::getStats", "ERROR: unable to convert '%s' to string",
                          pch);
      env->ReleaseStringUTFChars(ret, pch);
      return nullptr;
   }
   return ret;
}

extern "C"
JNIEXPORT jobject JNICALL Java_no_pack_drill_ararch_mar_MAR_allocateBuffer
      (JNIEnv* env, jobject inst, jint size)
//----------------------------------------------------------------------------------
{
   unsigned char* data = new unsigned char[size];
   return env->NewDirectByteBuffer(data, size);
}

extern "C"
JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_HardwareCamera_enqueue
  (JNIEnv* env, jobject instance, jstring cameraid, jboolean isRGBA, jlong ts,
   jint rgbaLen, jbyteArray rgba_arr, jint monoLen, jbyteArray grey_arr)
//----------------------------------------------------------------------------------------------
{
   if (! repository->initialised.load())
      return JNI_TRUE;
   const char *psz = env->GetStringUTFChars(cameraid, 0);
   std::string id = psz;
   env->ReleaseStringUTFChars(cameraid, psz);
   const unsigned long cid = Camera::camera_ID(id);
   Camera* camera = repository->hardware_camera_interface_ptr(cid);
   if (camera == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_enqueue",
                          "Camera %s not defined", id.c_str());
      return JNI_FALSE;
   }

   int w, h;
   camera->get_preview_size(w, h);
   int64_t timestamp = static_cast<int64_t>(ts);
   FrameInfo* frame_info = nullptr;
   jbyteArray rgbaData = reinterpret_cast<jbyteArray>(env->NewGlobalRef(rgba_arr));
   jbyteArray monoData = nullptr;
   if (monoLen > 0)
   {
      monoData = reinterpret_cast<jbyteArray>(env->NewGlobalRef(grey_arr));
      frame_info = new FrameInfo(cid, timestamp, w, h,
                                 (isRGBA) ? ColorFormats::RGBA : ColorFormats::BGRA, vm,
                                 rgbaLen, rgbaData, monoLen, monoData);
   }
   else
      frame_info = new FrameInfo(cid, timestamp, w, h,
                                 (isRGBA) ? ColorFormats::RGBA : ColorFormats::BGRA, vm,
                                 rgbaLen, rgbaData);
//    __android_log_print(ANDROID_LOG_INFO, "jni::enqueue", "FrameInfo instances: %d", FrameInfo::instances());
   if (! camera->enqueue(frame_info))
   {
      env->DeleteGlobalRef(rgbaData);
      if (monoData)
         env->DeleteGlobalRef(monoData);
      __android_log_print(ANDROID_LOG_ERROR, "jni::enqueue",
                          "Error in enqueue of frame for camera %s.", id.c_str());
      return JNI_FALSE;
   }
#ifdef QUEUE_STATS
   unsigned long cno = camera->camera_id();
   std::pair<int64_t, RunningStatistics<uint64_t, long double>> pp = enqueueStats[cno];
   int64_t last_timestamp = pp.first;
   if (last_timestamp > 0)
   {
      long double diff = static_cast<long double>(timestamp - last_timestamp);
      RunningStatistics<uint64_t, long double>& stats = pp.second;
      stats(diff);
//      constexpr long double nano2ms = static_cast<long double>(1000000.0);
//      __android_log_print(ANDROID_LOG_INFO, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_enqueue",
//                          "C++ Enqueue Time: %Lf for camera %lu", diff/nano2ms, cno);
   }
   pp.first = timestamp;
   enqueueStats[cno] = pp;
#endif
   return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_no_pack_drill_ararch_mar_HardwareCamera_enqueueYUV(JNIEnv *env, jobject inst,
                                                        jstring cameraid, jbyteArray YUVJava, jint w,
                                                        jint h, jboolean isRGBA, jlong ts,
                                                        jint rgbaLen, jbyteArray rgbaJavaArr,
                                                        jint monoLen, jbyteArray greyJavaArr)
//------------------------------------------------------------------------------------------------
{
   if (! repository->initialised.load())
      return JNI_TRUE;
   const char *psz = env->GetStringUTFChars(cameraid, 0);
   std::string id = psz;
   env->ReleaseStringUTFChars(cameraid, psz);
   const unsigned long cid = Camera::camera_ID(id);
   Camera* camera = repository->hardware_camera_interface_ptr(cid);
   if (camera == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_enqueueYUV",
                          "Camera %s not defined", id.c_str());
      return JNI_FALSE;
   }

   void* YUVData = env->GetPrimitiveArrayCritical(YUVJava, 0);
   if (YUVData == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_enqueueYUV",
                          "Could not pin YUVJava parameter");
      return JNI_FALSE;
   }
   void* outputJavaRGB = env->GetPrimitiveArrayCritical(rgbaJavaArr, 0);
   if (outputJavaRGB == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::enqueueYUV", "Could not pin outputJavaRGB parameter");
      env->ReleasePrimitiveArrayCritical(YUVJava, YUVData, 0);
      return JNI_FALSE;
   }

   if (! toMAR::vision::YUV2RGBA(YUVData, outputJavaRGB, w, h, (isRGBA == JNI_TRUE), "jni::enqueueYUV"))
   {
      env->ReleasePrimitiveArrayCritical(rgbaJavaArr, outputJavaRGB, 0);
      env->ReleasePrimitiveArrayCritical(YUVJava, YUVData, 0);
      return JNI_FALSE;
   }
   env->ReleasePrimitiveArrayCritical(rgbaJavaArr, outputJavaRGB, 0);
   if ( (monoLen > 0) && (greyJavaArr != nullptr) )
   {
      void* outputJavaGrey = env->GetPrimitiveArrayCritical(greyJavaArr, 0);
      if (! toMAR::vision::YUV2Mono(YUVData, outputJavaGrey, w, h, "jni::enqueueYUV"))
         monoLen = 0;
      env->ReleasePrimitiveArrayCritical(greyJavaArr, outputJavaGrey, 0);
   }
   env->ReleasePrimitiveArrayCritical(YUVJava, YUVData, 0);

   int64_t timestamp = static_cast<int64_t>(ts);
   FrameInfo* frame_info = nullptr;
   jbyteArray rgbaData = reinterpret_cast<jbyteArray>(env->NewGlobalRef(rgbaJavaArr));
   jbyteArray monoData = nullptr;
   if (monoLen > 0)
   {
      monoData = reinterpret_cast<jbyteArray>(env->NewGlobalRef(greyJavaArr));
      frame_info = new FrameInfo(cid, timestamp, w, h,
                                 (isRGBA) ? ColorFormats::RGBA : ColorFormats::BGRA, vm,
                                 rgbaLen, rgbaData, monoLen, monoData);
   }
   else
      frame_info = new FrameInfo(cid, timestamp, w, h,
                                 (isRGBA) ? ColorFormats::RGBA : ColorFormats::BGRA, vm,
                                 rgbaLen, rgbaData);
//   __android_log_print(ANDROID_LOG_INFO, "jni::enqueue", "FrameInfo instances: %d", FrameInfo::instances());
   if (! camera->enqueue(frame_info))
   {
      env->DeleteGlobalRef(rgbaData);
      if (monoData)
         env->DeleteGlobalRef(monoData);
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_enqueueYUV",
                          "Error in enqueue of frame for camera %s.", id.c_str());
      return JNI_FALSE;
   }
#ifdef QUEUE_STATS
   unsigned long cno = camera->camera_id();
   std::pair<int64_t, RunningStatistics<uint64_t, long double>> pp = enqueueStats[cno];
   int64_t last_timestamp = pp.first;
   if (last_timestamp > 0)
   {
      long double diff = static_cast<long double>(timestamp - last_timestamp);
      RunningStatistics<uint64_t, long double>& stats = pp.second;
      stats(diff);
//      constexpr long double nano2ms = static_cast<long double>(1000000.0);
//      __android_log_print(ANDROID_LOG_INFO, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_enqueueYUV",
//                          "C++ Enqueue Time: %Lf for camera %lu", diff/nano2ms, cno);
   }
   pp.first = timestamp;
   enqueueStats[cno] = pp;
#endif
   return JNI_TRUE;
}

/*
 JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_HardwareCamera_addCamera
  (JNIEnv *, jobject, jstring, jint, jboolean);
 */

extern "C"
JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_HardwareCamera_addCamera
  (JNIEnv* env, jobject inst, jstring cameraid, jint queueSize, jboolean isRearFacing)
//--------------------------------------------------------------------------
{
   const char *psz = env->GetStringUTFChars(cameraid, 0);
   std::string id = psz;
   env->ReleaseStringUTFChars(cameraid, psz);
   if (! repository->add_camera(id, queueSize, (isRearFacing == JNI_TRUE)))
   {
      __android_log_print(ANDROID_LOG_WARN, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_addCamera",
                          "Camera %s previously added", id.c_str());
      return JNI_FALSE;
   }
   repository->next_seqno(Camera::camera_ID(id));
   return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_MAR_addSensors
   (JNIEnv* env, jobject, jintArray sensorArray)
//---------------------------------------------------------------
{
   jsize no = env->GetArrayLength(sensorArray);
   jint* sensors = env->GetIntArrayElements(sensorArray, 0);
   std::vector<int> sensorVec;
   for (auto i=0; i<no; ++i)
      sensorVec.push_back(sensors[i]);
   env->ReleaseIntArrayElements(sensorArray, sensors, 0);
   std::stringstream errs;
   Sensors& sensor_controller = Sensors::instance();
   for (int sensor : sensorVec)
   {
      if (! sensor_controller.add_sensor(sensor, 1000, &errs))
      {
         __android_log_print(ANDROID_LOG_WARN, "jni::Java_no_pack_drill_arach_mar_MAR_addSensors",
                             "Error %s", errs.str().c_str());
         return JNI_FALSE;
      }
   }
   return JNI_TRUE;
}


extern "C"
JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_HardwareCamera_setPreviewSize
  (JNIEnv* env, jobject, jstring cameraid, jint previewWidth, jint previewHeight)
//-----------------------------------------------------------------------------
{
   const char *psz = env->GetStringUTFChars(cameraid, 0);
   std::string id = psz;
   env->ReleaseStringUTFChars(cameraid, psz);
   Camera* camera = repository->hardware_camera_interface_ptr(Camera::camera_ID(id));
   if (camera == nullptr)
   {
      __android_log_print(ANDROID_LOG_WARN, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_setPreviewSize", "Camera %s not defined",
                          id.c_str());
      return JNI_FALSE;
   }
   camera->preview_size(previewWidth, previewHeight);
   return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_no_pack_drill_ararch_mar_HardwareCamera_clearQueue(JNIEnv *env, jobject inst,
                                                        jstring cameraId)
//-----------------------------------------------------------------------------------------------
{
   const char *psz = env->GetStringUTFChars(cameraId, 0);
   std::string id = psz;
   env->ReleaseStringUTFChars(cameraId, psz);
   unsigned long cid = Camera::camera_ID(id);
   Camera* camera = repository->hardware_camera_interface_ptr(cid);
   if (camera == nullptr)
   {
      __android_log_print(ANDROID_LOG_WARN, "jni::Java_no_pack_drill_arach_mar_HardwareCamera_clearQueue", "Camera %s not defined",
                          id.c_str());
      return JNI_FALSE;
   }
   camera->clear();
   repository->clear_queued(cid);
   return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_MAR_startMAR
  (JNIEnv* env, jobject, jint rendererType, jboolean aprilTagDetect, jboolean facialRecog)
//--------------------------------------------------------------
{
   if (androidWindow == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_MAR_startMAR",
                          "Android window not yet set.");
      return JNI_FALSE;
   }

   int texWidth =-1, texHeight =1;
   std::vector<std::shared_ptr<Camera>> rear_cameras = repository->rear_cameras();
   std::vector<std::shared_ptr<Camera>> front_cameras = repository->front_cameras();
   if ( (rear_cameras.empty()) && (front_cameras.empty()) )
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_MAR_startMAR",
                          "No cameras");
      return JNI_FALSE;
   }
#ifdef HAS_FACE_DETECTION
   isFacialRecognition = (facialRecog == JNI_TRUE);
   if ( (isFacialRecognition) && (! toMAR::vision::init_faces(nullptr)) )
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_MAR_startMAR",
                          "Could not initialise face detection (check cascade files).");
      return JNI_FALSE;
   }
#endif
   if (! rear_cameras.empty())
   {
      std::shared_ptr<Camera> camera_sp = rear_cameras[0];
      camera_sp->get_preview_size(texWidth, texHeight);
   }
   else
   {
      std::shared_ptr<Camera> camera_sp = front_cameras[0];
      camera_sp->get_preview_size(texWidth, texHeight);
   }

   int screenWidth = get_screen_width(), screenHeight = get_screen_height();
   RendererFactory& renderer_factory = RendererFactory::instance();
   Renderer* renderer = nullptr;
#ifdef HAS_BULB
   if (rendererType == 1)
      renderer = renderer_factory.make_bulb_renderer(androidWindow, "render", screenWidth, screenHeight,
                                                     texWidth, texHeight, 60.0, 0.0625, 20.0, "App",
                                                     filament::Engine::Backend::OPENGL);
   else if (rendererType == 2)
      renderer = renderer_factory.make_bulb_renderer(androidWindow, "render", screenWidth, screenHeight,
                                                     texWidth, texHeight, 60.0, 0.0625, 20.0, "App",
                                                     filament::Engine::Backend::VULKAN);

#endif
#ifdef HAS_SIMPLE_RENDERER
#ifdef HAS_FACE_DETECTION
   if (isFacialRecognition)
   {
      if ( (! rear_cameras.empty()) && (! front_cameras.empty()) )
         faceRenderType = FaceRenderType::OVERLAY;
      else if ( (rear_cameras.empty()) && (! front_cameras.empty()) )
         faceRenderType = FaceRenderType::BB;
      else
         faceRenderType = FaceRenderType::BB;
   }
   else
      faceRenderType = FaceRenderType::NONE;
#else
   isFacialRecognition = false;
   faceRenderType = FaceRenderType::NONE;
#endif
#ifdef HAS_APRILTAGS
   isAprilTags = (aprilTagDetect == JNI_TRUE);
#else
   isAprilTags = false;
#endif
   if (rendererType == 0)
      renderer = renderer_factory.make_vulkan_renderer(0, "TBB", androidWindow, nullptr, "render",
            texWidth, texHeight, "App", isAprilTags, faceRenderType,  false);
#endif
   if (renderer == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_MAR_startMAR",
                          "No renderer available/Error creating renderer type %d", rendererType);
      return JNI_FALSE;
   }
   DetectorType rearDetectorType = DetectorType::NONE, frontDetectorType = DetectorType::NONE;
   if (isAprilTags)
      rearDetectorType = DetectorType::APRILTAGS;
   if (isFacialRecognition)
      frontDetectorType = DetectorType::FACE_RECOGNITION;
   architecture.reset(make_architecture("TBB", renderer, rear_cameras, front_cameras,
                                        rearDetectorType, frontDetectorType,
                                        TrackerType::NONE, TrackerType::NONE,
                                        RendererType::BENCHMARK));
   if (! architecture)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_MAR_startMAR", "Error creating parallel architecture");
      return JNI_FALSE;
   }
   if (! architecture->start())
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_arach_mar_MAR_startMAR", "Error starting parallel architecture");
      return JNI_FALSE;
   }
   return JNI_TRUE;
}

extern "C"
JNIEXPORT void JNICALL Java_no_pack_drill_ararch_mar_MAR_stopMAR
      (JNIEnv* env, jobject inst)
{
   repository->must_terminate.store(true);
   repository->initialised.store(false);
   if (architecture)
      architecture->stop();
}

extern "C"
JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_CPUFrameHandler_CPUConvertYUV
   (JNIEnv* env, jobject inst, jbyteArray YUVJava, jint w, jint h, jboolean isRGBA,
    jbyteArray rgbaJavaArr, jbyteArray greyJavaArr)
//--------------------------------------------------------------
{
//   const size_t greyLen = static_cast<const size_t>(w * h), rgbaLen = greyLen * 4;
   void* YUVData = env->GetPrimitiveArrayCritical(YUVJava, 0);
   if (YUVData == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_ararch_mar_CPUFrameHandler_CPUConvertYUV",
            "Could not pin YUVJava parameter");
      return JNI_FALSE;
   }
   void* outputJavaRGB = env->GetPrimitiveArrayCritical(rgbaJavaArr, 0);
   if (outputJavaRGB == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::Java_no_pack_drill_ararch_mar_CPUFrameHandler_CPUConvertYUV",
                          "Could not pin outputJavaRGB parameter");
      env->ReleasePrimitiveArrayCritical(YUVJava, YUVData, 0);
      return JNI_FALSE;
   }
   if (! toMAR::vision::YUV2RGBA(YUVData, outputJavaRGB, w, h, (isRGBA == JNI_TRUE), "jni::CPUConvertYUV"))
   {
      env->ReleasePrimitiveArrayCritical(rgbaJavaArr, outputJavaRGB, 0);
      env->ReleasePrimitiveArrayCritical(YUVJava, YUVData, 0);
      return JNI_FALSE;
   }
   env->ReleasePrimitiveArrayCritical(rgbaJavaArr, outputJavaRGB, 0);
   if (greyJavaArr != nullptr)
   {
      void* outputJavaGrey = env->GetPrimitiveArrayCritical(greyJavaArr, 0);
      if (outputJavaGrey == nullptr)
         __android_log_print(ANDROID_LOG_ERROR, "jni::CPUConvertYUV", "Could not pin outputJavaGrey parameter");
      else
      {
         toMAR::vision::YUV2Mono(YUVData, outputJavaGrey, w, h, "jni::enqueueYUV");
         env->ReleasePrimitiveArrayCritical(greyJavaArr, outputJavaGrey, 0);
      }
   }
   env->ReleasePrimitiveArrayCritical(YUVJava, YUVData, 0);
   return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_no_pack_drill_ararch_mar_CPUFrameHandler_CPUConvertNV21
   (JNIEnv* env, jobject inst, jobject Ybuf, jobject Ubuf,
   jobject Vbuf, jint w, jint h, jboolean isRGBA, jbyteArray rgbaJavaArr,
   jbyteArray greyJavaArr)
//--------------------------------------------------------------------------------
{
   const size_t greyLen = static_cast<const size_t>(w * h), rgbaLen = greyLen * 4;
   uchar* Y = (uchar*) env->GetDirectBufferAddress(Ybuf);
   uchar* U = (uchar*) env->GetDirectBufferAddress(Ubuf);
   uchar* V = (uchar*) env->GetDirectBufferAddress(Vbuf);
   void* outputJavaRGB = env->GetPrimitiveArrayCritical(rgbaJavaArr, 0);
   void* outputJavaGrey = (env->IsSameObject(greyJavaArr, nullptr))
                          ? nullptr : env->GetPrimitiveArrayCritical(greyJavaArr, 0);
   jboolean ret = JNI_TRUE;
   if (! toMAR::vision::NV2RGBA(Y, U, V, w, h, (isRGBA == JNI_TRUE), outputJavaRGB, outputJavaGrey,
         "jni::CPUConvertNV21"))
      ret = JNI_FALSE;
   env->ReleasePrimitiveArrayCritical(rgbaJavaArr, outputJavaRGB, 0);
   if (outputJavaGrey != nullptr)
      env->ReleasePrimitiveArrayCritical(greyJavaArr, outputJavaGrey, 0);
   return ret;
}

std::string stack_dump(JNIEnv* env, std::string message)
//--------------------------------
{
   if ( (env == nullptr) && (vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK) )
      return message;
   jthrowable exc;
   exc = env->ExceptionOccurred();
   if (exc == nullptr)
      return message;
   jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
   jstring s = (jstring) env->CallObjectMethod(exc, toString);
   const char* dump = env->GetStringUTFChars(s, 0);
   std::string ss = message + dump;
   env->ReleaseStringUTFChars(s, dump);
   return ss;
}

bool init_class_loader(JNIEnv *env)
//-------------------------------
{
   if ( (env == nullptr) && (vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK) )
      return false;
   jclass javaLangThread = env->FindClass("java/lang/Thread");
   if (javaLangThread == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::init_class_loader", "WTF Could not find java/lang/Thread.");
      return false;
   }
  jclass javaLangClassLoader = env->FindClass("java/lang/ClassLoader");
   if (javaLangClassLoader == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::init_class_loader", "WTF Could not find java/lang/ClassLoader");
      return false;
    }
   jmethodID currentThread = env->GetStaticMethodID(javaLangThread, "currentThread", "()Ljava/lang/Thread;");
   if (currentThread == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::init_class_loader", "WTF Could not get currentThread method");
      return false;
   }
   jmethodID getContextClassLoader = env->GetMethodID( javaLangThread, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
   if (getContextClassLoader == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::init_class_loader", "WTF Could not get getContextClassLoader method");
      return false;
   }
   jobject thread = env->CallStaticObjectMethod(javaLangThread, currentThread);
   if (thread == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::init_class_loader", "WTF Could not get thread with currentThread method");
      return false;
   }
   jobject loader = env->CallObjectMethod(thread, getContextClassLoader);
   if (loader == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::init_class_loader", "WTF Could not load ClassLoader with getContextClassLoader");
      return false;
   }
   //classLoader = env->NewWeakGlobalRef(loader);
   classLoader = env->NewGlobalRef(loader);
   return true;
}

jclass find_class_with_classloader(JNIEnv* env, const char* className)
//--------------------------------------------------------------------
{
   if ( (env == nullptr) && (vm->GetEnv((void**) &env, JNI_VERSION_1_6) != JNI_OK) )
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::find_class_with_classloader", "%s", "Error getting env");
      return nullptr;
   }
   if (env->ExceptionCheck())
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::find_class_with_classloader", "%s", "Pending exception");
      return nullptr;
   }

   if (classLoader == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::find_class_with_classloader", "%s", "classloader null/not found");
      if ( (!init_class_loader(env)) && (!init_class_loader(nullptr)) )
         return env->FindClass(className);
   }

  // JNI FindClass uses class names with slashes, but
  // ClassLoader.loadClass uses the dotted "binary name"
  // format. Convert formats.
   std::string convName = className;
   std::replace(convName.begin(), convName.end(), '/', '.');

   jclass javaLangClassLoader = env->FindClass("java/lang/ClassLoader");
   if (javaLangClassLoader == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::find_class_with_classloader", "%s", "Could not find ClassLoader");
      return env->FindClass(className);
   }
   jmethodID loadClass = env->GetMethodID(javaLangClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
   if (loadClass == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::find_class_with_classloader", "%s", "Could not find ClassLoader.loadClass");
      return env->FindClass(className);
   }

   const char* pch = convName.c_str();
   jstring strClassName = env->NewStringUTF(pch);
   if (env->ExceptionCheck())
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::find_class_with_classloader", "ERROR: unable to convert '%s' to string",
                          pch);
      env->ReleaseStringUTFChars(strClassName, pch);
      return nullptr;
   }
   jclass cls = (jclass) env->CallObjectMethod(classLoader, loadClass, strClassName);
   env->ReleaseStringUTFChars(strClassName, pch);
   if (env->ExceptionCheck())
   {
      env->ExceptionDescribe();
      __android_log_print(ANDROID_LOG_ERROR, "jni::find_class_with_classloader",
                          "ERROR: unable to load class '%s' using main thread classloader",  className);
      return env->FindClass(className);
   }
   return cls;
}

extern "C"
JNIEXPORT jint JNICALL
Java_no_pack_drill_ararch_mar_HardwareCamera_inFlight(JNIEnv *env, jobject thiz)
{
   return FrameInfo::instances();
}
