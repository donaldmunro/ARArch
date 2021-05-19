#ifndef _ARCH_VULKAN_RENDERER_H
#define _ARCH_VULKAN_RENDERER_H

#include "mar/render/VulkanRenderer.h"
#include "mar/acquisition/FrameInfo.h"
#include <mar/util/cv.h>

namespace toMAR
{
   enum class FaceRenderType : unsigned { NONE = 0, BB = 1, OVERLAY = 2 };

   struct ArchVulkanRendererState
   {
      bool isAprilTags;
      int64_t lastAprilTagTime;
      FaceRenderType faceRenderType;
      std::vector<DetectedBoundingBox *> lastAprilTags;
      DetectedBoundingBox* lastFace;
      int64_t lastFaceTime;
      DetectedROI* lastOverlay;
      int64_t lastOverlayTime;
      uint64_t lastSeq;

      ArchVulkanRendererState(bool isAprilTags, int64_t lastAprilTagTime,
                              FaceRenderType faceRenderType,
                              DetectedBoundingBox *lastFace, int64_t lastFaceTime,
                              DetectedROI *lastOverlay, int64_t lastOverlayTime) :
         isAprilTags(isAprilTags), lastAprilTagTime(lastAprilTagTime), faceRenderType(faceRenderType),
         lastFace(lastFace), lastFaceTime(lastFaceTime),
         lastOverlay(lastOverlay), lastOverlayTime(lastOverlayTime), lastSeq(0)
      {}
   };

   class ArchVulkanRenderer : public VulkanRenderer
      //===============================================
   {
   public:
      ArchVulkanRenderer(int id, const char *appName, const char *assetsDir, bool isAprilTags = false,
                         FaceRenderType faceRenderType = FaceRenderType::NONE,
                         bool isShowFps = false);
      ~ArchVulkanRenderer();

   protected:
      void draw(FrameInfo *frame, void *texture) override;

   private:
      int id;
      static tbb::concurrent_unordered_map<int, ArchVulkanRendererState*> states;


      inline void draw_bounding_boxes(FrameInfo *frame, void *texture,
                                      std::vector<DetectedBoundingBox *> &L)
      //--------------------------------------------------------------------
      {
         for (DetectedBoundingBox *target : L)
         {
            DetectRect<double> &rect = target->BB;
            toMAR::vision::drawBB(texture, frame->width, frame->height, rect.top, rect.left,
                                  rect.bottom, rect.right, 255, 0, 0, 6,
                                  "VulkanRenderer::draw()");
         }
      }

      inline void delete_last_tags()
      //---------------------------
      {
         ArchVulkanRendererState* state = ArchVulkanRenderer::states[id];
         if (! state->lastAprilTags.empty())
         {
            for (DetectedBoundingBox *pBB : state->lastAprilTags) if (pBB) delete pBB;
            state->lastAprilTags.clear();
         }
         state->lastAprilTagTime = 0;
      }
   };
}
#endif
