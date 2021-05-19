#include <cstdint>
#include <time.h>

namespace toMAR
{
   namespace util
   {
      int64_t now_monotonic()
      //---------------------
      {
         struct timespec tme;
         clock_gettime(CLOCK_MONOTONIC, &tme);
         return tme.tv_sec * 1000000000L + tme.tv_nsec;
      }

      int64_t now_realtime()
      //---------------------
      {
         struct timespec tme;
//         tme.tv_sec = tme.tv_nsec = 0;
         clock_gettime(CLOCK_REALTIME, &tme);
         return tme.tv_sec * 1000000000L + tme.tv_nsec;
      }

      int64_t now_boot()
      //---------------------
      {
         struct timespec tme;
         clock_gettime(CLOCK_BOOTTIME, &tme);
         return tme.tv_sec * 1000000000L + tme.tv_nsec;
      }
   }
}
