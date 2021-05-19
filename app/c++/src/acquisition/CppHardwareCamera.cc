#include "HardwareCamera.h"

#include <opencv2/core/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include "util.h"

bool HardwareCamera::open(const std::string& id)
//----------------------------------------------
{
   ACameraManager* manager = camera_manager.get();
   ACameraIdList *camera_list = nullptr;
   if (ACameraManager_getCameraIdList(manager, &camera_list) == ACAMERA_OK)
   {
      auto list_deleter = [](ACameraIdList* p){ if (p != nullptr) ACameraManager_deleteCameraIdList(p); };
      std::unique_ptr<ACameraIdList, decltype(list_deleter)> cameras(camera_list, list_deleter);
      for (int i = 0; i < cameras->numCameras; ++i)
      {
         const std::string cid(cameras->cameraIds[i]);
         ACameraMetadata* metadata_p;
         if (ACameraManager_getCameraCharacteristics(manager, cid.c_str(), &metadata_p) == ACAMERA_OK)
         {
            MetaDeleter deleter;
            std::unique_ptr<ACameraMetadata, MetaDeleter> meta_data(metadata_p, deleter);
            if (! id.empty())
            {
               if (id == cid)
               {
                  camera_id = id;
                  metadata = std::move(meta_data);
                  break;
               }
               else
                  continue;
            }
            int32_t count = 0;
            const uint32_t* tags = nullptr;
            ACameraMetadata_getAllTags(meta_data.get(), &count, &tags);
            for (int tagIdx = 0; tagIdx < count; ++tagIdx)
            {
               if (tags[tagIdx] == ACAMERA_LENS_FACING_BACK)
               {
                  ACameraMetadata_const_entry lensInfo = { 0 };
                  if (ACameraMetadata_getConstEntry(meta_data.get(), tags[tagIdx], &lensInfo) == ACAMERA_OK)
                  {
                     auto facing = static_cast<acamera_metadata_enum_android_lens_facing_t>(lensInfo.data.u8[0]);
                     if (facing == ACAMERA_LENS_FACING_BACK)
                     {
                        camera_id = cid;
                        metadata = std::move(meta_data);
                        break;
                     }
                  }
               }
            }
         }
      }
      if (camera_id.empty())
         return false;
   }
   else
      return false;
   if (camera_id.empty())
      return false;
   device_listener.context = static_cast<void *>(this);
   device_listener.onDisconnected = &HardwareCamera::onDisconnected;
   device_listener.onError = &HardwareCamera::onError;
   ACameraDevice *cameradevice = nullptr;
   if ( ACameraManager_openCamera(manager, camera_id.c_str(), &device_listener, &cameradevice) != ACAMERA_OK)
   {
      camera_id = "";
      metadata.reset();
      return false;
   }
   camera_device.reset(cameradevice);
   is_open = true;
   return true;
}

bool HardwareCamera::start_preview(std::string dir, int w, int h)
//---------------------------------------------------
{
   if (is_previewing)
      stop_preview();
   if ( (! is_open) || (! camera_device) )
   {
      LOGE("HardwareCamera::start_preview: Camera not initialized before start_preview called");
      return false;
   }
   // AImageReader wouldn't be necessary if yuv_alloc->getSurface() worked in the NDK.
   AImageReader *imgreader;
   if (AImageReader_new(w, h, AIMAGE_FORMAT_YUV_420_888, 2, &imgreader) != AMEDIA_OK)
   {
      LOGE("HardwareCamera::start_preview: Could not create image reader for AIMAGE_FORMAT_YUV_420_888 of size 2");
      return false;
   }
   img_reader.reset(imgreader);
   image_listener.context = this; image_listener.onImageAvailable = onImageCallback;
   if (AImageReader_setImageListener(imgreader, &image_listener) != AMEDIA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error setting listener for ImageReader");
      img_reader.reset(nullptr);
      return false;
   }
   if (AImageReader_getWindow(imgreader, &surface) != AMEDIA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error getting surface from ImageReader");
      img_reader.reset(nullptr);
      return false;
   }

   if (renderscript.get() != nullptr)
      renderscript->finish();
   renderscript.clear();
   renderscript = new android::RSC::RS();
   if (renderscript->init(dir.c_str()))
   {
      yuv2rgba_script.reset(new ScriptC_yuv2rgba(renderscript));
      android::RSC::sp<const android::RSC::Element> yuv_element = android::RSC::Element::YUV(renderscript);
      android::RSC::sp<const android::RSC::Type> yuv_type = android::RSC::Type::create(renderscript, yuv_element, w, h, 0);
      yuv_alloc.clear();
      yuv_alloc = android::RSC::Allocation::createTyped(renderscript, yuv_type, RS_ALLOCATION_MIPMAP_NONE,
                                                        RS_ALLOCATION_USAGE_IO_INPUT | RS_ALLOCATION_USAGE_SCRIPT);
   //   android::RSC::sp<android::Surface> yuv_surface = yuv_alloc->getSurface(); //Surface not defined anywhere (forward declaration in toolchains/renderscript/prebuilt/linux-x86_64/platform/rs/cpp/rsCppStructs.h)
   //   android::Surface* yuv_surface = yuv_alloc->getSurface().get();      

      android::RSC::sp<const android::RSC::Element> rgba_element = android::RSC::Element::RGBA_8888(renderscript);
      android::RSC::sp<const android::RSC::Type> rgba_type = android::RSC::Type::create(renderscript, rgba_element, w, h, 0);
      rgba_alloc.clear();
      rgba_alloc = android::RSC::Allocation::createTyped(renderscript, rgba_type, RS_ALLOCATION_MIPMAP_NONE,
                                                         RS_ALLOCATION_USAGE_SHARED | RS_ALLOCATION_USAGE_SCRIPT);
      android::RSC::sp<const android::RSC::Element> grey_element = android::RSC::Element::U8(renderscript);
      android::RSC::sp<const android::RSC::Type> grey_type = android::RSC::Type::create(renderscript, grey_element, w, h, 0);
      grey_alloc = android::RSC::Allocation::createTyped(renderscript, grey_type, RS_ALLOCATION_MIPMAP_NONE,
                                                         RS_ALLOCATION_USAGE_SHARED | RS_ALLOCATION_USAGE_SCRIPT);
   }
   else
   {
      LOGE("HardwareCamera::start_preview: Error initializing renderscript with directory %s", dir.c_str());
      renderscript.clear();
   }

   ACameraDevice* device = camera_device.get();
   session_listener.context = this;
   session_listener.onActive = onSessionActive; session_listener.onReady = onSessionReady;session_listener.onClosed = onSessionClosed;
   ACaptureSessionOutputContainer* outputs;
   if (ACaptureSessionOutputContainer_create(&outputs) != ACAMERA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error creating session output container.");
      stop_preview();
      return false;
   }
   yuv_outputs.reset(outputs);
   ACaptureSessionOutput* output;
   if (ACaptureSessionOutput_create(surface, &output)!= ACAMERA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error creating session output.");
      stop_preview();
      return false;
   }
   yuv_output.reset(output);
   if (ACaptureSessionOutputContainer_add(outputs, output) != ACAMERA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error adding session output.");
      stop_preview();
      return false;
   }
   ACameraOutputTarget* target;
   if (ACameraOutputTarget_create(surface, &target) != ACAMERA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error creating target.");
      stop_preview();
      return false;
   }
   output_target.reset(target);
   if (ACameraDevice_createCaptureRequest(device, TEMPLATE_RECORD, &request) != ACAMERA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error creating capture request");
      stop_preview();
      return false;
   }
   if (ACaptureRequest_addTarget(request, target) != ACAMERA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error binding target to capture request");
      stop_preview();
      return false;
   }
   ACameraCaptureSession* sess;
   if (ACameraDevice_createCaptureSession(device, outputs, &session_listener, &sess) != ACAMERA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error creating session output.");
      stop_preview();
      return false;
   }
   session.reset(sess);
   if (ACameraCaptureSession_setRepeatingRequest(sess, nullptr, 1, &request, nullptr) != ACAMERA_OK)
   {
      LOGE("HardwareCamera::start_preview: Error starting repeat requests.");
      stop_preview();
      return false;
   }
   is_previewing = true;
   rgba_size = w*h*4;
   camera_width = w; camera_height = h;
   rgba_data.reset(new unsigned char[rgba_size]);
   return true;
}

void HardwareCamera::stop_preview()
//--------------------------------
{
   session.reset(nullptr);
   output_target.reset(nullptr);
   img_reader.reset(nullptr);
   yuv_outputs.reset(nullptr);
   yuv_output.reset(nullptr);
   if (renderscript.get() != nullptr)
      renderscript->finish();
   renderscript.clear();
   rgba_size = 0;
   camera_width = camera_height = -1;
   is_previewing = false;
}

void HardwareCamera::on_camera_disconnect()
//-------------------------------
{
   LOGE("Camera %s disconnected", camera_id.c_str());
   if (is_previewing)
      stop_preview();
   camera_device.reset(nullptr);
   metadata.reset(nullptr);
   is_open = false;
}

void HardwareCamera::on_error(int err) { LOGE("Camera %s error %d", camera_id.c_str(), err); }

void HardwareCamera::on_image_available(AImageReader *reader)
//--------------------------------------------------------
{
   static int badcount = 0;
   int32_t format;
   media_status_t status = AImageReader_getFormat(reader, &format);
   if ( (status == AMEDIA_OK) && (format == AIMAGE_FORMAT_YUV_420_888) )
   {
      AImage *image;
      status = AImageReader_acquireNextImage(reader, &image);
      if (status == AMEDIA_OK)
      {
         uint8_t *Y = nullptr, *U = nullptr, *V = nullptr;
         int ylen = 0, ulen = 0, vlen =0;
         media_status_t ystatus = AImage_getPlaneData(image, 0, &Y, &ylen);
         media_status_t ustatus = AImage_getPlaneData(image, 1, &U, &ulen);
         media_status_t vstatus = AImage_getPlaneData(image, 2, &V, &vlen);
         if ( (ystatus != AMEDIA_OK) || (ylen <= 0) || (ustatus != AMEDIA_OK) || (ulen <= 0) || (vstatus != AMEDIA_OK) || (vlen <= 0) )
         {
            if (badcount++ < 200)
               LOGE("HardwareCamera::on_image_available: AImage_getPlaneData failed (%d %d %d", ystatus, ustatus, vstatus);
         }
         else
         {
            int64_t ts;
            AImage_getTimestamp(image, &ts);
            int32_t width, height, ystride, ustride, vstride;
            AImage_getHeight(image, &height);
            AImage_getWidth(image, &width);
            AImage_getPlaneRowStride(image, 0, &ystride);
            AImage_getPlaneRowStride(image, 1, &ustride);
            AImage_getPlaneRowStride(image, 2, &vstride);
            LOGW("HardwareCamera::on_image_available: height: %d, stride: %d", height, ystride);
//            std::unique_ptr<unsigned char> rgb_data(mar::util::cpuYUV2RGB(Y, U, V, ylen, ulen, ystride, height));
            std::unique_ptr<unsigned char> rgb_data(mar::util::YUVRaw(Y, U, V, width, height,
                                                                      ylen, ulen, vlen, ustride, vstride));
            cv::Mat yuv(camera_height + camera_height/2, camera_width, CV_8UC1, rgb_data.get());
            cv::Mat rgb;
            cv::cvtColor(yuv, rgb, cv::COLOR_YUV420p2BGR);
            // cv::Mat rgb(camera_height, camera_width, CV_8UC3, rgb_data.get());
            cv::imwrite("/sdcard/framebuffer.png", rgb);
/*
            Image.Plane Yplane = planes[0];
            Image.Plane Uplane = planes[1];
            Image.Plane Vplane = planes[2];
            ByteBuffer Y = Yplane.getBuffer();
            ByteBuffer U = Uplane.getBuffer();
            ByteBuffer V = Vplane.getBuffer();
            Y.rewind();
            U.rewind();
            V.rewind();

 */

//            yuv_alloc->copy1DFrom(data); // This wouldn't be necessary if yuv_alloc->getSurface() worked in the NDK and the AImageReader was dropped
//            yuv2rgba_script->set_in(yuv_alloc);
//            yuv2rgba_script->forEach_yuv2rgba(rgba_alloc);
//            rgba_alloc->copy1DTo(rgba_data.get());
//            cv::Mat rgba(camera_height, camera_width, CV_8UC4, image);
//            cv::imwrite("/sdcard/framebuffer.png", rgba);
         }
         AImage_delete(image);
      }
      else
         if (badcount++ < 200)
            LOGE("HardwareCamera::on_image_available: AImageReader_acquireNext/LatestImage error %d", status);
   }
   else
   if (badcount++ < 200)
      LOGE("HardwareCamera::on_image_available: AImageReader_getFormat error %d format %d", status, format);

}

void HardwareCamera::onDisconnected(void *context, ACameraDevice *device)
{
   HardwareCamera* camera = static_cast<HardwareCamera*>(context);
   camera->on_camera_disconnect();
}

void HardwareCamera::onError(void *context, ACameraDevice *device, int error)
{
   HardwareCamera* camera = static_cast<HardwareCamera*>(context);
   camera->on_error(error);
}

void HardwareCamera::onImageCallback(void *context, AImageReader* reader)
{
   HardwareCamera* camera = static_cast<HardwareCamera*>(context);
   camera->on_image_available(reader);
}

void HardwareCamera::onSessionActive(void *context, ACameraCaptureSession *session)
{
   HardwareCamera* camera = static_cast<HardwareCamera*>(context);
   camera->on_session_active(session);
}

void HardwareCamera::onSessionReady(void *context, ACameraCaptureSession *session)
{
   HardwareCamera* camera = static_cast<HardwareCamera*>(context);
   camera->on_session_ready(session);
}

void HardwareCamera::onSessionClosed(void *context, ACameraCaptureSession *session)
{
   HardwareCamera* camera = static_cast<HardwareCamera*>(context);
   camera->on_session_closed(session);
}


