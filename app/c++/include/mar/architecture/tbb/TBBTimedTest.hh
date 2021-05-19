#ifndef _TBBTIMEDTEST_H
#define _TBBTIMEDTEST_H

#include <chrono>
#include <random>
#include <atomic>

#include "mar/acquisition/FrameInfo.h"

namespace toMAR
{
   class TBBTimedTest
   //================
   {
   public:
      virtual bool spin(std::atomic_bool &ab, FrameInfo *frame, double poissonRate)
      //------------------------------------------------------
      {
         void* env;
         unsigned char* data = frame->getColorData(env);
         if (data == nullptr)
            return true;
         bool b = false;
         if (ab.compare_exchange_strong(b, true))
         {
            std::chrono::high_resolution_clock::time_point detectStart = std::chrono::high_resolution_clock::now();
            std::random_device rd;
            std::mt19937 generator(rd());
            std::poisson_distribution<long> poissonDistribution{poissonRate};
            long intervalMax = poissonDistribution(generator);
            long long elapsed;
            std::uniform_int_distribution<unsigned> randomWidth(0, static_cast<unsigned int>(frame->width)),
                                                    randomHeight(0, static_cast<unsigned int>(frame->height));
            do
            {
               unsigned w = randomWidth(generator), h = randomHeight(generator);
               size_t offset = w*h*4;
               unsigned char R = data[offset], G = data[offset+1], B = data[offset+2];
               const std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
               elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - detectStart).count();
            }
            while (elapsed < intervalMax);
            frame->releaseColorData(env, data);
            ab.store(false);
            return true;
         }
         frame->releaseColorData(env, data);
         return false;
      }

   };
}
#endif
