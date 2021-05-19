#ifndef _SENSORS_H
#define _SENSORS_H

#include <cstdio>
#include <memory>
#include <unordered_map>
#include <mutex>

#include <android/sensor.h>
#include <dlfcn.h>

#include "tbb/spin_mutex.h"

#include "mar/acquisition/SensorData.hh"

namespace toMAR
{
   class Sensors
   //===========
   {
   public:
      static Sensors& instance()
      //--------------------------------
      {
         static Sensors the_instance;
         return the_instance;
      }
      Sensors(Sensors const&) = delete;
      Sensors(Sensors&&) = delete;
      Sensors& operator=(Sensors const&) = delete;
      Sensors& operator=(Sensors &&) = delete;

      bool add_sensor(int sensor, int queuedMax, std::stringstream* errs);
      size_t size() { return sensors.size(); }
      size_t initialize(std::stringstream* errs =nullptr);
      size_t get_all(int sensorid, std::multimap<int64_t, ASensorEvent*>& events);
      size_t get_all(std::multimap<int64_t, ASensorEvent*> &events);
      size_t get_all_between(int64_t start, int64_t end, std::multimap<int64_t, ASensorEvent*>& events);
      size_t get_between(int sensorid, int64_t start, int64_t end, std::multimap<int64_t, ASensorEvent*>& events);
      size_t get_from(int sensorid, int64_t start, std::multimap<int64_t, ASensorEvent*>& events);
      size_t get_all_from(int64_t start, std::multimap<int64_t, ASensorEvent*>& events);

      void sensor_handler_thread(int timeout_ms, int max_to_process, bool &stop);

      int process_queues(int timeout_ms, int max_to_process);

      std::string package_name;
   private:
      Sensors() {}
      ASensorManager* getSensorManager(const char* packageName);
      tbb::spin_mutex sensorLock;
      std::unordered_map<int, SensorData*> sensors;
      ALooper *looper;
      std::mutex mutex;

      static std::unordered_map<int, std::pair<std::string, int>> SUPPORTED_SENSORS;
   };
}
#endif
