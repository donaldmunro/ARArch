#ifndef _FRAMEINFO_H
#define _FRAMEINFO_H

#include <memory>
#include <cstdint>
#include <atomic>
#include <string>

#include <inttypes.h>
#include <android/log.h>
#include <mar/util/util.hh>
#include <mar/util/Countable.hh>

#include "mar/jniint.h"

namespace toMAR
{
   class Repository;

   enum class ColorFormats : unsigned { RGBA = 0, BGRA = 1 };

   struct FrameInfo : public Countable<FrameInfo>
   //============================================
   {
      unsigned long camera_id;
      uint64_t seqno;
      int64_t timestamp, javaTimestamp;
      int width = 0, height = 0;
      ColorFormats colorFormat;
      int rgbaLen =0, monoLen =0;
      jbyteArray rgba, mono;
      JavaVM* vm;
      std::atomic_int rgbaAcquires{0}, monoAcquires{0};
      std::atomic_bool isDetecting{false}, isTracking{false}, isRendering{false};

      FrameInfo(unsigned long cameraId, int64_t ts, int w, int h, ColorFormats format, JavaVM* vm,
            int rgbaLen, jbyteArray rgbaData) : camera_id(cameraId), seqno(0),
            timestamp(util::now_monotonic()), javaTimestamp(ts),
            width(w), height(h), colorFormat(format), rgbaLen(rgbaLen), monoLen(0),
            rgba(rgbaData), mono(nullptr), vm(vm) {}

      FrameInfo(unsigned long cameraId, int64_t ts, int w, int h, ColorFormats format,  JavaVM* vm,
                int rgbaLen, jbyteArray rgbaData, int monoLen, jbyteArray monoData) :
                camera_id(cameraId), seqno(0), timestamp(util::now_monotonic()), javaTimestamp(ts),
                width(w), height(h), colorFormat(format),
                rgbaLen(rgbaLen), monoLen(monoLen), rgba(rgbaData), mono(monoData), vm(vm) {}

      FrameInfo(const FrameInfo& other) = delete;
      FrameInfo& operator=(FrameInfo const&) = delete;

      ~FrameInfo() { dispose(); }

      unsigned char* getColorData(void*& context);
      void releaseColorData(void *context, unsigned char* p);
      unsigned char* getMonoData(void*& context);
      void releaseMonoData(void *context, unsigned char* p);
      bool getEnv(JNIEnv*& env);
      void dispose();
   };
};

#endif //MAR_FRAMEINFO_H
