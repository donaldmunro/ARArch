#ifndef _MAR_STRUCTURES_H
#define _MAR_STRUCTURES_H

#include <cstdio>
#include <cstdint>
#include <memory>
#include <array>

#include "mar/util/util.hh"

namespace toMAR
{
   struct CameraFrameData
   {
      unsigned long cameraId = std::numeric_limits<unsigned long>::max();
      unsigned long camera2Id = std::numeric_limits<unsigned long>::max(); //only for stereo
      uint64_t seqno_1 = 0, seqno_2 = 0;
   };

   template <std::size_t N>
   struct CameraFrameDef
   //===================
   {
      std::array<CameraFrameData, N> cameras;

      std::size_t n() { return N; }

      int count;

      CameraFrameDef() { clear(); }

      void clear()
      //----------
      {
         for (size_t i=0; i<N; i++)
            cameras[i] = std::move(CameraFrameData
                                   { .cameraId = std::numeric_limits<unsigned long>::max(),
                                     .camera2Id = std::numeric_limits<unsigned long>::max(),
                                     .seqno_1 = 0, .seqno_2 = 0 });
         count = 0;
      }

      void set(size_t i, unsigned long cameraId, uint64_t seq1 =0)
      //--------------------------------------------------------
      {
 #if !defined(NDEBUG)
         if (i>=N)
            __android_log_assert("i>=N", "Structures::CameraFrameDef", "%zu<%zu", i, N);
 #endif
         cameras[i] = std::move(CameraFrameData
         { .cameraId = cameraId, .camera2Id = std::numeric_limits<unsigned long>::max(),
           .seqno_1 = seq1, .seqno_2 = 0 });
         count++;
      }

      void set_stereo(size_t i, unsigned long camera1Id, unsigned long camera2Id, uint64_t seq1 =0,
                      uint64_t seq2 =0, bool isClear =false)
      //------------------------------------------------------------------------------------------
      {
#if !defined(NDEBUG)
         if (i>=N)
            __android_log_assert("i>=N", "Structures::CameraFrameDef", "%zu<%zu", i, N);
#endif
         if (isClear) clear();
         cameras[i] = std::move(CameraFrameData{ .cameraId = camera1Id, .camera2Id = camera2Id,
                                                 .seqno_1 = seq1,
                                                 .seqno_2 = ( (seq2 == 0) && (seq1 != 0) ) ? seq1 : seq2});
         count++;
      }
   };

   using CameraFrame = CameraFrameDef<4>;

   template<class T>
   struct DetectRect
   {
      T top, left;
      T bottom, right;
      T w, h;

      DetectRect() : top(-1), left(-1), bottom(-1), right(-1), w(0), h(0) {}

      DetectRect(T top, T left, T bottom, T right) : top(top), left(left), bottom(bottom), right(right)
      {
         w = abs((right - left));
         h = abs((bottom - top));
      }
      T abs(const T& v) { return v < 0 ? -v : v; }
      T width() { return w; }
      T height() { return h; }
      T area() { return w*h; }
   };

   struct DetectedBoundingBox
      //------------------------
   {
      DetectRect<double> BB;
      const unsigned long cameraId;
      uint64_t seqno;
      int64_t timestamp;

      DetectedBoundingBox() : BB(), cameraId(std::numeric_limits<unsigned long>::max()),
         seqno(0), timestamp(-1) {}

      DetectedBoundingBox(uint64_t seq, const unsigned long camera, double top, double left,
         double bottom, double right) : BB(top, left, bottom, right), cameraId(camera), seqno(seq),
         timestamp(toMAR::util::now_monotonic())
      {}
   };

   class DetectedBoundingBoxCompare
   {
   public:
      bool operator()(const DetectedBoundingBox* l,
                      const DetectedBoundingBox* r) const { return l->seqno > r->seqno; }
   };

   struct DetectedROI
   //================
   {
      void* image;
      DetectRect<double> BB;
      const unsigned long cameraId;
      uint64_t seqno;
      int64_t timestamp =-1;
      bool inUse = false;

      DetectedROI() : image(nullptr), BB(), cameraId(std::numeric_limits<unsigned long>::max()), seqno(0), timestamp(-1) {}

      DetectedROI(uint64_t seq, const unsigned long camera, void* img, double top, double left,
                  double bottom, double right) : image(img), BB(top, left, bottom, right),
                  cameraId(camera), seqno(seq), timestamp(toMAR::util::now_monotonic()),inUse(false)
      {}

#if !defined(NDEBUG)
      ~DetectedROI()
      //------------
      {
         if (image) //Delete in renderer
            __android_log_print(ANDROID_LOG_ERROR, "~DetectedROI",
                                "Disposing DetectedROI with dangling image");
      }

#endif
   };

   class DetectedROICompare
   {
   public:
      bool operator()(const DetectedROI* l, const DetectedROI* r) const { return l->seqno > r->seqno; }
   };

    struct UInt64DescComparator
    {
    public:
        bool operator()(const uint64_t& l, const uint64_t& r) const { return l < r; }
    };

   template <class T1, class T2>
    struct pair_hasher
    {
       std::size_t operator() (const std::pair<T1, T2> &pair) const
       {
          return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
       }
    };
};
#endif
