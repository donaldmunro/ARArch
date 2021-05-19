#ifndef _MAR_UTIL_HH
#define _MAR_UTIL_HH

#include <cstdint>
#include <time.h>

namespace toMAR
{
   namespace util
   {
      int64_t now_monotonic();
      int64_t now_realtime();
      int64_t now_boot();
   }
};

#endif

