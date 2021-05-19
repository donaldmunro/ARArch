#ifndef _MAR_CV_H
#define _MAR_CV_H

#include <vector>

#include "mar/Structures.h"

namespace toMAR
{
   namespace vision
   {
      bool YUV2RGBA(void* YUV, void* RGBA, int w, int h, bool isRGBA, const char *logtag);
      bool YUV2Mono(void* YUV, void* mono, int w, int h, const char *logtag);
      bool NV2RGBA(void* Y, void* U, void *V, int w, int h, bool isRGBA, void* outputJavaRGB,
                   void* outputJavaGrey, const char *logtag);
      bool overlay(void *src, int width, int height, void* dst, int w, int h, const char *logtag);
      bool drawBB(void* img, int width, int height, double top, double left, double bottom, double right,
                  int r, int g, int b, int stroke, const char *logtag);
      bool init_faces(void* params);
      bool find_face(void *src, int width, int height, int minArea,  DetectRect<int>& faces,
                     const char *logtag);
      void dump(unsigned long cid, uint64_t seqno, const char* nid, int w, int h,
                void *framedata);
   }
};

#endif
