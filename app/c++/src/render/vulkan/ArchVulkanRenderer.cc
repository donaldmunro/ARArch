#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "mar/render/ArchVulkanRenderer.h"

#include <mar/architecture/tbb/TBBRender.h>

//#define TAKE_PICTURES 1

namespace toMAR
{
   tbb::concurrent_unordered_map<int, ArchVulkanRendererState*> ArchVulkanRenderer::states;

   ArchVulkanRenderer::ArchVulkanRenderer(int id, const char *appName, const char *assetsDir,
                                          bool isAprilTags, FaceRenderType faceRenderType,
                                          bool isShowFps):
         VulkanRenderer(appName, assetsDir, isShowFps), id(id)
   //---------------------------------------------------------------------
   {
      states[id] = new ArchVulkanRendererState(isAprilTags, 0, faceRenderType, nullptr, 0,
                                               nullptr, 0);
   }

   void ArchVulkanRenderer::draw(FrameInfo *frame, void *texture)
   //-----------------------------------------------------------
   {
#ifdef HAS_APRILTAGS
      tbb::concurrent_hash_map<uint64_t, std::vector<DetectedBoundingBox *>> &targets = repository->aprilTags;
      tbb::concurrent_hash_map<uint64_t, std::vector<DetectedBoundingBox *>>::accessor it;
      ArchVulkanRendererState* state = ArchVulkanRenderer::states[id];
      const int64_t now = toMAR::util::now_monotonic();
#ifdef TAKE_PICTURES
      int tags =0, faceDetects = 0;
#endif
      if (state->isAprilTags)
      {
         const int64_t lastTime = now - state->lastAprilTagTime;
         if (lastTime > 280000000)
         {
            delete_last_tags();
            uint64_t lastSeq;
            if (repository->recentAprilTags.try_pop(lastSeq))
            {
               if (targets.find(it, lastSeq))
               {
                  std::vector<DetectedBoundingBox *> &L = it->second;
                  if ((!L.empty()) && (L[0]) && (now - L[0]->timestamp) <= 400000000)
                  {
                     draw_bounding_boxes(frame, texture, L);
                     state->lastAprilTagTime = L[0]->timestamp;
                     state->lastAprilTags = std::move(L);
                     targets.erase(it);
#ifdef TAKE_PICTURES
                     tags++;
#endif
                  }
                  else
                  {
                     for (DetectedBoundingBox *pBB : L) if (pBB) delete pBB;
                     L.clear();
                  }
               }
            }
         }
         else
         {
            draw_bounding_boxes(frame, texture, state->lastAprilTags);
#ifdef TAKE_PICTURES
            tags += state->lastAprilTags.size();
#endif
         }
      }

//      uint64_t seq = frame->seqno;
//      uint64_t lastSeq = seq - 5;
//      for (; seq>lastSeq; seq--)
//      {
//          if (targets.find(it, seq))
//          {
//              std::vector<DetectedBoundingBox*>& L = it->second;
//              for (DetectedBoundingBox* target : L)
//              {
//                 DetectRect<double>& rect = target->BB;
//                 toMAR::vision::drawBB(texture, frame->width, frame->height, rect.top, rect.left,
//                                       rect.bottom, rect.right, 255, 0, 0, 6,  "ArchVulkanRenderer::draw()");
//              }
//              //targets.erase(it);
//              //repository->recentAprilTags.try_push(L);
//              break;
//          }
//      }

#endif
#ifdef HAS_FACE_DETECTION
      if (state->faceRenderType == FaceRenderType::BB) // Front camera only
      {
         const int64_t now = toMAR::util::now_monotonic();
         const uint64_t seqno = frame->seqno;
         const int64_t lastTime = now - state->lastFaceTime;
         if (lastTime > 280000000)
         {
            if (state->lastFace != nullptr)
            {
               delete state->lastFace;
               state->lastFace = nullptr;
            }
            uint64_t lastSeq;
            if (repository->recentFaceDetections.try_pop(lastSeq))
            {
               tbb::concurrent_hash_map<uint64_t, DetectedBoundingBox*>::accessor it;
               if (repository->faceDetections.find(it, lastSeq))
               {
                  DetectedBoundingBox* bb = it->second;
                  if (bb)
                  {
                     DetectRect<double> &rect = bb->BB;
                     toMAR::vision::drawBB(texture, frame->width, frame->height, rect.top, rect.left,
                                           rect.bottom, rect.right, 255, 0, 0, 6,
                                           "ArchVulkanRenderer::draw()");
                     state->lastFace = bb;
                     state->lastFaceTime = bb->timestamp;
                     repository->faceDetections.erase(it);
                  }
               }
            }
         }
         else if (state->lastFace)
         {
            DetectRect<double>& rect = state->lastFace->BB;
            toMAR::vision::drawBB(texture, frame->width, frame->height, rect.top, rect.left,
                                  rect.bottom, rect.right, 255, 0, 0, 6,
                                  "ArchVulkanRenderer::draw()");
         }
      }
      else if (state->faceRenderType == FaceRenderType::OVERLAY) // Rear and front camera active
      {
         const int64_t now = toMAR::util::now_monotonic();
//         const int64_t lastTime = now - state->lastOverlayTime;

         uint64_t lastSeq = repository->lastFaceOverlay.load();
         if (state->lastSeq != lastSeq)
         {
            // __android_log_print(ANDROID_LOG_INFO, "ArchVulkanRenderer::draw()", "New face overlay %lu", lastSeq);
            state->lastSeq = lastSeq;
            tbb::concurrent_hash_map<uint64_t, DetectedROI*>::accessor it;
            if (repository->faceOverlays.find(it, lastSeq))
            {
               DetectedROI* face = it->second;
               if ( (face) && (face->image) )
               {
                  // __android_log_print(ANDROID_LOG_INFO, "ArchVulkanRenderer::draw()", "Found face overlay %lu %p", lastSeq, face->image);
                  face->inUse = true;
                  cv::Mat *faceOverlay = static_cast<cv::Mat *>(face->image);
//                     dump_tex(frame->camera_id, frame->seqno, "N", faceOverlay->cols, faceOverlay->rows, faceOverlay->data);
                  toMAR::vision::overlay(texture, frame->width, frame->height, faceOverlay->data,
                                         faceOverlay->cols, faceOverlay->rows, "ArchVulkanRenderer::draw()");
#ifdef TAKE_PICTURES
                  faceDetects++;
#endif
                  if (state->lastOverlay != nullptr)
                  {
                     // __android_log_print(ANDROID_LOG_INFO, "FaceOverlayTBBDetector::()", "Delete face overlay %lu", state->lastOverlay->seqno);
                     repository->faceOverlays.erase(state->lastOverlay->seqno);
                     if (state->lastOverlay->image != nullptr)
                     {
                        cv::Mat* pm = static_cast<cv::Mat*>(state->lastOverlay->image);
                        delete pm;
                     }
                     state->lastOverlay->image = nullptr;
                     delete state->lastOverlay;
                     state->lastOverlay = nullptr;
                  }
                  state->lastOverlay = face;
                  state->lastOverlayTime = face->timestamp;
                  repository->currentRenderOverlay.store(lastSeq);
                  repository->faceOverlays.erase(it);
               }
               else
                  if (face) delete face;
            }
            else
               __android_log_print(ANDROID_LOG_WARN, "ArchVulkanRenderer::draw()", "Could not find face overlay %lu", lastSeq);
         }
         else if (state->lastOverlay)
         {
            cv::Mat *faceOverlay = static_cast<cv::Mat *>(state->lastOverlay->image);
            // __android_log_print(ANDROID_LOG_INFO, "ArchVulkanRenderer::draw()", "Reusing face overlay %lu %p", state->lastOverlay->seqno, faceOverlay);
            toMAR::vision::overlay(texture, frame->width, frame->height, faceOverlay->data,
                                   faceOverlay->cols, faceOverlay->rows, "ArchVulkanRenderer::draw()");
#ifdef TAKE_PICTURES
            faceDetects++;
#endif
         }
      }
#endif
#ifdef TAKE_PICTURES
      __android_log_print(ANDROID_LOG_INFO, "ArchVulkanRenderer::draw()", "TAKE_PICTURE %d %d", tags, faceDetects);
      if ( (tags > 0) && (faceDetects > 0) )
         toMAR::vision::dump(frame->camera_id, frame->seqno, "N", frame->width, frame->height, texture);
#endif
//         cv::Mat m(h, w, CV_8UC4, framedata), mm;
//         char nname[512];
//         cv::cvtColor(m, mm, cv::COLOR_RGBA2BGR);
//         sprintf(nname, "/sdcard/Pictures/%lu-%08lu.png", frame->camera_id, frame->seqno);
//         cv::imwrite(nname, mm);
   }

   ArchVulkanRenderer::~ArchVulkanRenderer()
   //-----------------------------------------------
   {
      auto it = states.find(id);
      if (it != states.end())
      {
         if (it->second)
            delete it->second;
      }
   }
}
