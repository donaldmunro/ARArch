#ifndef MAR_HARDWARECAMERA_H
#define MAR_HARDWARECAMERA_H

#include <string>
#include <unordered_map>
#include <memory>

#include <camera/NdkCameraManager.h>
#include <media/NdkImageReader.h>
#include <camera/NdkCameraDevice.h>
#include <media/NdkImageReader.h>
#include <android/native_window.h>
#include <cpp/RenderScript.h>
#include <cpp/rsCppStructs.h>
#include "ScriptC_yuv2grey.h"
#include "ScriptC_yuv2rgba.h"

#include <android/log.h>

#define LOG_TAG "HardwareCamera"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ASSERT(cond, fmt, ...)                                \
  if (!(cond)) {                                              \
    __android_log_assert(#cond, LOG_TAG, fmt, ##__VA_ARGS__); \
  }

struct ManagerDeleter { void operator()(ACameraManager* p) const { if (p != nullptr) ACameraManager_delete(p);};};
struct MetaDeleter { void operator()(ACameraMetadata* p) const {if (p != nullptr) ACameraMetadata_free(p);};};
struct DeviceDeleter { void operator()(ACameraDevice* p) const {if (p != nullptr) ACameraDevice_close(p);};};
struct ImageReaderDeleter { void operator()(AImageReader* p) const {if (p != nullptr) AImageReader_delete(p);};};
struct SessionOutputContainerDeleter { void operator()(ACaptureSessionOutputContainer* p) const {if (p != nullptr) ACaptureSessionOutputContainer_free(p);};};
struct SessionOutputDeleter { void operator()(ACaptureSessionOutput* p) const {if (p != nullptr) ACaptureSessionOutput_free(p);};};
struct SessionDeleter { void operator()(ACameraCaptureSession* p) const {if (p != nullptr) { ACameraCaptureSession_abortCaptures(p); ACameraCaptureSession_close(p); } };};
struct TargetDeleter { void operator()(ACameraOutputTarget* p) const {if (p != nullptr) ACameraOutputTarget_free(p); }; };

class HardwareCamera
{
public:
   explicit HardwareCamera(ACameraManager *manager = nullptr)
   {
      (manager == nullptr) ? camera_manager.reset(ACameraManager_create(), camera_deleter) : camera_manager.reset(manager, camera_deleter);
   }

   explicit HardwareCamera(std::shared_ptr<ACameraManager> manager) { camera_manager = manager; }

   bool open(const std::string& id);
   bool start_preview(std::string string, int w, int h);
   bool opened() {return is_open; }
   bool previewing() {return is_previewing; }
   void stop_preview();

protected:
   void on_camera_disconnect();
   void on_error(int err);
   void on_image_available(AImageReader *reader);
   void on_session_active(ACameraCaptureSession *session) { }
   void on_session_ready(ACameraCaptureSession *session) {}
   void on_session_closed(ACameraCaptureSession *session) {}

private:
   ManagerDeleter camera_deleter;
   std::string camera_id;
   bool is_open = false, is_previewing = false;
   int rgba_size =0, camera_width =-1, camera_height =-1;
   std::unique_ptr<unsigned char> rgba_data;
   std::shared_ptr<ACameraManager> camera_manager;
   std::unique_ptr<ACameraMetadata, MetaDeleter> metadata;
   DeviceDeleter device_deleter;
   std::unique_ptr<ACameraDevice, DeviceDeleter> camera_device{nullptr, device_deleter};
   ImageReaderDeleter img_deleter;
   std::unique_ptr<AImageReader, ImageReaderDeleter> img_reader{nullptr, img_deleter};
   SessionOutputContainerDeleter session_output_container_deleter;
   std::unique_ptr<ACaptureSessionOutputContainer, SessionOutputContainerDeleter> yuv_outputs{nullptr, session_output_container_deleter};
   SessionOutputDeleter session_output_deleter;
   std::unique_ptr<ACaptureSessionOutput, SessionOutputDeleter> yuv_output{nullptr, session_output_deleter};
   ACaptureRequest *request;
   ACameraDevice_stateCallbacks device_listener;
   ACameraCaptureSession_stateCallbacks session_listener;
   AImageReader_ImageListener image_listener;
   ANativeWindow *surface;
   SessionDeleter session_deleter;
   std::unique_ptr<ACameraCaptureSession, SessionDeleter> session{nullptr, session_deleter };
   TargetDeleter target_deleter;
   std::unique_ptr<ACameraOutputTarget, TargetDeleter> output_target{nullptr, target_deleter };
   android::RSC::sp<android::RSC::RS> renderscript;
   android::RSC::sp<android::RSC::Allocation> yuv_alloc, rgba_alloc, grey_alloc;
   std::unique_ptr<ScriptC_yuv2rgba> yuv2rgba_script;

   static void onDisconnected(void* context, ACameraDevice* device);
   static void onError(void* context, ACameraDevice* device, int error);
   static void onSessionActive(void* context, ACameraCaptureSession *session);
   static void onSessionReady(void* context, ACameraCaptureSession *session);
   static void onSessionClosed(void* context, ACameraCaptureSession *session);
   static void onImageCallback(void *context, AImageReader *reader);
};


#endif //MAR_HARDWARECAMERA_H
