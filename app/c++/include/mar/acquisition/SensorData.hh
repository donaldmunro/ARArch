#ifndef MAR_SENSORDATA_HH
#define MAR_SENSORDATA_HH

#include <mutex>

#include <android/sensor.h>

#include "mar/util/bounded_map.h"

namespace toMAR
{
   class SensorData
   {
   public:
      SensorData(unsigned long max_queue_size, std::mutex& mtx) : shared_queue(max_queue_size, 0), mutex(mtx) {}

      void push(ASensorEvent & ev)
      //------------------------
      {
         std::lock_guard<std::mutex> lock(mutex);
         ASensorEvent * event = new ASensorEvent(ev);
         shared_queue.insert(std::make_pair(ev.timestamp, event));
      }

      unsigned long between(const __int64_t start, const __int64_t end, std::multimap<int64_t, ASensorEvent*> &events)
      //----------------------------------------------------------------------------------------
      {
         std::lock_guard<std::mutex> lock(mutex);
         return shared_queue.between(start, end, events);
      }

      unsigned long all(std::multimap<int64_t, ASensorEvent*> &events)
      //----------------------------------------------------
      {
         std::lock_guard<std::mutex> lock(mutex);
         return shared_queue.all(events);
      }

      unsigned long from(const __int64_t start, std::multimap<int64_t, ASensorEvent*> &events)
      //----------------------------------------------------------------------------
      {
         std::lock_guard<std::mutex> lock(mutex);
         return shared_queue.from(start, events);
      }

      bool supported() { return is_supported; }

   private:
      struct ASensor *sensor;
      struct ASensorEventQueue *sensor_queue;
      bounded_map<__int64_t, ASensorEvent *> shared_queue;
      std::mutex& mutex;
      bool is_supported = false, good = false;


      friend class Sensors;
   };
};
#endif //MAR_SENSORDATA_HH
