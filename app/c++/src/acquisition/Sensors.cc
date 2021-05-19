#include <sstream>

#include <android/log.h>

#include "mar/acquisition/Sensors.h"
#include "mar/Repository.h"
#include "mar/acquisition/SensorData.hh"

namespace toMAR
{
   std::unordered_map<int, std::pair<std::string, int>> Sensors::SUPPORTED_SENSORS =
   {
      { ASENSOR_TYPE_GRAVITY, std::pair<std::string, int>("Gravity", 3) },
      { ASENSOR_TYPE_LINEAR_ACCELERATION, std::pair<std::string, int>("Linear Accelerometer", 3) },
      { ASENSOR_TYPE_ACCELEROMETER, std::pair<std::string, int>("Accelerometer", 3) },
      { ASENSOR_TYPE_GYROSCOPE, std::pair<std::string, int>("Gyroscope", 3) },
      { ASENSOR_TYPE_MAGNETIC_FIELD, std::pair<std::string, int>("Magnetic", 3) },
      { ASENSOR_TYPE_ROTATION_VECTOR, std::pair<std::string, int>("Rotation Vector", 5) },
      { ASENSOR_TYPE_GAME_ROTATION_VECTOR, std::pair<std::string, int>("Game Rotation Vector", 5) },
      { ASENSOR_TYPE_ACCELEROMETER_UNCALIBRATED, std::pair<std::string, int>("Uncalibrated Accelerometer", 6) },
      { ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED, std::pair<std::string, int>("Uncalibrated Gyroscope", 6) },
      { ASENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED, std::pair<std::string, int>("Uncalibrated Magnetic", 6) },
   };

   const int LOOPER_ID_USER = 3;

   bool Sensors::add_sensor(int sensor_id, int queuedMax, std::stringstream* errs)
   //-------------------------------------------------
   {
      ASensorManager *sensor_manager = getSensorManager(package_name.c_str());
      if (sensor_manager == nullptr)
      {
         if (errs)
            *errs << "Sensors::add_sensor:: Error getting Android Sensor Manager using main package name '" << package_name << "'";
         return false;
      }
      std::string sensor_name;
      auto supportedIter = SUPPORTED_SENSORS.find(sensor_id);
      bool isSupported = (supportedIter != SUPPORTED_SENSORS.end());
      if (isSupported)
         sensor_name = supportedIter->first;
      else
         sensor_name = "Unknown/Unsupported";

      ASensor *sensor = const_cast<ASensor *>(ASensorManager_getDefaultSensor(sensor_manager, sensor_id));
      if (sensor == nullptr)
      {
         if (errs)
            *errs << "Sensors::add_sensor: Error obtaining " << sensor_name << " sensor.";
         return false;
      }
      {
         tbb::spin_mutex::scoped_lock _lock(sensorLock);
         auto it = sensors.find(sensor_id);
         if (it == sensors.end())
         {
            SensorData* sensor_info = new SensorData(queuedMax, mutex);
            sensors[sensor_id] = sensor_info;
            return true;
         }
      }
      return false;
   }

   size_t Sensors::initialize(std::stringstream* errs)
   //-----------------------------------------------
   {
      if (sensors.size() == 0)
      {
         *errs << "Sensors::initialize - No sensors to initialize";
         return 0;
      }
      ASensorManager *sensor_manager = getSensorManager(package_name.c_str());
      if (sensor_manager == nullptr)
      {
         if (errs)
            *errs << "Error getting Android Sensor Manager using main package name '" << package_name << "'";
         return 0;
      }
      if (looper == nullptr)
         looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
      if (looper == nullptr)
      {
         if (errs)
            *errs << "Error getting Android thread looper.";
         return 0;
      }
      size_t n = 0;
      for (auto it = sensors.begin(); it != sensors.end(); ++it)
      {
         int sensor_id = it->first;
         SensorData* sensor_info = it->second;
         auto supported_it = SUPPORTED_SENSORS.find(sensor_id);
         sensor_info->is_supported = (supported_it != SUPPORTED_SENSORS.end());
         std::string sensor_name;
         if (sensor_info->is_supported)
            sensor_name = supported_it->second.first;
         sensor_info->sensor = const_cast<ASensor *>(ASensorManager_getDefaultSensor(sensor_manager, sensor_id));
         if (sensor_info->sensor == nullptr)
         {
            if (errs)
               *errs << "Error obtaining " << sensor_name << " sensor.";
            sensor_info->good = false;
            continue;
         }
         sensor_info->sensor_queue = ASensorManager_createEventQueue(sensor_manager, looper, LOOPER_ID_USER, nullptr, nullptr);
         if (sensor_info->sensor_queue == nullptr)
         {
            if (errs)
               *errs << "Error obtaining " << sensor_name << " events queue";
            sensor_info->good = false;
            continue;
         }
         int bestrate = ASensor_getMinDelay(sensor_info->sensor);
         if (bestrate <= 0)
         {
            if (errs)
               *errs << "Error obtaining best sampling rate for " << sensor_name << " sensor.";
            bestrate = 4000;
         }
         int sensor_status = ASensorEventQueue_enableSensor(sensor_info->sensor_queue, sensor_info->sensor);
         if (sensor_status < 0)
         {
            if (errs)
               *errs << "Error enabling " << sensor_name << " sensor";
            ASensorManager_destroyEventQueue(sensor_manager, sensor_info->sensor_queue);
            sensor_info->good = false;
            continue;
         }
         sensor_status = ASensorEventQueue_setEventRate(sensor_info->sensor_queue, sensor_info->sensor, bestrate);
         if (sensor_status < 0)
         {
            if (errs)
               *errs << "Error setting rate to " << bestrate << " for " << sensor_name << " sensor";
            //ASensorEventQueue_disableSensor(sensor_info->sensor_queue, sensor_info->sensor);
            //ASensorManager_destroyEventQueue(sensor_manager, queue);
         }
         n++;
      }
      return n;
   }

   int Sensors::process_queues(int timeout_ms, int max_to_process)
   //-----------------------------------------------------------
   {
      int poll = ALooper_pollAll(timeout_ms, nullptr, nullptr, nullptr);
      if ( (poll == ALOOPER_POLL_TIMEOUT) || (poll == ALOOPER_POLL_ERROR) )
         return 0;
      int count = 0;
      for(auto it = sensors.begin(); it != sensors.end(); ++it)
      {
         SensorData* sensor_info = it->second;
         ASensorEventQueue *queue = sensor_info->sensor_queue;
         if (ASensorEventQueue_hasEvents(queue) > 0)
         {
            ASensorEvent event;
            count = 0;
            while (ASensorEventQueue_getEvents(queue, &event, 1) > 0)
            {
               sensor_info->push(event);
               if (count++ > max_to_process) break;
            }
         }
      }
      return count;
   }

   //Despite linking with libandroid and all other sensor APIs linking OK, ASensorManager_getInstanceForPackage doesn't
   ASensorManager *Sensors::getSensorManager(const char *packageName)
   //------------------------------------------------------
   {
      typedef ASensorManager *(*PF_GETINSTANCEFORPACKAGE)(const char *name);
      void *androidHandle = dlopen("libandroid.so", RTLD_NOW);
      PF_GETINSTANCEFORPACKAGE getInstanceForPackageFunc = (PF_GETINSTANCEFORPACKAGE) dlsym(androidHandle,
                                                                                            "ASensorManager_getInstanceForPackage");
      if (getInstanceForPackageFunc)
         return getInstanceForPackageFunc(packageName);

      typedef ASensorManager *(*PF_GETINSTANCE)();
      PF_GETINSTANCE getInstanceFunc = (PF_GETINSTANCE) dlsym(androidHandle, "ASensorManager_getInstance");
      if (getInstanceFunc == nullptr) return nullptr;
      return getInstanceFunc();
   }

   size_t Sensors::get_all_between(int64_t start, int64_t end, std::multimap<int64_t, ASensorEvent*>& events)
   //------------------------------------------------------------------------------------------------------------------
   {
      size_t no = 0;
      for(auto it = sensors.begin(); it != sensors.end(); ++it)
      {
         int sensorid = it->first;
         SensorData* sensor_info = it->second;
         no += sensor_info->between(start, end, events);
      }
      return no;
   }

   size_t Sensors::get_between(int sensorid, int64_t start, int64_t end, std::multimap<int64_t, ASensorEvent*>& events)
   //------------------------------------------------------------------------------------------------------
   {
      auto it = sensors.find(sensorid);
      if (it == sensors.end())
         return 0;
      SensorData* sensor_info = it->second;
      return sensor_info->between(start, end, events);
   }

   size_t Sensors::get_all(int sensorid, std::multimap<int64_t, ASensorEvent*>& events)
   //------------------------------------------------------------------------
   {
      auto it = sensors.find(sensorid);
      if (it == sensors.end())
         return 0;
      SensorData* sensor_info = it->second;
      return sensor_info->all(events);
   }

   size_t Sensors::get_all(std::multimap<int64_t, ASensorEvent*>& events)
   //--------------------------------------------------------------------
   {
      size_t no = 0;
      for(auto it = sensors.begin(); it != sensors.end(); ++it)
      {
         int sensorid = it->first;
         SensorData* sensor_info = it->second;
         no += sensor_info->all(events);

      }
      return no;
   }

   size_t Sensors::get_from(int sensorid, int64_t start, std::multimap<int64_t, ASensorEvent *> &events)
   //----------------------------------------------------------------------------------------------------
   {
      auto it = sensors.find(sensorid);
      if (it == sensors.end())
         return 0;
      SensorData* sensor_info = it->second;
      return sensor_info->from(start, events);
   }

   size_t Sensors::get_all_from(int64_t start, std::multimap<int64_t, ASensorEvent *> &events)
   //-----------------------------------------------------------------------------------------
   {
      size_t no = 0;
      for(auto it = sensors.begin(); it != sensors.end(); ++it)
      {
         int sensorid = it->first;
         SensorData* sensor_info = it->second;
         no += sensor_info->from(start, events);

      }
      return no;

   }

   void Sensors::sensor_handler_thread(int timeout_ms, int max_to_process, bool &stop)
   //--------------------------------------------------------------------------------
   {
//      looper = ALooper_forThread();
      std::stringstream errs;
      size_t noSensors = sensors.size();
      size_t inited = initialize(&errs);
      if (inited == 0)
      {
         __android_log_print(ANDROID_LOG_ERROR, "Sensors::sensor_handler_thread",
                             "No sensors initialized correctly (%s %ld/%ld)", errs.str().c_str(), inited, noSensors);

         return;
      }
      while (! stop)
         process_queues(timeout_ms, max_to_process);
   }
}
